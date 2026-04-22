# Virtual Textures & Texture Streaming — research notes for ALZE Engine

Fecha: 2026-04-22. Round 3 / agente 7.
Target: ALZE Engine (`/root/repos/alze-engine`, C++17, SDL2 + OpenGL 3.3 hoy, sin RTTI/exceptions, equipo pequeño).
Alcance: Megatexture (id Tech 5, Rage 2011) → Decima VT (Guerrilla) → UE5 Runtime/Streaming VT → sparse textures API (Vulkan / DX12 / GL 4.4) → Granite (Arntzen) → KTX2/Basis.
No overlap: `aaa_engines.md` menciona MegaTexture de pasada ("Rage streaming pop-in"); `rage_rockstar.md` describe RPF streaming pero NO toca virtual textures (GTA/RDR2 nunca usaron VT, solo mip-streaming clásico). Este documento entra al algoritmo de VT: feedback buffer, indirection, tile pool.

## 1. MegaTexture / id Tech 5 — la idea original

**John Carmack, id Software, 2004-2011.** El concepto arrancó en Enemy Territory: Quake Wars (Splash Damage/id Tech 4, 2007) como "terrain megatexture" — una única textura gigante (32k×32k inicialmente) pintada en world-space sobre el terreno. Luego evolucionó para cubrir **toda superficie** de un nivel en Rage (2011, id Tech 5).

**Motivación.** Evitar la monotonía de terrenos tileados: con texturas únicas por metro cuadrado, cada rincón del mundo es arte único ("unique texturing"). Permite a los artistas pintar en 3D world-space (a la Mudbox / Mari) en vez de tejer shaders de splatmap.

**Diseño de id Tech 5:**
- **Virtual atlas** de ~128k × 128k texels por nivel (Rage shippeó con ~24 GB de texturas únicas a disco tras compresión).
- **Páginas de 128×128 texels** con borde de 4 texels para anisotropic filtering = tile físico 136×136. Compresión DXT5 → 128 × 128 × 1 byte = 16 KB por tile BC.
- **Physical page pool** en GPU: ~64 MB en 360/PS3, ~128-256 MB en PC. Contiene los tiles actualmente visibles.
- **Indirection texture**: un look-up 2D que mapea coord virtual (u,v) ∈ [0,1] → coord física en el pool. El shader sampler hace el two-step: muestrea indirection, ajusta UV, muestrea page pool.
- **Feedback pass**: cada N frames el engine renderiza la escena a un buffer low-res (256×256 o 512×512) escribiendo como color el tile-ID deseado por píxel (`pack(virtual_page_x, virtual_page_y, mip_level)`). CPU lee el buffer, calcula set de tiles únicos → cola de streaming.
- **Streamer**: thread dedicado lee del disco, decomprime JPEG-XR / HD-Photo + transcodifica a DXT → upload a page pool con `glTexSubImage2D` / equivalente D3D9. Swap en indirection.
- **Autoría**: los artistas pintaban directamente sobre la geometría en Mudbox/Mari. El build tool "compilaba" el mega-atlas off-line en horas.

**Por qué falló en Rage 2011 (no el concepto, la implementación en HW de 2011):**
1. **Throughput de disco insuficiente.** HDD de 2011 en Xbox 360 / PS3 entregan ~20-30 MB/s de reads aleatorios. Rage necesitaba ~15-30 MB/s sostenido con bursts cuando el jugador giraba la cámara. Bursts no se cubrían.
2. **Pop-in dramático.** Con feedback latency de 2-4 frames + lectura de disco de ~50-200 ms por tile, girar la cámara rápido producía el famoso "texture pop": la escena aparecía en mip 6-7 (blurry) y resolvía a mip 0 (crisp) durante 1-3 segundos.
3. **Transcoding CPU-bound.** La decompresión JPEG-XR + recompresión a DXT consumía 2-3 ms de CPU por tile; con bursts de 50-100 tiles/frame, saturaba un core entero.
4. **No caching por sesión.** El engine no persistía tiles entre misiones — al cambiar zona, se descartaba todo el pool.
5. **Review de prensa.** Digital Foundry y Eurogamer puntuaron pop-in como "show-stopper". El concepto quedó marcado como tech risk.

