# Lumen + Virtual Shadow Maps — deep dive (R3 / ALZE Engine)

Fecha: 2026-04-22. Consumer: ALZE Engine (C++17 no-RTTI/no-exceptions, SDL2 + OpenGL 3.3, futuro Vulkan, ~25-30K LOC).

Esta nota profundiza Lumen (UE5 dynamic GI) y Virtual Shadow Maps. No duplica `../ue5.md` — aquí van las matemáticas y estructuras de datos reales. Referencias primarias al final con URLs y notas de acceso.

## 1. Contexto: qué problema resuelve Lumen

Lightmap bake de UE4 = horas de preprocess + no funciona con geometría/luz dinámica. Ray tracing hardware = caro + requiere RTX 2000+. Voxel Cone Tracing (Crassin 2011) = leaking inevitable en geometría fina. SSGI = se rompe fuera de pantalla. Lumen apunta a calidad ~bake-quality indirect diffuse + specular con 4-8 ms de presupuesto GPU en PS5/XSX, sin depender de RT hardware y sin bake.

Ideas centrales (Wright et al. SIGGRAPH 2022):

1. **Desacoplar shading del trace**: en vez de evaluar materiales completos en cada hit (lo clásico), los rayos samplan un **surface cache** 2D pre-shadeado por objeto. Coste del hit ≈ texture fetch.
2. **Ray tracing backend intercambiable**: software (mesh SDF + global SDF) o hardware (DXR BVH). Mismo "shade hits via cards" por encima.
3. **Radiance caching two-level**: screen-space probes (cara pero adaptativo, sigue la cámara) + world-space probes (persistentes, amortizan entre frames).
4. **Integración desacoplada de resolución**: final gather a 1/16 spatial downsample; el integrator a full-res usa estos probes como lookup table filtrada.

Los subsistemas Lumen son: `Lumen Scene` (mesh cards + surface cache + voxel scene), `Lumen Radiance Cache` (world probes), `Lumen Screen Probe Gather` (screen probes + final gather), `Lumen Reflections` (separate path para specular), `Lumen Hardware Ray Tracing` (BVH backend opcional).

## 2. Lumen Scene — surface cache + mesh cards + global SDF

### 2.1 Mesh Cards

Cada mesh estático recibe, en el build, un conjunto de **cards**: proyecciones axis-aligned (típicamente las 6 caras del AABB, más cards adicionales si la geometría lo pide — Epic usa un placement automático que heuristicamente añade cards para concavidades). Cada card es una textura 2D pequeña (256² o 512² típicamente, configurable) que rasteriza el mesh desde la dirección de la card y guarda **albedo, opacidad, normal, emisión** en el atlas global.

- Atlas de producción en UE5: **Material Atlas** = un mega-texture 2D con bin-packing de todas las cards de todos los meshes cargados. Viene en 4 atlases MRT: `LumenSceneAlbedo` (RGB + opacity), `LumenSceneNormal` (R11G11B10 octahedral), `LumenSceneEmissive` (R11G11B10), `LumenSceneDepth` (R16F, profundidad desde el plane de la card).
- Resolución típica: 8192×8192 o 12288×12288 atlases. ~256 MB footprint para escenas AAA.
- Cards se generan **en build time**, por eso Lumen tiene un paso de build (no bake de light, sino de cards). Meshes skeletal no participan (no estáticos), por eso personajes dinámicos "flotan" en GI — se fakean con distance field hacks.

### 2.2 Surface Cache propiamente

Paralelo al atlas de material va el `Lumen Surface Cache` = atlas **igual layout** que guarda la irradiance **indirecta y directa** integrada, en `R11G11B10F` por texel.

- Actualización: cada frame, un shader recorre un subset rotado de cards (budget 512×512 texels de update / frame típico) y re-samplea iluminación directa: N·L por cada luz local visible + shadow map sample + sky visibility. Para rebotes indirectos, **muestrea el propio cache con multi-bounce** (feedback loop — rebote 2 y 3 "gratis" si esperas unos frames).
- Resultado: cuando un rayo Lumen golpea una card, devuelve `albedo * (direct + indirect)` ya shadeado; no evalúa material HLSL en el hit. Es casi un lookup de 2 texels.
- Atlas updates prioritizados por visibility + distance + "dirty" (luces movieron). Static parts se quedan cached entre frames.

### 2.3 Global Distance Field (GDF) y Mesh Distance Fields (MDF)

Para el **software ray tracing** Lumen necesita una representación geométrica trazeable sin BVH hardware. Dos niveles:

- **Mesh Distance Field (MDF)**: por cada mesh estático, un volume texture 3D (típicamente 32³ a 128³, `R8_SNORM`) que guarda la distancia signada al surface más cercano, normalizada a un radio local. Se construye offline. Permite **sphere tracing**: para un rayo que está en el punto p, `d = sample(MDF, p)` te dice cuánto puedes avanzar sin impactar. Si `d < ε` → hit.
- **Global Distance Field (GDF)**: volume texture 3D en **clipmap cascades** (4-5 niveles) centrado en la cámara. Cada cascada dobla la extensión espacial y la resolución por voxel se reduce. En UE5 los defaults son: cascada 0 = 64 m lado @ `R8_SNORM` 256³; cascada 4 = ~1 km lado @ 256³. GDF se construye por frame componiendo los MDFs de los objetos cercanos en un compute pass.

Ray tracing software:

```
hit trace_sphere(ray r):
    p = r.origin
    for step in 0..maxSteps:       # maxSteps ~ 64
        d = sample_GDF(p)
        if d < eps:                # hit
            # refine with local MDF of closest mesh
            d = sample_MDF(p, localMesh)
            if d < eps:
                return sample_surface_cache(p, localMesh.cardAtlasUV)
        p += r.dir * d             # safe advance
        if dist(p, r.origin) > r.tmax: return miss
```

