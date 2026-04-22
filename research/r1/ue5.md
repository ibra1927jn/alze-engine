# Unreal Engine 5 — Investigación técnica (para ALZE Engine)

Fecha: 2026-04-21. Target engine: UE 5.4/5.5, con referencias a 5.7 docs.
Consumidor: ALZE Engine (C++17 no-RTTI/no-exceptions, SDL2 + OpenGL 3.3, ~25-30K LOC, Fase 7).

## Overview

UE5 es la quinta iteración mayor del motor de Epic Games, con linaje directo desde Unreal Engine 1 (1998, Tim Sweeney escribió a mano la primera VM de UnrealScript) → UE2 (Unreal 2, Splinter Cell) → UE3 (Gears, masificación de deferred shading) → UE4 (2014, PBR de Brian Karis, Blueprint como sucesor de UnrealScript) → UE5 (2022, release oficial con 5.0). La transición 4→5 no es incremental: introduce **Nanite** (geometría virtualizada), **Lumen** (GI dinámica), **Virtual Shadow Maps**, **World Partition** (streaming automático), **TSR** (upscaling propio), **Mass Entity** (ECS data-oriented), **PCG** (procedural content generation), **Chaos** como físico por defecto (sustituye PhysX), y **Verse** como lenguaje nuevo (vía UEFN).

Licencia: gratuita hasta USD 1M de revenue por producto; después 5% royalty (excepto Fortnite/UEFN que paga a creators vía Creator Economy). Source disponible en GitHub para licenciatarios. Ships con: **Fortnite** (el titán que co-desarrolla el motor), **Immortals of Aveum** (Ascendant, 2023, primer AAA nativo UE5), **Black Myth: Wukong** (Game Science, 2024), **Senua's Saga: Hellblade II**, **Robocop: Rogue City**, **Stalker 2**, **The Finals** (Embark). Hay críticas serias a la performance en muchos de estos títulos (ver "En qué falla").

## Rendering pipeline

**Nanite — geometría virtualizada.** Pipeline clásico GPU asume ≤1 triangle/pixel y escala mal con millones de triángulos. Nanite rompe eso. Mecanismo:

- El mesh se preprocesa offline a **clusters** de ~128 triángulos, agrupados en **cluster groups** (~32 clusters) formando un **DAG** multinivel. Cada nivel colapsa triángulos con edge-collapse preservando bordes compartidos.
- Runtime: selección de LOD **per-cluster-group** en GPU (compute shader) según error de pantalla proyectado. El DAG garantiza LODs sin cracks entre grupos vecinos porque los edges se preservan.
- **Rasterización dual**: clusters cuyos triángulos son ~1px o menores van a un **software rasterizer** (compute shader, scanline emisor a imagen atómica con InterlockedMax empaquetando depth+visibility ID); clusters grandes van al rasterizer hardware. El software path es hasta **3× más rápido** para micro-triángulos porque el hw rasterizer desperdicia ciclos en quads de 2×2 casi vacíos.
- **Visibility buffer**: en vez de escribir GBuffer completo, cada pixel guarda un ID de 64 bits (cluster ID + triangle ID + depth). Material pass posterior: shader screen-space que, por cada material único, procesa solo los pixeles de ese material (classification via tile histogram), reconstruye atributos (UV, normal) desde el visibility ID y evalúa el material HLSL. Desacopla el coste del material del coste de rasterizar.

**Lumen — GI dinámica.** Two-level: near field (SDF/HW RT) + far field (voxel scene). Estructuras:

- **Surface Cache**: cada mesh tiene un atlas 2D de "cards" (proyecciones axis-aligned con heightfield + albedo + normal + irradiance). Cuando un rayo golpea, se samplea el atlas en vez de re-evaluar material. Es un cache 2D que vive en GPU y se actualiza incrementalmente.
- **Software ray tracing**: Mesh Distance Fields per-mesh + Global Distance Field escena-wide. Trace rápido sin RT hardware.
- **Hardware ray tracing**: BVH de DXR/Vulkan RT si el GPU lo soporta; mejor calidad (geometría exacta) con igual o mejor perf en ciertas escenas.
- **Final gather con radiance caching**: screen-space probes (una probe cada 16×16 pixeles aprox) + world-space radiance cache (probes persistentes en grid 3D alrededor de la cámara, con octahedral encoding y spatial/temporal filtering). Se lanzan pocos rayos por probe (~32-128) pero se amortizan por muchos pixels.
- **Reflection path** separado: screen-space → Lumen reflections → glossy path con surface cache lookup.