**Lecciones que id aplicó en id Tech 6+ (Doom 2016):**
- **Abandonaron el mega-atlas único.** En Doom 2016 el sistema es "tiled atlas" de 16k × 8k con tiles 128×128 pero compuesto en runtime de materiales clásicos — la autoría vuelve a ser UV-space + shader, NO paint-over-world.
- **id Tech 7 (Doom Eternal, 2020)**: VT "hibrido" — algunos assets muy grandes (sky domes, mundos vastos) usan VT; los props usan texturas convencionales con mip-streaming. El peso del pipeline VT no se paga para cada prop.
- **id Tech 8 (Dark Ages, 2025)**: aún menos VT. La filosofía ahora es "VT donde ayuda, mip streaming donde es suficiente". (Tiago Sousa, SIGGRAPH 2025.)

## 2. Decima virtual textures (Guerrilla / Kojima)

Guerrilla tomó la idea de id Tech 5 y la rehízo sobre hardware de 2017+ (PS4 con SSD opcional, PS5 con NVMe + hardware decompression). Horizon Zero Dawn (2017) y especialmente **Death Stranding (2019)** son los showcases.

**Diferencias clave vs MegaTexture:**
- **Tiled from authoring.** El mundo NO es un único atlas monolítico; se particiona en "VT regions" de 4k × 4k o 8k × 8k por zona lógica. Esto permite purgar/cargar por región al entrar/salir.
- **Feedback buffer al compute shader.** En vez de renderizar a color y leer desde CPU, el feedback se escribe a un SSBO con `imageStore` desde un compute pass conservative-raster. Menos latencia, más datos por pixel (mip derivative explícito, no estimado).
- **Tile size 128×128 + 4px border**, BC7 (PS4 Pro, PC) o ASTC 6×6 (PS5). Sale ~22 KB por tile BC7, ~18 KB ASTC.
- **Physical page pool tipicamente 512 MB** en PS4, 1 GB en PS5 (Guerrilla usa parte del budget para material VT + separate indirection pool para environment probes).
- **Asset on disk** pre-transcodificado por plataforma: la build produce un blob BC7-packed-into-GDEFLATE (PS5) o LZ4 (PC) listo para DMA directo a VRAM con minimal CPU.
- **Streaming prioritization** basado en: distancia a cámara, ángulo de incidencia (superficies frontales priorizadas), velocidad de cámara (frustum predictivo), heurísticas de "visibilidad más allá del frustum actual" (cuando el jugador va a rotar). Guerrilla llamó a esto "cone-based prefetch".
- **SSD asumido.** El sistema está diseñado contra NVMe 5+ GB/s (PS5). Sin SSD, la latencia es inaceptable (por eso PS5-only parts de Horizon Forbidden West usan VT agresivamente; PS4 version fallbackea a más mip-streaming clásico).

**Cómo resolvieron los problemas de Rage:**
1. **Throughput**: PS5 NVMe con hardware Kraken/Oodle Texture decompression = ~8-20 GB/s efectivos. MegaTexture en 2011 no podía soñar con esto.
2. **Pop-in**: prefetch cone + mip-fallback garantizado. El sampler nunca falla: si el tile exacto no está, usa el tile más bajo mip disponible (filtered upwards). Pop-in se convierte en "blurry → sharp" gradual en ~100-500 ms en vez de 2-3s.
3. **Transcoding**: cero en runtime. Todo está pre-transcodificado a BC7/ASTC en el build. El disk-to-VRAM path es casi un memcpy.
4. **Author-time**: los artistas siguen pintando en world-space (Mari), pero el build particiona automáticamente en regiones.

**Ref primaria**: Giliam de Carpentier & Kohei Ishiyama, "Decima Engine: Advances in Lighting and AA," SIGGRAPH 2017. No es VT-specific pero documenta el stack. Más preciso: de Carpentier's blog (https://www.decarpentier.nl/) tiene notas dispersas. La referencia realmente técnica de Decima's VT es **Hans-Kristian Arntzen's Granite blog series** — Arntzen ingenierizó en Arm, pero su Granite engine replica 80% del approach Decima.

## 3. Sparse textures API (hardware level)

Desde 2013 el HW expone directamente el concepto de "virtual address space decoupled from physical memory" para texturas. Ya no hace falta emular con indirection manual (aunque muchos engines aún la usan por control).

### Vulkan `VK_KHR_sparse_binding` / `VK_KHR_sparse_residency`

- **`VK_KHR_sparse_binding`** (core in 1.0): permite crear un `VkImage` con `VK_IMAGE_CREATE_SPARSE_BINDING_BIT`. El objeto existe en virtual address space pero no tiene memoria física asociada. Tú bindeas páginas específicas con `vkQueueBindSparse`.
- **`VK_KHR_sparse_residency_image2D`**: permite acceder a regiones no-bindeadas desde el shader (lectura devuelve 0 o el color "residency") y queryear con `OpImageSparseRead` / `sparseTexelsResident()` si el fetch devolvió datos reales.
- **Tile size fija por formato.** Para BC7 RGBA, tile = 128 × 128 texels (64 KB memoria). Para R8, tile = 256 × 256. Queryable con `vkGetImageSparseMemoryRequirements`.
- **Granularidad**: la GPU alinea binds a tile boundaries. No puedes bindear 17 texels — siempre un tile entero.
- **Aliasing**: un pool físico puede respaldar múltiples virtual images (sparse aliasing). Esencial para "page pool compartido" tipo Decima.