La elegancia: un trace = ~64 texture lookups sobre un par de 3D textures. Compute shader throughput excelente en GPU moderna.

### 2.4 Voxel Lighting (Far Field, >~200 m)

Para distancias > radius del GDF, Lumen usa un **voxel scene** más grosero (global irradiance volume a 2 m/voxel típico) que se alimenta del surface cache y del sky light. Sirve para occluir/sky-shade rayos largos sin tracer GDF completo.

Desde 5.3 Epic añadió **Far Field Traces** = extensión hasta 1 km vía un BVH coarse (solo meshes "farfield-enabled") para quitar el pop/leak en open worlds.

## 3. Screen probes y final gather

Aquí vive la parte más densa matemáticamente — el "final gather diffuse".

### 3.1 Screen probe placement

Cada frame, en una primera pass, se coloca una grid de **screen probes**:

- Pass uniforme: una probe cada 16×16 píxeles + jitter temporal (bluenoise offset). A 1920×1080 son ~8000 probes.
- Pass adaptive (2 iteraciones a 8 px y 4 px): detecta áreas de alta varianza (edges geométricos, material discontinuities) y añade probes extra. Total ~12-20 K probes/frame según escena.
- Desde UE 5.6, un **nuevo algoritmo adaptive** reduce probe count preservando calidad: clasifica pixels por plane distance + normal deviation y sólo añade probes cuando la vecindad de 16x16 no los comparte.

Cada screen probe guarda en un atlas:

- **Radiance**: octahedral map 8×8 (`R11G11B10F`) = 64 direcciones integradas + 1-texel guard band (→10×10 storage).
- **BRDF PDF SH**: 3 SH bands (9 floats) proyectados del cosine-weighted BRDF lobe de los pixels alrededor.
- **Lighting PDF SH**: 2-3 SH bands del frame anterior (reproyectado) = prior de dónde vino luz.

### 3.2 Importance sampling

Para cada probe se lanzan **~32-64 rayos**, distribuidos por sampling de `BRDF_PDF * Lighting_PDF`:

1. Se combina la PDF BRDF (cos-weighted) con la PDF de la luz (previa) multiplicando las SHs (producto de SH en proyección).
2. Se samplea la PDF resultante con **CDF inversión en octahedral space**. 64 direcciones eligen mayormente hacia donde vino luz + brdf significativo.
3. Cada rayo trace (GDF software, o BVH hardware) → surface cache lookup → devuelve radiance.
4. Las 64 sample radiances se acumulan en el octahedral 8×8 del probe.

Costo: 64 rays × 12K probes/frame = 768K rays/frame. A 1080p nativo, 0.4 rays/pixel efectivos — órdenes de magnitud menos que path tracing SPP=1.

### 3.3 Temporal filtering

El probe del frame N se blendea con el del frame N-1 reproyectado (motion vector del pixel-anchor del probe, con bilateral reject sobre normal + depth). Blend típico 0.1 — equivalente a moving average exponencial ~10 frames. Esto es lo que permite usar 32-64 rays/probe sin que las luces parpadeen.

Ghosting se mitiga con: (a) bbox clamp sobre vecindario octahedral; (b) disocclusion reject (si el probe actual no encuentra match histórico con depth similar, descarta); (c) per-probe confidence weight.

### 3.4 Integración a full-res

El pass final (diffuse integrator) corre a resolución nativa. Para cada pixel:

1. Encuentra los 4 screen probes más cercanos (bilinear en probe grid).
2. Bilinear weighting por **plane distance + normal** (rejection si la superficie del pixel diverge de la del probe → bilateral upsampling).
3. Sample octahedral de cada probe en dirección `dot(normal, V)` (integrado sobre cosine-weighted BRDF).
4. Suma ponderada = irradiance indirect diffuse en ese pixel.

Pipe total: `screen_probe_place → trace → filter_temporal → diffuse_integrate → output`. 8000 probes × 64 rays a 1440p ~3 ms en PS5.

## 4. World-space Radiance Cache (WRC)

Además de los screen probes (adaptados a la cámara), Lumen mantiene un **grid 3D de world-space probes persistentes** centrado en la cámara:

- Grid: típicamente 48×32×48 en clipmap (~73K probes). Resolución radiance: 32×32 octahedral por probe (más alta que screen probes — trace budget más generoso).
- Update: cada probe re-traza **64-128 rayos/frame**, pero **sólo un subset de probes se updatea cada frame** (round-robin + prioridad por visibility). Efectivamente ~2% de probes/frame → cycle de 50 frames.
- Propósito: los screen probes **delegan rayos largos (>5 m) al WRC**. Un rayo largo de screen probe hace `traceGDF → hits WRC cell → returns irradiance cacheado`. Es el equivalente a "final gather en 2 pasos": screen probe ≈ irradiance estimate cercano + WRC ≈ irradiance estimate lejano, cacheado y amortizado entre frames.
- Jerarquía que reduce varianza dramáticamente: las direcciones lejanas (donde el trace sería más caro y el signal más ruidoso) se toman del WRC (pre-integrado).

Narkowicz (2022) describe esto como "cone-tracing analogy" pero implementado con sampling point-based.

## 5. Screen traces + world traces fallback

La primera intersección que Lumen intenta NO es en world space — es en **screen space**:

1. **Screen trace**: para cada rayo de screen probe, raymarch en HZB (hierarchical Z-buffer, mipmap del depth). 16 steps max, exactitud pixel-perfect para hits visibles. Coste ~0.3 ms para 768K rays.
2. Si screen trace no hit dentro de `thickness` tolerance, o sale del frustum → **fallback a world trace**:
   - **Software**: sphere-trace GDF/MDF → surface cache lookup.
   - **Hardware**: DXR ray query → surface cache lookup (o hit lighting, ver §7).
3. Si world trace tampoco hit → sample **sky cubemap** (sky light).