**Virtual Shadow Maps (VSM)**. Una sola "shadow map" virtual de 16k × 16k (hasta 64k² con clipmaps direccionales) que solo existe como **page table**: solo las tiles de 128×128 que cubren pixels visibles se rasterizan. Page cache persiste entre frames — si geometría estática no cambió y la luz no se movió, se reutiliza. Integra con Nanite: el rasterizer software puede escribir directo a las páginas. Reemplaza el viejo sistema de cascaded + spotlight shadow maps con calidad uniforme cerca/lejos. Invalidaciones agresivas cuando el sol rota (time-of-day games sufren).

**TSR vs DLSS vs FSR**. Las tres atacan el mismo problema (renderizar a resolución baja y reconstruir a alta con info temporal). Diferencias:

- **TSR** (propietario Epic, cross-platform): puro shader HLSL, sin red neuronal, heurísticas refinadas con motion vectors + disocclusion detection + anti-flicker. Integración profunda con el HZB y los motion vectors del engine.
- **DLSS** (NVIDIA): red neuronal en Tensor cores; calidad superior en subpixel detail, pero solo RTX.
- **FSR** (AMD, abierto): hasta FSR 2 era puro shader spatial-temporal, FSR 3+ agrega frame generation. Cross-GPU.

TSR gana por no requerir hardware específico ni plugins, pero no iguala DLSS 3 en escenas de alto detalle.

**Material system**: HLSL node graph en editor → Material Translator genera HLSL → **cross-compilation** (vía HLSLcc / Mesa patches) a GLSL/MSL/SPIR-V según plataforma. Cada combinación (material × vertex factory × feature level × shading model × quality × lighting setup) genera una **permutation**. Render path principal: **base pass deferred** (Lumen + GBuffer reducido) + **forward path** (mobile, VR). Hay también un **path tracer** mode para validación/archviz (reference ground truth, no tiempo real).

## Scene + entity model

**Actor/Component (legacy)**. UE4 hereda del modelo OO clásico: `AActor` con árbol de `UActorComponent`. Todo es `UObject` con reflection via macros (`UCLASS`, `UPROPERTY`, `UFUNCTION`). GC automático. Escala mal a > 10k actores por la sobrecarga de UObject (vtable, GC rooting, replication metadata).

**Mass Entity** (plugin experimental→production). ECS **archetype-based** al estilo EnTT/flecs:

- Entity = int32 index + generation (handle). Cero datos.
- **Fragments** (components sin lógica) stored en SoA por archetype. Un archetype agrupa todas las entidades con idéntico set de fragments en chunks contiguos (cache-friendly).
- **Tags**: fragments vacíos para partitioning.
- **Processors** (systems): iteran queries (por fragments requeridos). Ejecutan en task graph paralelizable.
- Bridges a Actors cuando hace falta (LOD: lejos = mass entity con vertex anim mesh; cerca = actor MetaHuman full rigged).
- Usado en tech demo Matrix Awakens (City Sample): decenas de miles de NPC + tráfico a 30 FPS en PS5.

**World Partition**. Reemplaza streaming manual (LevelStreaming de UE4). Workflow:

- **One File Per Actor (OFPA)**: cada actor se guarda en su propio .uasset, no en el .umap del level. Evita conflicts de merge.
- **Runtime grid**: spatial hash 2D (default) o 3D. Actores se asignan a cells según su bounds + grid settings en World Settings.
- **Cook**: cada cell → uno o más **streaming levels** generados automáticamente. Combinaciones de data layers activas en un cell generan levels adicionales.
- **Data Layers**: agrupaciones ortogonales (ej. "day" / "night" / "destroyed_state") que activan/desactivan subsets de actores a runtime.
- **HLOD** (Hierarchical LOD): mesh-merge automático de cells lejanos en proxies low-poly.