### DirectX 12 Tiled Resources (Tier 1 / 2 / 3)

- **Tier 1** (GCN 1.0+, Kepler+): bind/unbind por tile. Sin residency feedback en shader.
- **Tier 2** (GCN 1.1+, Maxwell+): añade residency status (shader puede query `CheckAccessFullyMapped`). Residency minimum LOD clamp — si un mip no está bindeado, el sampler automáticamente sube a uno que sí.
- **Tier 3** (Turing+, RDNA+): sparse 3D textures, per-mip residency granularity fina. Requerido para VT "agresivo" (no cargar mips altos nunca).
- **Tile size**: misma 64 KB para la mayoría de formatos (128×128 BC7 o 64×64 RGBA8).
- **API**: `CreateReservedResource` + `UpdateTileMappings`.

### OpenGL `ARB_sparse_texture` + `ARB_sparse_texture2` (GL 4.4+)

- Core del approach Vulkan pero más limitado.
- **`glTexPageCommitmentARB(target, level, xoffset, yoffset, zoffset, width, height, depth, commit)`**: commit/uncommit tiles explícitamente.
- Tile size queryable via `glGetInternalformativ(GL_VIRTUAL_PAGE_SIZE_X_ARB, ...)`. Típicamente 128×128 para BC, 256×256 para R8.
- **`ARB_sparse_texture2`** (2014): sparse residency read — el shader puede detectar si un texel es resident con `sparseTexelsResidentARB` (GLSL). Devuelve 0/default si no.
- **ARB_bindless_texture** + ARB_sparse_texture = stack mínimo para un VT decente en GL 4.4 sin Vulkan.
- **Relevancia ALZE**: GL 4.4 (2013) está disponible en 99% de targets actuales. ALZE está en 3.3, pero subir a 4.4 para VT es posible sin ir a Vulkan. `ARB_sparse_texture` es la ruta minimalista.

**Advertencias:**
- No todos los drivers implementan sparse bien. Mesa + AMD Linux tiene soporte maduro; Intel iGPU <Gen11 no soporta sparse. Apple M1/M2 (MoltenVK sobre Metal) emula sparse con perf penalty.
- Tile size 64 KB es un piso; no puedes hacer tiles más pequeños aunque tu asset sea minúsculo.
- Sparse textures **no son cache-coherent** entre CPU writes y GPU reads — necesitas barrera explícita después de `vkQueueBindSparse` (la semáforo se provee en la llamada misma).

## 4. UE5 Virtual Textures — RVT vs SVT

Epic distingue **dos sistemas distintos** bajo el paraguas "Virtual Textures":

### Runtime Virtual Textures (RVT)

- **Qué es**: una textura generada **en tiempo real** por renderización de fuentes existentes (material graph, decals, landscape paint layers) a un atlas. El contenido NO vive en disco — se renderiza on-demand y se cachea.
- **Caso de uso canónico**: terrain blending. En vez de samplear 8 capas splatmap por pixel en el material del landscape (caro), se renderizan las capas a una RVT una vez por región y se samplea 1 vez en el pass principal.
- **Cómo funciona**: un `URuntimeVirtualTexture` declara resolución virtual (ej. 16k × 16k), page size (ej. 128), tipo (BaseColor / Normal / WorldHeight). Los materiales marcados "writes to RVT" se ejecutan en un pass separado que escribe a tiles cuando se solicitan.
- **Feedback mecanismo**: igual que SVT (ver abajo), pero como el "backing store" es el render-a-atlas, rellenar un tile es un draw call, no un disk read.
- **Ventaja**: latency baja (re-render toma <1 ms por tile típico), sin footprint en disco.
- **Limitación**: el material RVT debe ser determinístico y dependiente solo de worldpos (no de view, no de time animated) — porque el cache invalidaría constantemente.

### Streaming Virtual Textures (SVT)