El handoff screen→world se hace en el mismo compute pass con `if (!screenHit) worldTrace()` — no hay readback. Motivación: >60% de rays típicamente resuelven en screen space (cerca de la superficie local) a 10× menos coste que GDF.

## 6. Hardware Lumen (HWRT)

Cuando está disponible DXR/VK_RT, Lumen cambia el backend "world trace":

### 6.1 Surface Cache mode (default HWRT)

- Build BVH de todos los meshes estáticos de la escena (TLAS + BLAS update per frame para dinámicos).
- Ray query: `RayFlags.ACCEPT_FIRST_HIT` sobre TLAS → closest hit shader mínimo → samplear `LumenSurfaceCache` con UV calculada desde el triangle + baricentric + card projection.
- No evalúa material HLSL en el hit. Igual de barato que SW en shading, **mejor precisión geométrica** (triángulos exactos, no SDFs aproximados) — importante para paredes finas, foliage, cables.

### 6.2 Hit Lighting mode

- Hit shader evalúa **material HLSL completo** en cada intersección: sample textures, run material graph, apply direct lighting.
- **5× más caro** que Surface Cache (SIGGRAPH 2022 Matrix Awakens: Hit Lighting 11.54 ms vs Surface Cache 2.44 ms en reflections, PS5).
- Gana cuando materials son complejos (layered), en reflections de primer rebote. Default en Movie Render Queue / cinematics.

### 6.3 Inline vs non-inline

- UE5 5.0-5.2: usa inline ray queries (ray queries desde compute shader, sin callable hit shaders).
- 5.3+: path alternativo con Shader Execution Reordering (SER, RTX 40+) que reordena hits por material → mejor occupancy con Hit Lighting.

### 6.4 Hardware Lumen es NO-default incluso en PC

En 5.4+, SW es default en todas las plataformas excepto PS5 Pro / RTX Series X|S con HW Lumen forzado. Razón: consistencia cross-platform + overhead de BVH build + driver instability.

## 7. Hybrid / fallback por plataforma

| Plataforma          | Backend default     | Detalle                                                                 |
|---------------------|---------------------|-------------------------------------------------------------------------|
| PS5 / Xbox Series X | SW Lumen            | 60 FPS perf mode, 30 FPS quality mode                                  |
| Xbox Series S       | SW Lumen low        | Radiance cache res reducida, screen probes a 1/32 (no 1/16)            |
| PS5 Pro             | HWRT (PSSR + RT)    | Hit Lighting opt-in; Surface Cache default                             |
| PC RTX 30+          | HWRT opt-in         | Surface Cache; Hit Lighting en cinematic presets                       |
| PC GTX / Intel UHD  | SW Lumen low / off  | GI desactivado en low preset                                           |
| Quest 3 / Mobile    | No Lumen            | Forward renderer + baked (UE5 mobile path)                             |

Números de Epic (Looman UE 5.6 Performance Highlights + Epic docs):

- **1080p PS5 SW Lumen**: 2-4 ms total (GI + reflections). Target 60 FPS = 4 ms budget en 16.66 ms frame.
- **1440p PS5 HW Lumen Surface Cache**: ~4 ms.
- **1440p PS5 HW Lumen Hit Lighting**: 8-12 ms (cinematics).
- **4K PC RTX 4090 HW Lumen**: 2-3 ms.

## 8. Virtual Shadow Maps

Ortogonal a Lumen pero casi siempre usado con él — resuelve shadow mapping para el mismo use case de mundos grandes con un sólo directional light + multiple spot/point lights.

### 8.1 Modelo clásico (CSM) y su problema

Cascaded Shadow Maps: 3-5 shadow map arrays 2K × 2K rendered per cascade. Pre-allocated. Problemas:

- Shimmering cuando la cámara se mueve (cada cascada re-renderiza everything).
- Trade-off de resolución brutal: para 2 km de view distance, la última cascada tiene ~10 cm/texel.
- No escala a 10+ luces — cada una necesita su propia cubemap.

### 8.2 VSM core idea

Una shadow map **virtual** de 16384×16384 (16K²) por luz. NUNCA se aloca físicamente. En vez:

- **Page table**: grid 128×128 de entries (cada entry mapea una tile virtual 128×128 texels). 32 bits por entry: `residency + dirty + frameMarker + physicalPageXY`.
- **Physical page pool**: texture `R32F` (reinterpretada como `R32U` para `imageAtomicMin` en renderizado) de típicamente 4096×4096 = 1024 pages × 128² texels = 16 MB. Compartida entre todas las luces VSM.

### 8.3 Algoritmo por frame

```
1. Clear page table frame markers
2. Mark phase:
     for each pixel of depth buffer:
         world_pos = unproject(pixel)
         for each shadowed light:
             light_clip = light.viewProj * world_pos
             virtual_uv = light_clip.xy * 0.5 + 0.5
             page = floor(virtual_uv * 128)
             pageTable[page].needed = 1
3. Allocate phase:
     Walk page table; for each needed page without valid residency:
         alloc physical page (LRU evict)
         mark dirty
4. Render phase (one draw per dirty page cluster):
     for each dirty page:
         cull meshes whose AABB projects into the page's virtual footprint
         render to physical page via imageAtomicMin on R32U (depth)
5. Sample phase (later, in lighting shader):
     light_clip = light.viewProj * world_pos
     virtual_uv = ...
     page = floor(virtual_uv * 128)
     phys_uv = pageTable[page].physXY + frac(virtual_uv * 128) / physTexSize
     shadow = sample(physicalAtlas, phys_uv)
```

### 8.4 Caching entre frames

Si la geometría de una page no cambió (meshes estáticos + light estática), la page del frame anterior se reusa intacta. El page table `frameMarker` + `invalidationList` (bbox de moving objects del frame) determina qué pages re-renderizar.

Pero caching tiene constraints duros:

- Si **un solo pixel** del source atlas de un page se marcó invalid (mesh movió, luz rotó), la page entera se re-renderiza.
- Directional lights tienen un problema: rotación del sol invalida **casi toda** la page table. Fortnite Chapter 4 (Epic blog) explícitamente desactiva cache para directional lights (asume re-render per-frame); cachea sólo spot/point. Trade-off: se ahorra CPU overhead de invalidation tracking, más GPU cost de re-render.

### 8.5 Integración con Nanite

El software rasterizer de Nanite puede escribir directamente al physical atlas via atomic depth compare. Esto amortiza dramáticamente el coste: en vez de hacer un `DrawIndirect` per shadow map, el Nanite visibility buffer pass emite "pixels en luz virtual space" directamente. **Fast path** exclusivo de Nanite meshes. Static meshes no-Nanite pasan por el hardware rasterizer path (más lento).

### 8.6 Números

Epic Fortnite Chapter 4 (blog):

- Physical pool default: 4K × 4K = 64 MB (R32F) → ~1024 pages.
- Virtual res 16K² per light → 128×128 = 16384 pages potenciales por luz.
- Fortnite BR runtime: ~200-500 pages alocadas per frame = 12-30 MB working set.
- Directional light VSM en PS5: 3-5 ms vs ~8 ms CSM equivalente (grosero, depende de escena).
- Best gain: indoor scenes con muchas spot/point lights. CSM no podía con 50+ shadowed lights; VSM maneja 100+.

### 8.7 Comparación vs CSM

| Axis                | CSM                        | VSM                                        |
|---------------------|----------------------------|--------------------------------------------|
| Memory              | Fixed 4-5 × 2K² per light  | Shared pool 64 MB total                    |
| Resolution near     | ~5 cm/texel                | ~2-3 cm/texel                              |
| Resolution far      | ~2 m/texel                 | Uniform across distance                    |
| Caching             | Difícil (everything or nothing) | Per-page, 80% hit rate típico         |
| Dynamic geometry    | OK (re-render cascada)     | OK (page invalidate)                       |
| Moving sun          | OK (same cost)             | Malo (page cache inválido)                 |
| Multi-light scale   | 3-5 lights max              | 100+ lights                                |
| Nanite synergy      | Draws por cascada           | Fast path directo a atlas                  |

## 9. Técnicas relacionadas — comparativa

### 9.1 DDGI (Majercik et al. 2019)

- Grid 3D uniforme de probes, cada una guarda **irradiance octahedral 8×8** (10×10 con guard band, `R11G11B10F`) + **visibility moments octahedral 16×16** (`R16G16F` = mean distance + mean squared distance).
- Update: ~288 rays/probe/frame (RTXGI default). Cada ray trace → hit point → compute direct lighting + sample probe irradiance → acumula con weight exponencial (α~0.05).
- Shading query: para un pixel P con normal N, interpolation trilinear de los 8 probes vecinos; cada probe weighted por **Chebyshev test** contra distancia(P, probe):
  ```
  weight_vis = max(0, (mu + mu_sq - dist²(P,probe)) / (mu_sq - mu² + ε))
  ```
  donde `mu, mu²` vienen del depth moments map en la dirección `P - probe`.
- Probe relocation: si un probe cae dentro de un mesh (occluded), se offsetea heurísticamente hacia fuera.
- Probe classification: probes 100% indoor/outdoor/inactive se marcan para skippear update rays.
- **Diferencia clave con Lumen**: DDGI es uniform grid, no persigue cámara con clipmap cascades (requiere clipmap manual por game), y no tiene "screen probes" — todo es world-space. Lumen gana en densidad cerca de cámara y en fidelity de detalle. DDGI gana en simplicidad + trivially consistent across space.

### 9.2 ReSTIR GI (Ouyang et al. 2021, HPG)

- Extensión de ReSTIR (Bitterli 2020) de direct lighting a indirect GI.
- Idea: cada pixel mantiene un **reservoir** (stream-sampled path sample) con weight = importance del path. Temporal reuse: reservoir del frame anterior se combina con el nuevo en proporción ponderada. Spatial reuse: vecinos en el mismo frame comparten reservoirs.
- Resampling Importance Sampling (RIS): sobre un pool grande de samples, se elige 1 con probability ∝ contribution. Mejor varianza que uniform sampling.
- Resultados: 9×-166× MSE improvement vs 1SPP path tracing baseline.
- **vs Lumen**: ReSTIR GI es *puro* path tracing + resampling; necesita RT hardware por obligación (trace real por pixel). Lumen cachea radiance (surface cache + probes) para amortizar el trace. ReSTIR gana cuando quieres fidelity cinematográfica (Cyberpunk Path Tracing mode); Lumen gana por perf en consoles y hybrid pipeline.
- Se puede combinar: UE5 5.5 Reflections usa una variante ReSTIR-ish temporal resampling para reflection rays.

### 9.3 Radiance Cascades (Sannikov 2023 / 2024)

Desarrollado para Path of Exile 2 (ExileCon 2023) — Alexander Sannikov. Observación:

- Light que proviene de lejos tiene alta **angular resolution** pero baja **spatial resolution** (una luz lejana afecta uniformemente un área grande).
- Light cercana es lo inverso: baja angular (muchas direcciones diferentes ven la misma luz) pero alta spatial (cambia rápido pixel-a-pixel).
- Estructura: N cascades. Cascade 0 = alta spatial res (probe cada píxel), baja angular res (8-16 direcciones, short rays). Cascade N = baja spatial res (probe cada 2^N px), alta angular res (8^N direcciones, longer rays).
- Cada cascade i cubre `[r_i, r_{i+1}]` range de radiance. Final integration = merge cascadas (bilinear filter entre niveles → GPU gives esto gratis).
- Pros: **cero temporal accumulation** → zero latency (crítico en juegos ARPG, menos motion blur artifact). 2D: ~12 ms constant cost scene-independent.
- Cons: original paper es 2D. 3D implementation (PoE2) usa cubemap-like angular layout pero overhead ~50 ms en scene complejas, ~2× Lumen en PS5.
- **vs Lumen**: RC es más elegante conceptualmente, zero temporal artifacts. Lumen gana throughput actual gracias a Surface Cache + screen space optimizations, y a que Epic lleva 4 años tuneándolo en escenas reales.