**Level Instances**: instanciación de levels completos como si fueran prefabs, con overrides per-instance (material swap, property override) que bakean al cook final. Packed level instance = baked static mesh merge al cook para perf.

## Asset pipeline

**Cooking**: transforma .uasset (editor-friendly) → formato runtime per-plataforma. Convierte texturas a BC/ASTC/ETC, compila shaders, strip de editor-only data, consolida bulk data en .pak o IoStore. Puede tomar horas para AAA.

**Shader compilation**: el verdadero cuello de botella. UE compila **decenas a cientos de miles de permutaciones** por proyecto (material × vertex factory × feature level × debug view mode × static switch...). Distribuible vía **XGE / FASTBuild / SN-DBS**. PSO (Pipeline State Object) precaching en 5.2+ reduce el stutter in-game (el motor precompila pipelines probables en background vía **PSO Precache** basado en componentes cargados).

**Derived Data Cache (DDC)**. Content-addressable cache de datos "derivados" de fuentes (compiled shaders, texture mips, navmesh, Nanite-built mesh, Lumen card data). Keys = hash del source + parámetros + version. Jerárquico: local (boot) → shared team (NFS/S3) → cloud Epic. Evita recomputar en cada artist. UnrealCloudDDC es el servicio Epic.

**Asset reference graph**: UObject references implícitas vía property reflection. El package system resuelve carga lazy: cargas un uasset → traverse referencias → incluye dependencias. Circular refs permitidas vía lazy/soft object pointers.

**Hot reload**: recompilación incremental de módulos C++ en editor. Notoriamente frágil; Live Coding (basado en Live++) lo reemplaza y es más robusto pero aún no perfecto.

## Threading + performance

**Task Graph** (TaskGraph API → reemplazado progresivamente por **Tasks API** moderna en 5.x). Tasks con dependencias explícitas corren en named threads (GameThread, RenderingThread, RHIThread, AudioThread) + worker pool. Write-once semantics: lecturas concurrentes OK, escritura serializada por ownership.

**Parallel rendering**: GameThread → (1 frame de latencia) → RenderingThread genera RHI command list → RHI thread traduce a API nativa (D3D12/Vulkan/Metal) → GPU. Desde 5.5, **parallel translation** divide RHI translation en múltiples workers (hasta 2× speedup).

**Unreal Insights**: profiler con trace file format (`.utrace`). Captura CPU timeline, GPU markers, memory, loadtime, networking. Complemento: stat commands in-game (`stat unit`, `stat gpu`, `stat scenerendering`).

**Memory tagging (LLM — Low Level Memory Tracker)**: marca cada allocation con un tag jerárquico (rendering/textures/nanite/lumen...) para atribuir memoria por subsistema sin recompilar.

## Scripting / gameplay

**Blueprint VM**. Visual scripting compilado a **bytecode** interpretado por una VM heredada de UnrealScript (Sweeney 1998, iterada). Cada nodo = opcode o llamada a función nativa. Overhead ~10× vs C++ para hot loops, aceptable para high-level gameplay. Nativization (convertir BP → C++ cooked) fue removida en UE5 — ahora la estrategia es: usar BP para composición, C++ para lógica pesada.

**C++ con reflection**: `UCLASS`/`USTRUCT`/`UPROPERTY`/`UFUNCTION` macros procesadas por **UnrealHeaderTool** (UHT) pre-compile generan `.generated.h` con metadata. Habilita: GC tracing, serialización, replication, editor inspector, BP exposure.

**UObject GC**: mark-and-sweep. Cada `UObject` en un root set (hard refs) + roots explícitos. Sweep cada ~60s (configurable) con incremental reachability analysis. **UPROPERTY** en members es lo que el GC traza; raw pointers a UObject son inseguros. Weak refs vía `TWeakObjectPtr`.