- **Qué es**: el clásico VT "à la Decima" — tiles pre-bakados en disco, streameados bajo demanda.
- **Formato**: UE5 usa **bulk data blocks** con BC7 + tile metadata, dentro de paquetes `.uasset` o split en chunks. Pre-transcodificados por plataforma en el cook.
- **Indirection**: page table texture 2D (uno por virtual texture), típicamente R16_UINT con entradas `(physical_x, physical_y, mip)`.
- **Feedback**: pass low-res (128×128 hasta 512×512 según plataforma) post-base-pass que escribe a un buffer los (vt_id, page_x, page_y, mip) requeridos. Readback es **async** — lag de 2-3 frames.
- **Physical page pool**: un atlas físico (o varios layers) en VRAM. Default ~256 MB pero configurable. Se particiona por formato (un pool para BC1, otro BC7, otro BC5 normales...).
- **Integración Nanite**: las materiales Nanite leen texturas VT igual que cualquier material. El material pass de Nanite (deferred decal-style visibility buffer → material resolution) emite feedback a VT durante la resolución de material.

### Tabla: UE5 RVT vs SVT

| Característica | RVT | SVT |
|---|---|---|
| Origen del tile | Draw-to-atlas (material graph) | Pre-baked en disco |
| Disk footprint | 0 | Terabytes posibles |
| Primer fetch latency | <1 ms (render) | 10-200 ms (disk + decode) |
| Uso típico | Terrain blend, decal accumulation | Environment albedo masivo |
| Invalidation | Cambio de material | Nunca (read-only) |

Refs: Epic docs — https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-texturing-in-unreal-engine, https://dev.epicgames.com/documentation/en-us/unreal-engine/runtime-virtual-texturing-in-unreal-engine, https://dev.epicgames.com/documentation/en-us/unreal-engine/streaming-virtual-texturing-in-unreal-engine.

## 5. Feedback buffer patterns

El corazón de cualquier VT reactivo es el feedback. Tres variantes principales:

### (a) Classic color-write + CPU readback (id Tech 5 style)

- Render pass low-res (256×256 es típico) que usa la misma geometría que el pass principal pero con un shader que escribe `color = pack_tile_id(uv, mip)`.
- Resolve to staging buffer, CPU reads unos frames después.
- CPU computes unique set (hash set), enqueue a streamer.
- **Latencia**: 2-4 frames (pipeline + readback).
- **Costo GPU**: ~0.2-0.5 ms un pass reducido.
- **Costo CPU**: ~0.1-0.3 ms iterate el buffer, dedup.

### (b) Compute-based feedback + SSBO (modern)

- Un compute shader evalúa "qué tiles necesitaría este tile del framebuffer" usando el gbuffer depth+derivatives, sin rerender geometry.
- Escribe a un atomic counter + `imageStore` en un buffer. Dedup parcial GPU-side con hash map lock-free.
- Readback async a CPU.
- **Latencia**: 2-3 frames.
- **Costo GPU**: ~0.1-0.2 ms (solo compute, sin raster adicional).
- **Ventaja**: mip derivative exacto (no estimado), menos pops.

### (c) GPU-driven streamer (Nanite + Lumen integrated)

- El feedback NO vuelve a CPU. GPU mantiene un "queue de tiles requested" en un SSBO, un compute shader evalúa y emite "decompress + upload" jobs a una copy queue. El host observa vía persistent-mapped buffer.
- UE5 se mueve hacia este modelo (aunque CPU aún coordina disk I/O).
- **Latencia**: 1 frame en el mejor caso.
- **Complejidad**: muy alta; requiere GPU upload queues first-class (DMA engine).

### Anti-pop strategies

1. **Mip fallback mandatorio**: el tile mip más bajo (single texel representa toda la VT) siempre resident. El sampler cae a él si nada más está cargado. Costo: negligible memoria (~1 tile por VT).
2. **Prefetch cone**: antes de llegar, streamer predice tiles del frustum futuro.
3. **Blend de mip**: interpolar entre mip_loaded y mip_desired durante 100-300 ms. El jugador ve un "focus-in" suave en vez de un "snap".
4. **Latency hiding con TAA**: con 4+ frames de TAA el pop-in se difumina temporalmente. Ayuda perceptualmente mucho más de lo que se reconoce.

## 6. KTX2 / Basis Universal transcoding

**KTX2** (Khronos Texture 2, 2019): contenedor estándar post-KTX1 para texturas GPU, extensible con supercompression. **Basis Universal** (Binomial / Rich Geldreich, 2018+): un codec intermedio ("universal") que almacena texturas en un formato que se **transcodifica** a BC1-BC7 / ASTC / ETC2 / PVRTC en tiempo de carga según la plataforma.