### 9.4 Voxel Cone Tracing / VXGI / SVOGI (Crassin 2011)

- Escena voxelizada (regular grid o Sparse Voxel Octree) → mipmap 3D de direct lighting.
- Cone trace: para cada pixel, lanzar 4-6 "cones" (un cone = sequence of mipmapped sphere samples). Cada cone integra radiance + occlusion.
- Pros: totalmente dinámico, manejable en GPU.
- Cons: **light leaking** inevitable en geometría fina (thin walls), voxel aliasing (flicker al mover luz).
- NVIDIA VXGI: versión productionizada para UE4 plugin. SVOGI: CryEngine's version (sparse voxel octree para no pagar RAM en aire vacío).
- **vs Lumen**: VCT inspiró Lumen. Epic explícitamente dijo "we started with VCT and kept hitting leaking issues, moved to distance fields + surface cache" (Narkowicz 2022). Lumen "far field voxel scene" es esencialmente un VCT simplificado para fallback far-field.

### 9.5 Light Propagation Volumes (Kaplanyan 2009, CryEngine 3)

- Escena voxelizada en grid 32³, cada cell guarda irradiance en SH bands (4 coeffs = 1 band o 9 = 2 bands).
- Direct lighting se inyecta a las cells del frame. Propagation step: iterativamente "difunde" radiance entre cells vecinas con un stencil SH propagation operator.
- Pros: dynamic lighting a tiempo real (~1-2 ms), barato.
- Cons: 1 rebote único; light leaking masivo en geometría fina; resolution limita detail.
- Legacy — superado en calidad por todo lo de 2019+. Mantengo por contexto histórico.

### 9.6 Spatial Hash Radiance Cache (NVIDIA 2024, RTXGI v2)

- Reemplaza el grid de DDGI por un **spatial hash table** GPU-side, keyed por `hash(worldPos / cellSize, normal)`.
- Collision resolution linear probing. Cells allocated sólo donde hay geometría visible → ahorro RAM masivo vs grid denso.
- Update via path-traced rays + RIS.
- Sirve como backend de Neural Radiance Cache (NRC) — una MLP tiny aprende a predecir radiance desde el hash features.
- **vs Lumen**: más similar al WRC de Lumen (sparse, visibility-driven) pero con hash en lugar de clipmap y con MLP opcional. Requiere RT hardware y tensor cores para la MLP.

### 9.7 Tabla comparativa

| Técnica           | Dynamic geom? | Bounces | Moving lights | GPU cost (1080p) | Hardware req         | ALZE GL 3.3 feasibility  |
|-------------------|---------------|---------|---------------|------------------|----------------------|--------------------------|
| DDGI              | Yes (slow)    | ∞ (feedback) | Yes     | 1-3 ms (RT)      | RT hardware req      | No (needs RT)            |
| Lumen SW          | Yes           | ∞ (surf cache) | Yes  | 3-5 ms           | GPU compute + SDF gen | Parcial: costoso SDF gen |
| Lumen HW (SC)     | Yes           | ∞       | Yes           | 2-4 ms           | DXR/VK_RT            | No                       |
| Lumen HW (HitLt)  | Yes           | ∞       | Yes           | 8-12 ms          | DXR + SER ideal      | No                       |
| Radiance Cascades | Yes           | 1-2     | Yes (zero-lag)| 12-15 ms (2D)    | GPU compute          | Parcial: 2D viable       |
| VXGI / SVOGI      | Yes (slow)    | 1-2     | Yes           | 5-8 ms           | GPU compute          | Parcial: voxelize en CS |
| LPV               | Yes           | 1       | Yes           | 1-2 ms           | GPU compute          | Sí (GL 3.3 compute... no, 4.3) |
| Light Maps        | No            | ∞ baked | No            | ~0 ms runtime    | CPU bake             | Sí (gold standard v1)    |
| ReSTIR GI         | Yes           | ∞       | Yes           | 4-8 ms           | RT hardware          | No                       |
| SH Probes Manual  | Partial       | ∞ baked | No            | <1 ms            | None                 | Sí (v1 realistic)        |

## 10. ALZE applicability

ALZE es un motor de 1 dev, C++17, GL 3.3, ~25-30K LOC. Hay que ser cruel con el scope. Tres horizontes:

### 10.1 v1 — OpenGL 3.3, hoy

Hardware baseline: intel UHD / GTX 1050 / SteamDeck. NO compute shaders (GL 3.3 no los tiene — desde 4.3). NO SSBO dinámico. NO image load/store. Esto descarta casi toda la técnica moderna.

**Feasible v1:**

1. **Lightmap baking offline** via un tool C++ propio (ray-traced en CPU con embree o tinybvh, output a 2K atlas per level). Es lo que se usó 2005-2015; sigue siendo "por defecto" para motores pequeños serios.
2. **SH irradiance probes manuales** (~9 floats/probe per 3 bands, RGB = 27 floats/probe). Artist places probes en el editor. Runtime: trilinear interpolation del más cercano octet. Static GI, no dynamic.
3. **Sky light SH** (1 probe para el sky dome) → ambient component cheap.
4. **Single directional light CSM** con 3 cascadas 2K², PCF 3×3. Suficiente para outdoor scenes.
5. **SSAO** (HBAO+ o GTAO variant) en un fragment shader post-pass. Es el único "dynamic GI approximation" razonable sin compute.
6. **Screen-Space Reflections** fragment shader para specular cheap. Usable pero con artifacts visibles fuera de pantalla.