**Verse** (Epic, debutó en UEFN 2023). Lenguaje funcional-lógico con **transactional memory** (AutoRTFM). Features: failure como control flow, deterministic concurrency, strong static types. En UEFN compila al Blueprint VM. Plan largo plazo: Verse como lenguaje gameplay de UE6 (integración con C++ ya en progreso, incluye que el GC y los mallocs de UE soporten semántica transaccional). Por ahora solo vive dentro de Fortnite/UEFN.

**Motion Matching** (5.4+). Sistema de animación nuevo: a runtime, por cada frame, busca en una base de datos de clips el **frame** cuya pose + velocity + trajectory (past + future) más se acerca a la demanda del gameplay (input + predicted trajectory). Reemplaza blend trees manuales. **Game Animation Sample** (GASP) ships con 500+ animations libres.

## En qué es bueno

- **Nanite + Lumen son genuinamente revolucionarios** — no hay equivalente comercial open-source. Eliminan el budget de LOD manual y el bake de lightmaps; ambos eran costes de producción gigantes en AAA.
- **Tooling maduro**: editor completo (level, material, blueprint, anim, VFX, sequencer, UMG, audio), Sequencer para cinemáticas rivalizando con DCC tools, MetaHumans pipeline, PCG visual.
- **Ecosistema**: Marketplace (ahora Fab) con miles de assets, tutoriales, cursos, Epic MegaGrants, documentación masiva.
- **Source disponible**: el motor completo es inspeccionable/modificable. Para un motor propio, es un manual gigante.
- **Scale**: World Partition + HLOD + Mass + Lumen + Nanite juntos permiten mundos de kilómetros cuadrados con densidad alta sin cargas visibles (Matrix Awakens, Fortnite BR).

## En qué falla

- **Shader compilation stutter** — el defecto más infame de UE4/5. Sin PSO precache bien configurado, el juego hace hitches al encontrar un material nuevo en una luz nueva. Stalker 2, Wukong, The Finals y muchos UE5 titles salieron con este problema. Epic añadió PSO precache en 5.2+ pero no lo arregla solo.
- **Build times** absurdos: proyectos AAA con cambio en un header core → 20-40 min recompile. DDC shared ayuda pero no elimina.
- **Monolithic editor**: arrancar el editor con un proyecto mediano toma minutos. Plugin system existe pero el core engine es muy tight-coupled.
- **Blueprint wall**: proyectos BP-heavy escalan mal. Cuando el equipo se da cuenta que necesita perf C++, portar BP complejos a C++ cuesta meses (los BP no exponen fácil su control flow).
- **5% royalty** encima de USD 1M. Para hit games es decenas de millones. Muchos estudios AA lo evitan migrando a Unity/Godot/custom.
- **Audio + Networking** están por detrás del resto (Iris en experimental hace años, audio graph limitado).
- **Peso**: editor + engine + source + DDC local puede pedir 150-300 GB por proyecto.

## Qué podríamos copiar para ALZE Engine

ALZE es C++17 no-RTTI/no-exceptions, SDL2+GL3.3, ~25-30K LOC. No hay manera de clonar Nanite/Lumen completos. Pero hay mecanismos aislables:

1. **Visibility-buffer style rendering** para modelos high-poly. En vez de MRT GBuffer tradicional, escribir **32-bit triangle ID + depth** en un solo RT durante la geometry pass; en compute (o fullscreen pass) reconstruir atributos desde el triangle ID (index buffer lookup con texture buffer) y evaluar el material. Ahorra ancho de banda, se clasifica por material en un tile histogram. Implementable en GL3.3 vía integer texture + SSBO bindless-like (texture buffer).
2. **Surface Cache idea para GI sin RT**. Por cada mesh, un atlas 2D pre-bakeado (albedo + emisión + normal proyectada) en 6 direcciones. Indirect lighting: voxel-cone-trace una global voxel texture (ej. 128³) que se actualiza al inyectar irradiance del surface cache bajo la luz directa actual. Dinámico en cuanto a luces, estático en cuanto a geometría — un sweet spot para engines pequeños.
3. **Archetype ECS al estilo Mass** para entities sin overhead: fragments en SoA por archetype, chunks de ~16KB, processors que queryean. Evita el problema de "todo hereda de GameObject". Para ALZE esto es sustituto del legacy Actor model en zonas data-heavy (partículas de simulación, NPCs, proyectiles).
4. **DDC-style content-hash cache** para assets derivados: BlobStore keyed por `hash(source_bytes + params + version_tag)`. Shaders compilados, texturas comprimidas, meshes procesados. Local dir primero, opcional dir compartido (NFS/SMB). Reduce cold-start de ~min a ~s si el equipo comparte cache.
5. **PSO precache ligero**: al cargar un nivel, scannear meshes + materiales visibles y emitir draws dummy (offscreen 1×1 RT) para que el driver compile los pipelines antes del primer frame real. Suficiente para eliminar 90% del stutter inicial.
6. **VSM-lite**: una shadow map virtual de 4k-8k con page table de tiles 128×128 y rasterización incremental (solo tiles cuyo bound entre el frustum). Estático cached, dinámico invalidado por frame. Sin Nanite, igual reduce el coste de shadows muy lejanas.
7. **Task graph estilo Fibers/Tasks API**: API simple con `Task<T>` + dependencias explícitas + worker pool. Separar GameThread / RenderThread / RHI-equivalent thread. Latencia 1 frame entre game y render, pero paralelización real.
8. **LLM memory tagging**: macro `ALZE_ALLOC_TAG("rendering/textures")` por scope que marca allocations al tag jerárquico. Debug view de memoria por subsistema casi gratis.
9. **Material translator muy simple**: un material = grafo JSON + pequeño lenguaje (expresiones). Compilar a GLSL con cross-compile manual. No llegar a Nanite's HLSL→*, solo evitar shader hand-written per material.
10. **World Partition spatial hash**: grid 2D de cells de 32-128m con streaming async de chunks (.alze packs) según camera AABB. Simple pero cubre 80% del valor: mundos >1km sin hitches.

## Qué NO copiar

- **Shader permutation explosion**. UE llega a 100k+ permutations per project, porque cada feature está matrix-crossed con todas las demás. Un motor pequeño debe tener **flag budget estricto**: < 2^12 permutations totales. Preferir uniform branching o specialization constants sobre #ifdef.
- **Blueprint VM interpretada para hot paths**. Si algún día ALZE expone scripting, compilar a bytecode está bien, pero mantener ruta C++ directa para simulación. No repetir la trampa de "gameplay 100% BP scale".
- **Monolithic editor acoplado al engine**. ALZE debe separar runtime (shippable binary mínimo) de toolchain (editor, importers) para que títulos shipped sean < 50 MB engine footprint.
- **UObject reflection system** con GC mark-and-sweep. Es poderoso pero costoso: UHT pre-compile, vtable bloat, GC stalls. Para C++17 sin RTTI, preferir reflection manual con `constexpr` + descriptor structs o una librería ligera como refl-cpp/rttr.
- **Actor tree + ComponentTick por-frame** de TickGroups complejos. Mejor ECS con passes explícitos (Simulation → Physics → Animation → Render).
- **Todo-es-UPROPERTY**. Exportar reflection solo donde se necesita (serialización, editor inspector, scripting bridge); C++ "puro" para el resto.
- **5% royalty mentality**: no acoplar el motor a un store/marketplace propietario; mantener ALZE MIT/Apache.

## Fuentes consultadas