**Flujo típico:**
1. Autor exporta PNG/EXR.
2. `basisu` encodes a `.ktx2` con Basis UASTC o Basis ETC1S supercompression (ETC1S = más compresión, menor calidad; UASTC = alta calidad, menor ratio).
3. Distribución: el `.ktx2` pesa ~25% de un PNG equivalente.
4. Runtime: la engine transcodifica por-tile a BC7 (PC), ASTC 6×6 (iOS/Android/PS5), ETC2 (Android low-end). Transcoder CPU ~50-200 MB/s por core.

**Ventajas:**
- **Un asset sirve para todas las plataformas** sin re-bake.
- **Disk footprint** reducido significativamente (el codec intermedio Basis + tras Zstd es más pequeño que BCn pre-compressed porque BCn ya es lossy + entropia alta).
- **Modern engines** (Godot 4, Bevy, UE5 via plugin, Three.js) lo soportan de serie.

**Integración con VT:**

- **Opción A (pre-transcode)**: durante el cook, el asset se transcodifica al formato BC/ASTC target **por plataforma** y se empaqueta en el tile atlas final. Ventaja: runtime es disk→VRAM memcpy + GDEFLATE decompress. Decima y UE5 SVT hacen esto.
- **Opción B (runtime transcode)**: tiles en disco siguen en formato Basis universal; el streamer transcodifica a BC7 por tile antes de upload. Ventaja: un asset set para todos. Desventaja: gastas 0.5-2 ms CPU por tile transcodificado. Inviable para bursts grandes.

**Recomendación ALZE**: opción A con fallback B. Pre-transcode para PC (BC7 universal support post-2012 GPUs). El pipeline se beneficia de Basis porque **el asset master es un solo KTX2** — si mañana ALZE porta a Switch, basta re-transcodear el cook sin tocar arte.

Refs: https://github.com/KhronosGroup/KTX-Software, https://github.com/BinomialLLC/basis_universal. Paper explanation: "A crash course on KTX2 and Basis Universal," Khronos blog 2020.

## 7. Mesh streaming + Nanite interaction

En UE5 la interacción Nanite ↔ VT es sutil:

- **Nanite geometry streaming** (cluster hierarchy) es un sistema separado de VT. Streamea clusters mesh, no texels. Lo cubrió `nanite.md` en este round.
- **Nanite materials usan VT para textura.** El material pass Nanite (visibility buffer → material evaluate) samplea VT SVT igual que cualquier material tradicional. Feedback se genera desde el pixel shader del material pass.
- **RVT + landscape**: en UE5 terrain, el landscape paint layers se bakean a una RVT (BaseColor + Normal + Height + BlendMask en 4 layers RVT). El terrain shader hace 4 samples a RVT en vez de 16+ samples al splatmap original.
- **RVT + characters/dynamic objects**: los characters NO escriben a RVT (sería cache-thrash). Solo objetos estáticos o estables (decals acumulativos, terrain, carreteras). Característicamente, caminar por el terrain NO invalida VT cache — lo que invalida es editar paint layers en editor.
- **Contaminación de VT cache por moving objects**: si pusieras un character en la RVT, su movimiento forzaría redraw continuo. Diseño: VT es para world-stable content. Dynamic = traditional texture.

## 8. Asset pipeline implications

Virtual textures no son una técnica "drop-in". Imponen una reestructura del pipeline:

**Autoría world-space vs UV-space:**
- VT fomenta **world-space painting** (Mari/Mudbox/Substance Painter world-space projection). El terrain y arquitectura se pinta sobre la geometría real, no sobre UVs planos.
- Pros: arte único, sin tiling visible, mejor proyección curvas (paredes curvas).
- Contras: UVs deben ser "world-space aware" (chart packing que respete worldpos), muchos DCC tools no soportan bien esto. El pipeline requiere tooling custom: unwrap, pack, bake, tile.

**LOD mip levels:**
- VT bakea una pirámide completa: desde mip 0 (texel-per-world-cm) hasta mip log2(virtual_size) (texel-per-world-km).
- Mip-chain tree en disco ocupa 4/3 del mip 0 (geométrica). Una VT de 128k × 128k en BC7 = 2 GB mip 0 + ~700 MB mips superiores = ~2.7 GB por virtual texture.

**Disk footprint ballparks (from Decima docs + UE5 community):**
- Open-world terrain 64 km² con albedo + normal + roughness + AO @ 0.5 cm/texel (PS5 target): **~25-35 GB** por zona.
- Interior level (shooter corridor) con unique texturing @ 1 cm/texel: **~5-10 GB**.
- A modo comparativo, GTA V sin VT: **~60 GB** totales (texturas mip-streamed + meshes). El salto a VT añade unos 20-30% de disk si se aplica al mundo entero, por eso Decima/UE5 lo aplican selectivamente.