**No feasible v1:** cualquier cosa que requiera sphere-trace 3D SDFs (necesita compute + atomics), surface cache updates (necesita MRT image store), radiance cache world probes (necesita SSBOs + compute).

### 10.2 v2 — Vulkan + compute (horizonte 1-2 años)

Una vez ALZE sube a Vulkan 1.2 + compute shaders + SSBOs + timeline semaphores, el universo se abre:

**Feasible v2:**

1. **DDGI lite**: 1 grid uniforme 32×16×32 probes (16K probes), 8×8 octahedral irradiance, 64 rays/probe updated en CS. Rays trazados contra un **scene BVH pre-built en CPU** (embree export → GPU buffer). O contra un **global SDF** construido una vez al cargar el level (no per-frame). ~3-5 ms/frame.
2. **Lumen surface cache lite**: cada mesh → 6 axis-aligned cards 256². Atlas global 8K². Update budget 512×512 texels/frame. NO dynamic SDF (static scene only) — usar BVH pre-built. Permite **dynamic lights en scene static**.
3. **Voxel Cone Tracing lite**: un single 128³ voxel volume, revoxelized cada N frames con el current lighting, 4 diffuse cones + 1 specular cone por pixel. Suficiente para indoor scenes pequeñas-medias. Leaking es aceptable si los assets se diseñan con grosor ≥ 1 voxel.
4. **Radiance Cascades 2D** para una "top-down" strategy/ARPG perspective (si algún día ALZE se usa para isométrico/top-down). Sannikov's original target. Presupuesto ~12 ms en hardware decente.
5. **VSM-lite**: un virtual shadow map de 8K² con 128×128 pages, physical pool 2K×2K (256 pages). Sin Nanite (no hay), se usa hw rasterizer con indirect draws y frustum cull per-page en CS. Target: 1 directional + 8 spot shadowed. ~2-4 ms.
6. **Screen Space Radiance Cache** sin world cache: screen probes a 1/16 res con bilateral upsample. Usado como denoiser + amortization sobre SSR/SSGI. ~1-2 ms.

**Difícil aún en v2:** hardware ray tracing (VK_KHR_ray_query exige RT cores; no todo hardware Vulkan los tiene — Intel UHD NO), Nanite (sistema entero es inviable sin 10K LOC dedicadas).

### 10.3 v3 — aspirational con RT hardware

Si en v3 (años) ALZE asume baseline RTX 3060 / PS5 / Xbox Series, y tienes tiempo/LOC:

**Aspirational v3:**

1. **Full Lumen-class SW** con surface cache + screen probes + WRC + GDF sphere tracing. Es un proyecto de 3-5 KLOC rendering + 2 KLOC tooling, probablemente >1 año de 1 dev.
2. **Hardware RT diffuse + reflections** via `VK_KHR_ray_query` inline desde compute. BVH build online. Surface cache cached lighting at hits. Mucho más simple de implementar que SW Lumen (el BVH es given).
3. **VSM full** con per-page caching, invalidation tracking, integrado con un mesh shader pipeline opcional.
4. **ReSTIR DI** para direct lighting (más impactful que GI): maneja 1000s de luces sin shadow maps per-light, sólo BVH shadow rays + reservoir resampling. ~3-5 ms.
5. **Neural Radiance Cache (NRC)** si se quiere investigar territory: MLP tiny 64-64-3 que aprende a predecir irradiance desde `hash(worldPos, normal, viewDir)` features. Entrenamiento online durante gameplay. ~2 KLOC + CUDA/Vulkan ML.

### 10.4 Tabla ALZE

| Componente                          | v1 (GL 3.3) | v2 (VK+compute) | v3 (VK+RT hw) | Notas                                 |
|-------------------------------------|-------------|-----------------|---------------|---------------------------------------|
| Lightmap bake offline               | Sí          | Sí              | Sí            | Base line sea cual sea                |
| Static SH probes (placed)           | Sí          | Sí              | Sí            | Cheap, artist-driven                   |
| CSM directional 3 cascades          | Sí          | Sí              | Reemplazar VSM | 2K² per cascade, PCF 3×3              |
| SSAO (GTAO)                         | Sí          | Sí              | Sí            | 0.5-1 ms, siempre worth it            |
| SSR                                 | Sí          | Sí              | +RT fallback  | 1-2 ms post pass                       |
| DDGI-lite grid 32³ @ 64 rays/probe  | No          | Sí              | Sí (mejor)    | Requiere compute + BVH/SDF             |
| Surface cache + mesh cards          | No          | Parcial (static)| Sí            | Atlas update es el bottleneck          |
| Global SDF + sphere trace           | No          | Sí (static)     | Sí (dynamic)  | SDF gen requiere compute; CPU tool v1 |
| Voxel Cone Tracing 128³             | No          | Sí              | Legacy        | Leaking es real; assets design around  |
| Radiance Cascades 2D                | No          | Sí              | Sí            | Top-down/iso games only                |
| Radiance Cascades 3D                | No          | Parcial         | Sí            | ~2× Lumen cost, experimental           |
| Screen probes + final gather        | No          | Sí (simple)     | Sí (full)     | Bilateral upsample crítico             |
| World radiance cache (probes)       | No          | Parcial         | Sí            | Clipmap grid; RT hw lo hace trivial    |
| ReSTIR DI (direct light)            | No          | No              | Sí            | Solo con RT hw; high impact/cost       |
| ReSTIR GI (indirect)                | No          | No              | Sí            | Full path tracing territory            |
| VSM-lite (8K²)                      | No          | Sí              | Sí            | Sin Nanite, con hw rasterizer          |
| VSM + Nanite fast path              | No          | No              | No (no Nanite) | ALZE no va a implementar Nanite        |
| Hit Lighting                        | No          | No              | Opcional      | Solo cinematics; 5× costo              |
| Neural Radiance Cache               | No          | No              | Aspirational  | ML infra significativa                 |