- [Understanding Nanite — UE5's virtualized geometry (Epic blog)](https://www.unrealengine.com/en-US/blog/understanding-nanite---unreal-engine-5-s-new-virtualized-geometry-system)
- [Karis, "A Deep Dive into Nanite" SIGGRAPH 2021 (PDF)](https://advances.realtimerendering.com/s2021/Karis_Nanite_SIGGRAPH_Advances_2021_final.pdf)
- [Notes on UE5 Nanite — Nikos Papadopoulos](https://www.4rknova.com/blog/2021/06/09/unreal-5-nanite)
- [Wright et al., "Lumen: Real-time GI in UE5" SIGGRAPH 2022 (PDF)](https://advances.realtimerendering.com/s2022/SIGGRAPH2022-Advances-Lumen-Wright%20et%20al.pdf)
- [Journey to Lumen — Krzysztof Narkowicz](https://knarkowicz.wordpress.com/2022/08/18/journey-to-lumen/)
- [Lumen Technical Details (Epic docs)](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-technical-details-in-unreal-engine)
- [Virtual Shadow Maps (Epic docs)](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine)
- [Virtual Shadow Maps in Fortnite Chapter 4 (Epic blog)](https://www.unrealengine.com/en-US/tech-blog/virtual-shadow-maps-in-fortnite-battle-royale-chapter-4)
- [Sparse Virtual Shadow Maps — J Stephano](https://ktstephano.github.io/rendering/stratusgfx/svsm)
- [TSR vs DLSS vs FSR analysis — Oreate AI](https://www.oreateai.com/blog/indepth-analysis-of-unreal-engine-5-super-resolution-technology-a-comparative-study-of-dlss-3-fsr-2-and-tsr/bcc281d65b6101ac644fca54fa2ecd88)
- [Mass Entity (Epic docs)](https://dev.epicgames.com/documentation/en-us/unreal-engine/mass-entity-in-unreal-engine)
- [Large Numbers of Entities with Mass (State of Unreal 2022)](https://dev.epicgames.com/community/learning/talks-and-demos/37Oz/large-numbers-of-entities-with-mass-in-unreal-engine-5)
- [Mass Sample (Megafunk, GitHub)](https://github.com/Megafunk/MassSample)
- [World Partition (Epic docs)](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine)
- [Unreal World Partition Internals — Sam Bloomberg](https://xbloom.io/2025/10/24/unreals-world-partition-internals/)
- [Procedural Content Generation Overview (Epic docs)](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-overview)
- [Shader stuttering: UE's solution (Epic blog)](https://www.unrealengine.com/tech-blog/game-engines-and-shader-stuttering-unreal-engines-solution-to-the-problem)
- [Using Derived Data Cache (Epic docs)](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-derived-data-cache-in-unreal-engine)
- [Parallel Rendering Overview (Epic docs)](https://dev.epicgames.com/documentation/en-us/unreal-engine/parallel-rendering-overview-for-unreal-engine)
- [UE5.5 Performance Highlights — Tom Looman](https://tomlooman.com/unreal-engine-5-5-performance-highlights/)
- [unreal_source_explained: threads (donaldwuid, GitHub)](https://github.com/donaldwuid/unreal_source_explained/blob/master/main/thread.md)
- [Iris Replication System (Epic docs)](https://dev.epicgames.com/documentation/en-us/unreal-engine/iris-replication-system-in-unreal-engine)
- [Iris: 100 Players in One Place — BorMor](https://bormor.dev/posts/iris-one-hundred-players/)
- [Blueprint VM performance — Intax](https://intaxwashere.github.io/blueprint-performance/)
- [Bringing Verse Transactional Memory Semantics to C++ (Epic blog)](https://www.unrealengine.com/en-US/tech-blog/bringing-verse-transactional-memory-semantics-to-c)
- [Motion Matching in UE (Epic docs)](https://dev.epicgames.com/documentation/en-us/unreal-engine/motion-matching-in-unreal-engine)
- [Chaos Scene Queries and Rigid Body Engine (Epic blog)](https://www.unrealengine.com/en-US/tech-blog/chaos-scene-queries-and-rigid-body-engine-in-ue5)
- [Niagara System and Emitter Module Reference (Epic docs)](https://dev.epicgames.com/documentation/en-us/unreal-engine/system-and-emitter-module-reference-for-niagara-effects-in-unreal-engine)
- [Unreal Engine 5 (Wikipedia)](https://en.wikipedia.org/wiki/Unreal_Engine_5)

Notas de acceso: tres URLs devolvieron 403 durante WebFetch (Epic stutter blog, Karis Nanite PDF, World Partition internals Bloomberg) y dos PDFs SIGGRAPH excedieron el límite de tamaño. El contenido se cubrió vía WebSearch abstract + fuentes alternas.