**Tooling implícito:**
- Asset watcher que recompila tiles solo si zona cambió.
- Preview en editor que samplea VT con mip cap ajustable.
- Perf profiler: "VT cache miss rate", "tiles fetched/frame", "pool fragmentation". UE5 expose `r.VT.*` cvars.

## 9. Memory + disk numbers típicos

Valores de sistemas productivos (UE5 default, Decima PS5, Granite sample config):

| Parámetro | Valor típico | Notas |
|---|---|---|
| Virtual texture size | 16k × 16k hasta 128k × 128k | Per VT, varios VTs por escena |
| Physical page pool | 256 MB (default UE5) hasta 1 GB (PS5) | Por tipo de formato, layered |
| Tile size | 128 × 128 texels | + 4 px border = 136² pero address espacio nominal 128 |
| Border size | 4 texels | Para filtrado anisotrópico correcto |
| Feedback buffer | 256 × 256 o 512 × 512 | Mucho más bajo que framebuffer |
| Readback latency | 2-4 frames | Pipelined con triple-buffer staging |
| Disk GB por zona (open-world) | 20-40 GB | BC7 + Oodle/GDEFLATE compressed |
| Tile decode time | 0.05-0.5 ms | GPU GDEFLATE (PS5) fastest |
| Tile network/disk fetch | 50-200 ms (HDD), 1-5 ms (NVMe) | Dominante en HDDs |

**Caso extremo Granite** (Hans-Kristian Arntzen, 2017-2020): el sample runs 256 MB pool sobre 8k × 8k virtual en demo.

**Footprint de indirection**: una VT de 128k × 128k con tile 128 → 1024×1024 tiles → R32_UINT indirection texture = 4 MB (solo mip 0) + mips = ~5 MB total por VT. Negligible.

## 10. ALZE applicability

**Contexto ALZE actual:** C++17, OpenGL 3.3, SDL2, equipo pequeño (1-3 devs). No hay Vulkan ni GL 4.4 aún.

**Subir a GL 4.4 es opcional pero barato** — la mayoría del código 3.3 sigue funcionando, añades `#version 440` en shaders que lo necesiten. `ARB_sparse_texture` desbloquea un VT real sin reescribir el renderer.

**Tres versiones incrementales:**

### v1 (GL 3.3, hoy): mip streaming clásico + presupuesto de memoria

- NO virtual textures. Ni sparse textures (3.3 no lo expone).
- Mip-streaming estándar: cada textura tiene todos los mips, engine elige cuál subir según distancia.
- Budget total (ej. 512 MB VRAM para texturas). LRU eviction cuando se supera.
- KTX2 + Basis Universal para assets → transcode a BC7 en load time, cacheado en disk transcoded.
- Esto cubre 95% del valor de VT para una ALZE-scale game y es ~2 KLOC de streamer.

### v2 (GL 4.4, cuando sea): sparse texture terrain-only

- UN solo virtual texture para el terrain (no el mundo entero).
- VT size 16k × 16k (o 32k × 32k), tile 128 × 128 BC7. ~1-3 GB disk, ~128 MB pool.
- Feedback: color-write low-res (256×256) + CPU readback. Simple, funcional, 2-3 frame latency tolerable.
- Indirection texture R32_UINT 128 × 128. Upload de tiles con `glTexSubImage2D` en la subregion del pool.
- Autoria: mantener UV-space painting + blend splatmap tradicional. VT se genera off-line como RVT-style baking durante cook. NO world-space painting tools (fuera de scope).
- **Usa fallback mip always-resident** para garantizar sampler nunca falla.
- Presupuesto efort: ~4-6 KLOC (streamer, feedback pass, indirection manager, tile pool). 2-3 semanas de 1 dev experienciado.

### v3 (Vulkan u OpenGL 4.6 + async compute): full SVT + RVT híbrido

- Extensión a múltiples VTs (terrain + props unique texturing opcional).
- Feedback vía compute shader + GPU-driven.
- RVT para landscape blending (runtime composed desde splatmap layers).
- Pre-transcoded KTX2/Basis por plataforma en cook.
- Presupuesto efort: 10-15 KLOC, 2-3 meses. **Solo justificable si ALZE target es open-world grande** (>10 km²).

### Tabla: VT system comparison