**Realismo para 1 dev:** v1 = lightmap bake + SH probes + CSM + SSAO + SSR. v2 aspiration = añadir DDGI-lite + VSM-lite. v3 = nunca Nanite, pero sí HW RT inline + Surface-cache-style hit lookup + ReSTIR DI es alcanzable en 6-12 meses de focused work si el resto del motor está sólido.

**La trampa a evitar**: intentar Lumen "completo" en v2 sin medir carefully qué parte da valor. Mejor implementar sólo screen probes + SSGI filtering + SDF soft shadows que intentar el stack entero y no terminar nada. Epic tiene 20+ engineers en Lumen. ALZE tiene 1.

## 11. Anti-patterns observados

1. **Re-evaluar material en cada hit**. Surface cache es exactamente lo que lo evita. Si implementas RT GI, cachea radiance una vez por surface patch; no evalúes shaders en hit.
2. **Uniform probe grid denso**. Grids 64³ a 1 m spacing = 250K probes = overkill. Usa clipmap cascades o sparse hash.
3. **Temporal accumulation sin clamp/reject**. Da ghosting terrible. Siempre bbox clamp sobre vecindario + disocclusion detection por depth/normal divergence.
4. **Shadow maps per luz**. A partir de 4-5 luces sombreadas, es unsustainable. VSM atlas compartido o stochastic shadow is the only way.
5. **Shader permutations explotadas por feature flags**. Lumen UE5 tiene ~10K permutations solo por sí misma. Para ALZE: compile flag budget < 2^10. Usar dynamic uniform branching sobre specialization constants.
6. **Ignorar PSO precache**. Shader stutter es el defecto UE por excelencia. Al cargar level, scanner materiales visibles + draws dummy fuera de pantalla para forzar compile. Crítico aunque el motor sea chico.
7. **Lightmap bake como oldschool**. No lo es. Unreal lo sigue ofreciendo en mobile path. Para ALZE v1 es la decisión correcta.

## 12. Fuentes consultadas

### Primarias