| Sistema | Feedback mechanism | Tile format | Max tile res | Physical pool típica | Known issues |
|---|---|---|---|---|---|
| MegaTexture (id Tech 5, 2011) | Color-write + CPU readback | DXT5 (BC3) | 128×128 + 4 border | 64-256 MB | Pop-in severo en HDD; CPU transcode caro |
| Decima VT (PS4/PS5) | Compute + SSBO | BC7 (PC), ASTC 6×6 (PS5) | 128×128 + 4 border | 512 MB - 1 GB | Requiere NVMe para full-res; complejidad autoría |
| UE5 RVT | Compute feedback + render-to-atlas | Runtime-rendered (any) | 128×128 default | 128-256 MB | Solo determinístico; no dynamic content |
| UE5 SVT | Compute feedback + async readback | BC7 / ASTC pre-baked | 128×128 default | 256 MB default | Disk footprint (terabytes posibles) |
| Granite (Arntzen, OSS) | Compute + SSBO | BC7 / ASTC | 128×128 | Configurable (demos 256 MB) | Single-dev OSS, no production-hardened |

### Tabla: ALZE applicability por versión

| Tier | Scope | Dependencias | Efort | Riesgo | Recomendación |
|---|---|---|---|---|---|
| v1 (hoy) | Mip-streaming clásico + KTX2/Basis + budget cap | GL 3.3, libktx, basisu | ~2 KLOC, 1 semana | Bajo | **HACER YA** |
| v2 (6-12m) | Sparse texture terrain only (1 VT) | GL 4.4 + ARB_sparse_texture | ~4-6 KLOC, 2-3 sem | Medio (drivers) | Posible, pero mide antes |
| v3 (12-24m) | Full SVT + RVT híbrido, multi-VT | Vulkan 1.2 + async compute | ~10-15 KLOC, 2-3 meses | Alto | Solo si ALZE abre open-world grande |

## Veredicto honesto

**¿Vale VT la pena implementar en ALZE dado el scope de equipo pequeño?**

**No en v1. Probable en v2 si se escala. Aspiracional en v3.**

Razones concretas:

1. **El retorno de VT depende del perfil del juego.** VT paga dividendos para mundos >5 km² con unique texturing. Si ALZE apunta a corridor shooter, puzzle game, arena combat o incluso open-world pequeño (<2 km² denso), **mip-streaming clásico + un budget cap bien ajustado** entrega el 90% del beneficio con el 10% del costo de ingeniería.

2. **El coste oculto es el pipeline de autoría.** Mentir al equipo diciendo "solo VT en runtime" olvida que alguien debe construir el tooling para particionar, bake, visualizar, invalidar. Eso es 50% del esfuerzo total y es trabajo específico del DCC/cook path. Un pequeño equipo se quema ahí antes de ver el beneficio runtime.

3. **Sparse textures en drivers no-AAA siguen siendo beta.** Mesa AMD Linux funciona; Intel iGPU <Gen11 no; Apple/Metal emula con penalty. Para un proyecto multiplatafora hobby-scale, depender de sparse textures es meter una dependencia de driver-quality que no controlas.

4. **Modern asset compression (Basis/KTX2 + Zstd/Oodle) ya resuelve 60% del problema disk.** Si el dolor es "mis texturas son muy grandes en disco", la respuesta es Basis + BC7 + Zstd, no VT. VT es la respuesta cuando el dolor es "mi VRAM no cabe ni con streaming convencional".

5. **Para un single-dev cook time importa.** VT cook times se miden en horas por zona (bake + particionar + mipmap + supercompressing cada tile). Esto mata iteración. Traditional streaming está ya listo al exportar un PNG.

**Ruta recomendada para ALZE:**

- **v1 ahora**: un texture streamer decente con budget cap de 512 MB, LRU eviction, KTX2/Basis pipeline, mip selección por distancia cámara. Esto no es "cutting-edge" pero es sólido y suficiente para 2-10 km² worlds.
- **Medir** qué partes del mundo son texture-bound en VRAM. Si el terrain dominates, considera v2 (un solo VT para terrain).
- **v2 solo cuando hay evidencia** de que el budget cap está doliendo a la calidad visual en targets-hardware mínimos.
- **v3 reservado a un posible sequel con scope open-world grande** o si un contribuidor especializado se suma al proyecto.

La trampa de un equipo pequeño es copiar features AAA porque son "the modern way". MegaTexture era "the modern way" en 2011 y quemó a Rage. La disciplina de trade-off honesto — "¿necesito VT o mip-streaming con buen budget?" — es más valiosa que el VT mismo.

**Citable para defender esta postura:** la propia id pisa el freno en id Tech 7/8 (Sousa SIGGRAPH 2025) — reducen VT a "where it helps" en vez del todo-VT de id Tech 5. Si id Software retrocede, un equipo pequeño no tiene razón para avanzar más allá.