- **Wright, D., Narkowicz, K., Kelly, P. — "Lumen: Real-time Global Illumination in Unreal Engine 5"** — SIGGRAPH 2022 Advances in Real-Time Rendering course. [PDF (tamaño excede WebFetch cache)](https://advances.realtimerendering.com/s2022/SIGGRAPH2022-Advances-Lumen-Wright%20et%20al.pdf). Curso index: [advances.realtimerendering.com/s2022](https://advances.realtimerendering.com/s2022/index.html).
- **Narkowicz, K. — "Journey to Lumen"** (2022-08-18) blog. [knarkowicz.wordpress.com/2022/08/18/journey-to-lumen](https://knarkowicz.wordpress.com/2022/08/18/journey-to-lumen/). History detallada del desarrollo, pivot heightfields → distance fields.
- **Epic Games Documentation — "Lumen Technical Details"** (UE 5.7). [dev.epicgames.com/documentation/en-us/unreal-engine/lumen-technical-details-in-unreal-engine](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-technical-details-in-unreal-engine). Nota: primary URL retornó 403 durante WebFetch directo; contenido cubierto vía WebSearch abstract + UWA blog reimplementation analysis.
- **Epic Games Documentation — "Lumen Global Illumination and Reflections"** + **"Lumen Performance Guide"**. Similar 403; cubierto indirectamente.
- **Epic Games Documentation — "Virtual Shadow Maps"** (UE 5.7). [dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine). 403 directo.
- **Epic Games Tech Blog — "Virtual Shadow Maps in Fortnite Battle Royale Chapter 4"** (2023). [unrealengine.com/en-US/tech-blog/virtual-shadow-maps-in-fortnite-battle-royale-chapter-4](https://www.unrealengine.com/en-US/tech-blog/virtual-shadow-maps-in-fortnite-battle-royale-chapter-4). 403 directo.
- **Majercik, Z., Guertin, J-P., Nowrouzezahrai, D., McGuire, M. — "Dynamic Diffuse Global Illumination with Ray-Traced Irradiance Fields"** — JCGT vol 8 no 2 (2019). [jcgt.org/published/0008/02/01](https://jcgt.org/published/0008/02/01/). PDF: [jcgt.org/published/0008/02/01/paper-lowres.pdf](https://www.jcgt.org/published/0008/02/01/paper-lowres.pdf).
- **McGuire, M. — "Dynamic Diffuse Global Illumination"** (blog series overview). [morgan3d.github.io/articles/2019-04-01-ddgi](https://morgan3d.github.io/articles/2019-04-01-ddgi/).
- **Majercik, Z. et al. — "Scaling Probe-Based Real-Time Dynamic Global Illumination for Production"** — JCGT vol 10 no 2 (2021). [jcgt.org/published/0010/02/01/paper-lowres.pdf](https://jcgt.org/published/0010/02/01/paper-lowres.pdf). Follow-up DDGI production notes.
- **Ouyang, Y., Liu, S., Kettunen, M., Pharr, M., Pantaleoni, J. — "ReSTIR GI: Path Resampling for Real-Time Path Tracing"** — HPG 2021 / Computer Graphics Forum vol 40. [onlinelibrary.wiley.com/doi/abs/10.1111/cgf.14378](https://onlinelibrary.wiley.com/doi/abs/10.1111/cgf.14378). NVIDIA landing: [research.nvidia.com/publication/2021-06_restir-gi-path-resampling-real-time-path-tracing](https://research.nvidia.com/publication/2021-06_restir-gi-path-resampling-real-time-path-tracing).
- **Bitterli, B. et al. — "Spatiotemporal Reservoir Resampling for Real-Time Ray Tracing with Dynamic Direct Lighting"** — SIGGRAPH 2020. [research.nvidia.com/sites/default/files/pubs/2020-07_Spatiotemporal-reservoir-resampling/ReSTIR.pdf](https://research.nvidia.com/sites/default/files/pubs/2020-07_Spatiotemporal-reservoir-resampling/ReSTIR.pdf). ReSTIR foundation paper.
- **Crassin, C., Neyret, F., Sainz, M., Green, S., Eisemann, E. — "Interactive Indirect Illumination Using Voxel Cone Tracing"** — Computer Graphics Forum (Pacific Graphics) 2011. [onlinelibrary.wiley.com/doi/abs/10.1111/j.1467-8659.2011.02063.x](https://onlinelibrary.wiley.com/doi/abs/10.1111/j.1467-8659.2011.02063.x). PDF at [research.nvidia.com/sites/default/files/pubs/2011-09_Interactive-Indirect-Illumination/GIVoxels-pg2011-authors.pdf](https://research.nvidia.com/sites/default/files/pubs/2011-09_Interactive-Indirect-Illumination/GIVoxels-pg2011-authors.pdf).
- **Sannikov, A. — "Radiance Cascades: A Novel Approach to Calculating Global Illumination"** — ExileCon 2023 + paper draft 2024. Resumen en [80.lv/articles/radiance-cascades-new-approach-to-calculating-global-illumination](https://80.lv/articles/radiance-cascades-new-approach-to-calculating-global-illumination). 3D demo [tmpvar.com/poc/radiance-cascades-3d](https://tmpvar.com/poc/radiance-cascades-3d/). Tutorial [jason.today/rc](https://jason.today/rc).
- **Osborne, J., Sannikov, A. — "Holographic Radiance Cascades for 2D Global Illumination"** — arXiv 2505.02041 (2025). [arxiv.org/abs/2505.02041](https://arxiv.org/abs/2505.02041). Follow-up sobre penumbra artifacts.
- **NVIDIA RTXGI v2 / RTXGI-DDGI** — Algorithms docs. [github.com/NVIDIAGameWorks/RTXGI-DDGI/blob/main/docs/Algorithms.md](https://github.com/NVIDIAGameWorks/RTXGI-DDGI/blob/main/docs/Algorithms.md). Parámetros default 288 rays/probe, classification, relocation.
- **Kaplanyan, A. — "Light Propagation Volumes in CryEngine 3"** — SIGGRAPH 2009. [advances.realtimerendering.com/s2009/Light_Propagation_Volumes.pdf](https://www.advances.realtimerendering.com/s2009/Light_Propagation_Volumes.pdf).

### Secundarias / implementation analysis

- **UWA Blog — "UE5 Lumen Implementation Analysis"** (2022). [blog.en.uwa4d.com/2022/01/25/ue5-lumen-implementation-analysis](https://blog.en.uwa4d.com/2022/01/25/ue5-lumen-implementation-analysis/). Análisis estructura-por-estructura con resoluciones y parámetros.
- **ShawnTSH1229 — "SimLumen"** reimplementation open source en MiniEngine. [github.com/ShawnTSH1229/SimLumen](https://github.com/ShawnTSH1229/SimLumen). Útil para ver el shader code reducido.
- **Stephano, J. — "Sparse Virtual Shadow Maps"** (StratusGFX blog). [ktstephano.github.io/rendering/stratusgfx/svsm](https://ktstephano.github.io/rendering/stratusgfx/svsm). Implementación detallada step-by-step algo que Epic docs no abren tanto.
- **Pernäs, J. — "Real-Time Global Illumination in Unreal Engine 5"** — Masaryk Univ 2023 BSc thesis. [is.muni.cz/th/n1qq4/real-time_GI_in_UE5.pdf](https://is.muni.cz/th/n1qq4/real-time_GI_in_UE5.pdf).
- **Looman, T. — "Unreal Engine 5.6 Performance Highlights"** (2025). [tomlooman.com/unreal-engine-5-6-performance-highlights](https://tomlooman.com/unreal-engine-5-6-performance-highlights/). Numbers update.
- **NVIDIA UE5.4 Raytracing Guideline v5.4** (2024). [dlss.download.nvidia.com/uebinarypackages/Documentation/UE5+Raytracing+Guideline+v5.4.pdf](https://dlss.download.nvidia.com/uebinarypackages/Documentation/UE5+Raytracing+Guideline+v5.4.pdf). Practical UE5 HWRT tuning.
- **GPUOpen — "GI-1.0: A Fast Scalable Two-Level Radiance Caching Scheme for Real-Time Global Illumination"** — AMD 2022. [gpuopen.com/download/publications/GPUOpen2022_GI1_0.pdf](https://gpuopen.com/download/publications/GPUOpen2022_GI1_0.pdf). AMD FidelityFX alternative approach.
- **GPUOpen — "World-Space Spatiotemporal Reservoir Reuse for Ray-Traced Global Illumination"** — SA 2021. [gpuopen.com/download/SA2021_WorldSpace_ReSTIR.pdf](https://gpuopen.com/download/SA2021_WorldSpace_ReSTIR.pdf). World-space ReSTIR variant.
- **Sachdeva, S. — "Spatiotemporal Reservoir Resampling (ReSTIR) — Theory and Basic Implementation"**. [gamehacker1999.github.io/posts/restir](https://gamehacker1999.github.io/posts/restir/). Tutorial accesible.

### Notas de acceso

- Epic documentation URLs `dev.epicgames.com/documentation/...` retornaron 403 a WebFetch directo en varias request. Contenido extraído vía WebSearch abstract + fuentes de terceros (UWA blog, forum threads, SimLumen source, Medium reimplementations).
- Lumen SIGGRAPH 2022 PDF excedió el límite de tamaño de WebFetch (10 MB content length). Cobertura vía Narkowicz "Journey to Lumen" blog (mismo autor, summarized) + abstract searches + UWA implementation analysis.
- web.archive.org no es fetchable desde este entorno.