## Fuentes consultadas

- John Carmack, "Id Tech 5 Challenges — From Texture Virtualization to Massive Parallelization," Quakecon 2008 keynote — https://www.youtube.com/watch?v=wt-iVFxgFWk (archive: https://web.archive.org/web/*/youtube.com/watch?v=wt-iVFxgFWk)
- John Carmack, QuakeCon 2011 keynote (Rage tech post-mortem topics) — https://www.youtube.com/watch?v=CyEg3XqpIWg
- Martin Mittring (Epic, later Crytek/id), "Advanced Virtual Texture Topics," SIGGRAPH 2008 course "Advances in Real-Time Rendering" — https://advances.realtimerendering.com/s2008/ (slides: https://www.ea.com/frostbite/news/advanced-virtual-texture-topics archive fallback, or https://web.archive.org/web/2020*/developer.amd.com/wordpress/media/2013/01/Chapter02-Mittring-AdvancedVirtualTextureTopics.pdf)
- Sean Barrett, "Sparse Virtual Textures" GDC 2008 — https://silverspaceship.com/src/svt/ (paper + sample code, still live)
- id Software "Megatexture" Wikipedia summary — https://en.wikipedia.org/wiki/MegaTexture
- Giliam de Carpentier & Kohei Ishiyama (Guerrilla), "Decima Engine: Advances in Lighting and AA," SIGGRAPH 2017 — https://advances.realtimerendering.com/s2017/DecimaSiggraph2017.pdf
- Giliam de Carpentier blog (Decima / VT context) — https://www.decarpentier.nl/
- Hans-Kristian Arntzen, "Virtual Texturing with Granite" blog series (2017-2020) — https://themaister.net/blog/ (search "virtual texturing"; sample repo https://github.com/Themaister/Granite)
- Epic Games, "Virtual Texturing in Unreal Engine" — https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-texturing-in-unreal-engine
- Epic Games, "Runtime Virtual Texturing in Unreal Engine" — https://dev.epicgames.com/documentation/en-us/unreal-engine/runtime-virtual-texturing-in-unreal-engine
- Epic Games, "Streaming Virtual Texturing in Unreal Engine" — https://dev.epicgames.com/documentation/en-us/unreal-engine/streaming-virtual-texturing-in-unreal-engine
- Khronos, `VK_KHR_sparse_binding` / `sparse_residency` spec — https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_KHR_sparse_binding.html, https://registry.khronos.org/vulkan/specs/latest/html/chap34.html#sparsememory
- Khronos, `ARB_sparse_texture` / `ARB_sparse_texture2` — https://registry.khronos.org/OpenGL/extensions/ARB/ARB_sparse_texture.txt, https://registry.khronos.org/OpenGL/extensions/ARB/ARB_sparse_texture2.txt
- Microsoft, "Tiled Resources" (D3D12) — https://learn.microsoft.com/en-us/windows/win32/direct3d12/tiled-resources
- Khronos, "KTX File Format Specification (2.0)" — https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html
- KTX-Software (libktx) — https://github.com/KhronosGroup/KTX-Software
- Basis Universal — https://github.com/BinomialLLC/basis_universal
- Rich Geldreich, "Announcing Basis Universal" blog 2019 — http://richg42.blogspot.com/2019/05/announcing-basis-universal-texture.html
- Tiago Sousa (id), "Fast as Hell: idTech 8 GI," SIGGRAPH 2025 — https://advances.realtimerendering.com/s2025/content/SOUSA_SIGGRAPH_2025_Final.pdf
- Jean Geffroy et al. (id), "Rendering the Hellscape of Doom Eternal," SIGGRAPH 2020 — https://advances.realtimerendering.com/s2020/RenderingDoomEternal.pdf
- Adrian Courrèges, "DOOM (2016) Graphics Study" (VT atlas analysis) — https://www.adriancourreges.com/blog/2016/09/09/doom-2016-graphics-study/
- Fabian Bauer (Rockstar North), "Creating the Atmospheric World of RDR2," SIGGRAPH 2019 — https://advances.realtimerendering.com/s2019/ (relevance: showcases non-VT streaming at similar world scale)
- Digital Foundry, "Rage Tech Analysis" 2011 — https://www.eurogamer.net/digitalfoundry-2011-rage-pc-face-off (pop-in critique)
- J.M.P. van Waveren (id), "Real-Time Texture Streaming & Decompression" id Software tech paper 2006 — https://mrelusive.com/publications/papers/Real-Time-Texture-Streaming-&-Decompression.pdf
