# Nanite — Deep Dive (ALZE r3)

Fecha: 2026-04-22. Target de consumo: `/root/repos/alze-engine` (C++17 no-RTTI/no-exceptions, SDL2+GL 3.3 hoy, Vulkan mañana, ~25-30K LOC, single-dev).
Este documento NO repite los párrafos de superficie ya escritos en `/root/lab_journal/research/alze_engine/ue5.md` (líneas 14-19, 120). Los extiende con los números concretos, bit-layouts, pseudocódigo de passes, y deltas entre versiones. Se asume que el lector ya sabe qué es "geometría virtualizada" a alto nivel.

## Exec summary (5 líneas)

1. Nanite = (cluster-DAG offline) + (per-view cluster selection en GPU) + (software rasterizer compute para micro-triángulos ≤ 32-256 px bbox) + (visibility buffer de 64 bits) + (material pass diferido tile-classified). El "secreto" no es una sola cosa: es la integración.
2. Números clave: **128 tri/cluster**, **8 children/nodo** en DAG, **128 KB/página** streaming, **~50% decimation por nivel**, threshold de **~32 px** para software raster (algunas fuentes dicen 256 px bounding box), **R32G32_UINT** visibility buffer con 25 bits cluster + 7 bits triangle + 32 bits depth.
3. Clonar **todo** Nanite es irrealista para single-dev (Karis+equipo invirtieron múltiples años). Subsets aislados son viables: cluster rendering, visibility buffer, two-pass HZB culling — cada uno es 1-3 meses de dev serio sobre GL 4.6 + compute.
4. Dependencias duras: **64-bit image atomics** (ARB_shader_image_load_store + NV_shader_atomic_int64 o Vulkan 1.2 core) para el software raster estilo Nanite; **mesh shaders** (Vulkan 1.3 + VK_EXT_mesh_shader o DX12 Ultimate) para la ruta HW de meshlets sin emulación. GL 3.3 actual de ALZE **no** soporta nada de esto sin emulación pesada.
5. Competencia viva: Bevy 0.14→0.16 (jms55), Unity `MeshLODGroup` (sigue siendo LOD discreto), meshoptimizer (Kapoulkine, librería donde todos construyen su DAG), Frostbite GeometryFX (Wihlidal 2016, sólo cluster-cull no virtualized). Decima y Snowdrop tienen cluster culling GPU-driven pero no virtualized geometry pública.

---

## 1. Algoritmo: construcción del DAG, selección de LOD per-view, decisión HW vs SW raster

### 1.1 Construcción del cluster DAG (offline / build time)

Entrada: malla con N triángulos (puede ser centenares de millones). Salida: un DAG estratificado por niveles de LOD, cada nodo = cluster de ≤128 triángulos, aristas = "este cluster de nivel L se forma al decimar este *grupo* de clusters de nivel L-1".

Pipeline offline (Karis 2021, confirmado por Bevy y candid startup writeup):

1. **Meshlet formation inicial (LOD 0)**. Partición del mesh original en clusters de **128 triángulos / 384 vértices máx** (los números son múltiplos de 32/64 por razones de wavefront GPU: 32 en NV, 64 en AMD GCN; SM de 128 threads les sienta como guante). La heurística estándar en meshoptimizer es `meshopt_buildMeshlets(max_vertices=64, max_triangles=124-128, cone_weight=0.5)`. Para Nanite/Bevy la métrica de calidad cambia: en vez de **maximizar** reuso de vértices entre triángulos de un meshlet (buen para culling/cache), se **minimiza** el número de vértices en el borde compartido entre meshlets (buen para simplificación posterior). jms55 en Bevy 0.16 reemplazó `meshopt_buildMeshlets` con partición basada en METIS justamente por esto: "DAG quality is the most important part of virtual geometry".

2. **Agrupación de clusters para decimar (group formation)**. Se toman los meshlets LOD L, se construye un grafo de adyacencia donde nodos = meshlets y aristas = "comparten ≥1 edge topológico". Se parte el grafo con METIS/k-way en grupos de **~8 clusters** cada uno (el branching factor fijo del DAG es 8 en Nanite). Bevy usa grupos de ~4 en 0.14, subió hacia 8 en 0.16.

3. **Decimación conjunta del grupo**. Los ~8 clusters del grupo se "funden" conceptualmente — triángulos + índices + atributos — y se simplifica como **una sola malla** usando **quadric error metric (Garland-Heckbert, 1997)**: `meshopt_simplifyWithAttributes()` o `meshopt_simplify()` con factor objetivo ~50% del triangle count original. Crítico: **los edges compartidos con grupos vecinos se lockean** (se marcan como no-collapsables). Esto garantiza que el borde exterior del grupo es idéntico en todos los niveles del DAG, y por tanto **no hay cracks** entre grupos adyacentes aunque uno esté en LOD 3 y el vecino en LOD 5. Este es el ingrediente anti-crack que hace que Nanite LOD sea seamless.

4. **Re-meshletization del grupo simplificado**. Los triángulos simplificados (aprox. 50% del original) se vuelven a partir en ~4 clusters nuevos de hasta 128 tri. Estos clusters son los "padres" en el DAG de los 8 clusters originales del grupo.

5. **Repetir** hasta que quede 1 cluster (o pocos) en la raíz. Niveles típicos: 10-25 para meshes grandes (1M+ tri).

6. **Quadric error per cluster** → se almacena como **bounding sphere + radius de error proyectado** por cada cluster. Se usa runtime para la selección LOD.

**Resultado**: un DAG multinivel, no un árbol estricto (un cluster puede ser padre de múltiples grupos si varios grupos vecinos se decimaron conjuntamente en un nivel superior). La propiedad importante: cualquier "corte" consistente del DAG (un frente que cruza todas las ramas exactamente una vez) produce una tessellation válida y sin cracks.

Pseudocódigo build (siempiendo Karis + Bevy 0.16 + meshoptimizer):

```cpp
// Offline — per mesh
std::vector<Cluster> lod0 = build_meshlets_metis(mesh, 128);
all_clusters = lod0;
current = lod0;
while (current.size() > ROOT_CLUSTER_BUDGET) {
    auto groups = metis_partition(current, GROUP_SIZE=8, edge_weight=shared_verts);
    std::vector<Cluster> next_lod;
    for (auto& group : groups) {
        Mesh fused = fuse(group);
        lock_boundary_edges(fused, group);         // anti-crack
        float target_tris = fused.tri_count() * 0.5f;
        Mesh simplified = meshopt_simplify_with_attr(fused, target_tris);
        float error = compute_quadric_error_radius(fused, simplified);
        auto children_clusters = build_meshlets(simplified, 128);
        for (auto& c : children_clusters) {
            c.parent_error = error;
            c.children_ids = group.cluster_ids();
        }
        next_lod.insert(next_lod.end(), children_clusters.begin(), children_clusters.end());
    }
    all_clusters.insert(all_clusters.end(), next_lod.begin(), next_lod.end());
    current = next_lod;
}
build_bvh(all_clusters); // per-mesh BVH para early reject
pack_to_pages(all_clusters, 128 * 1024);
```

Tiempos build reales: en Bevy 0.14, "muy lento" (varias horas para meshes millonarios). Epic no ha publicado cifras concretas pero la herramienta Nanite Tools ha mejorado entre 5.0 y 5.5 (paralelización METIS + cacheo).

### 1.2 Selección de LOD per-view (runtime GPU)

Por cada vista (main camera, cada cascade de shadow, cada cube face de point shadow, etc.) se ejecuta un **cluster hierarchy traversal** en compute. El criterio por cluster:

```
parent_error_projected_pixels = project_sphere_to_screen(cluster.parent_error_sphere, view)
if (parent_error_projected_pixels < PIXEL_THRESHOLD)
    draw this cluster (the parent is good enough)
else
    descend to children (need more detail)
```

`PIXEL_THRESHOLD` típico es ~1 pixel. Como la métrica es monotónica en la jerarquía (error_hijo < error_padre), el test se puede evaluar **sin recursión** en paralelo: cada cluster sabe si él mismo debe dibujarse o si hay que bajar más. Específicamente la condición es:

> dibujo el cluster C si: `project(C.parent_error) >= threshold` AND `project(C.self_error) < threshold`.

Eso produce exactamente una capa consistente del DAG sin necesidad de sincronización. La paralelización se hace con **persistent threads** + work-stealing queue: un fixed número de grupos de hilos (p.ej. 64 workgroups de 64 threads) persistentes durante toda la fase de culling, consumiendo un queue GPU-atómico de nodos a visitar.

### 1.3 HW vs SW rasterizer — heurística de ruta

Para cada cluster superviviente al culling, por cada triángulo se calcula su **screen-space bounding box**. Si el bbox es:
- "pequeño" (fuentes discrepan: Karis slides dicen ~32 pixels de largo mayor; candid startup analysis dice "<= 256 pixels total area"; el consenso útil: **largo mayor ≲ 4-16 px** es software-path ganador),
- **completamente dentro** de la pantalla (no necesita clipping — el SW raster no implementa clipping por perf),

entonces va al **software rasterizer compute shader**. El resto va por hardware draw indirect (1 draw por chunk de clusters usando `DrawIndexedIndirect` o mesh shaders en 5.3+).

Razón técnica por la que el SW raster gana en micro-triángulos: el HW raster scheduler trabaja en **2x2 quads**. Un triángulo de 1-2 píxeles activa un quad donde 3 de 4 lanes son "helper pixels" (sólo sirven para derivatives, no emiten output). Eficiencia real: 25%. Además el HW hace setup fijo del triángulo (edge equations, interpolantes) que para 1 triángulo de 2 px es todo overhead. Karis reporta **~3× speedup** del SW path vs HW en micro-geometría dominante. En el frame ejemplo del candid startup analysis: 3333 instances HW + ~34821 workgroups SW → SW maneja **>90% de la geometría**.

### 1.4 Software rasterizer — cómo funciona

Compute shader con un workgroup por cluster (≤128 threads, uno por triángulo). Cada thread:

1. Transform vértices del triángulo (WVP) → clip space → pantalla.
2. Computa edge equations (3 half-plane tests).
3. Itera bounding box escaneando pixels (scanline).
4. Para cada pixel dentro del triángulo, computa depth.
5. Empaqueta `(depth << 32) | visibility_id` como uint64.
6. `imageAtomicMax(visibility_target, pixel, packed)` — el Max sobre uint64 con depth en la MSB equivale a depth test "less-equal" GL-style si invertimos bits, o "greater-equal" con reversed-Z. Nanite usa **reversed-Z** (common en engines modernos para mejor precisión lejana), así el Max funciona directo.

Esta técnica depende crítica de **ImageAtomicMax sobre uint64**. En Vulkan: extensión `VK_KHR_shader_atomic_int64` + image type adecuado (`R64_UINT`). En DX12: `InterlockedMax` con `RWTexture2D<uint64_t>` via `ShaderModel 6.6`. En GL: `NV_shader_atomic_int64` (propietario) o emulación con 2x uint32 (slow). Vulkan 1.2 core mandatoria la extensión. **Sin este primitive el SW path de Nanite no es viable**; por eso Bevy 0.14 no lo tenía y 0.16 lo agregó con storage texture R64Uint.

---

## 2. Estructuras de datos: disco y memoria

### 2.1 Cluster pages (128 KB)

Unidad de streaming = página de **128 KB**. Empaqueta N clusters que comparten **localidad espacial** + **nivel similar del DAG** (si descargas un nivel entero, no sirve si la página mezcla leaf y root). Por página: header + lista de clusters con:

- Índices (quantized, delta-encoded against cluster-local strip order).
- Vértices (posiciones cuantizadas a bits variables por-cluster según el error admisible; normales como octahedral 16-bit; UVs 16/32-bit).
- Bounding sphere + quadric-error radius.
- Parent/children IDs.
- Material slot (4-8 bits típicamente).

Compresión observada en Nanite: ~20-28 bytes/triángulo (vs. ~96 B/tri en formato trivial). Para un mesh de 1M triángulos en disco: ~25 MB. Con las 128 KB páginas eso son ~200 páginas, que soportan streaming granular.

### 2.2 Pool en VRAM

La **resident pool** es un gran buffer circular en VRAM, típicamente **256-1024 MB** configurable (`r.Nanite.Streaming.StreamingPoolSize` en UE, default 512 MB). Root pages (el tope del DAG para cada mesh) son **always-resident** — garantizan que cualquier mesh en la escena tiene al menos un cluster dibujable aunque el streaming no haya cargado nada más.

Streaming request: cada frame el shader de selección LOD emite a un buffer de "requested pages" las páginas que quiere. CPU lee ese buffer (asíncrono, con 1-2 frames de latencia), encola IO async (direct storage en PS5/Xbox Series / DirectStorage en PC), descarga pagina al pool. Eviction por LRU cuando la pool se llena.

### 2.3 BVH per-mesh + scene BVH

- **Per-mesh BVH**: construido offline. Nodos internos agrupan clusters, early-reject frustum/occlusion culling. Stored en la página root del mesh.
- **Scene BVH / spatial hash**: Nanite mantiene una estructura ligera para iterar instances visibles; combina con GPU Scene (buffer de todos los instance transforms + metadata) y persistent culling.

### 2.4 Visibility buffer

Formato **R32G32_UINT** (64 bits por pixel, no MRT — un solo target):
- `R[31:7]` = **ClusterID** (25 bits → 33M clusters direccionables)
- `R[6:0]` = **TriangleID dentro del cluster** (7 bits → hasta 128 triángulos/cluster, exactamente el límite)
- `G[31:0]` = **Depth** (32 bits, reversed-Z)

Total: 64 bits que sirven tanto como depth buffer (para HZB subsequent) como para reconstrucción de atributos en material pass. Ahorro vs GBuffer tradicional (3-4 RT × 32-64 bits): **3-6×** en ancho de banda de geometry pass.

---

## 3. Rasterización — visibility buffer, material resolve, two-pass HZB culling

### 3.1 Two-pass occlusion culling (Nanite main pass)

Basado directo en Haar+Aaltonen 2015 "GPU-Driven Rendering Pipelines" (Assassin's Creed Unity). Nanite lo escaló y le añadió el cluster DAG.

**Pass 1 (occluder pass)**:
1. Reusar el HZB del **frame anterior** (`HZB_prev`) como oráculo de oclusión aproximada.
2. Instance culling: para cada instance en GPU Scene, test frustum + test HZB_prev de la bbox → lista de "instances posiblemente visibles".
3. Cluster selection (persistent threads): recorre DAG per-instance con el pixel-error criterion, escribe clusters "candidatos" a un buffer.
4. Cluster culling: frustum + HZB_prev por cluster → lista de clusters a rasterizar.
5. Rasterize HW + SW al visibility buffer (empty inicialmente).
6. Build `HZB_current` desde el depth recién escrito (min/max reduction en mipchain).

**Pass 2 (post-pass / second cull)**:
7. Re-test todos los clusters que fueron **descartados** en pass 1 por HZB_prev pero que con el HZB_current nuevo podrían ser visibles (esto captura disocclusions — algo revelado por cambio de cámara o animación).
8. Rasterizar supervivientes encima del visibility buffer.
9. Final HZB para uso en shadow passes + SSAO + Lumen.

Resultado: zero false negatives (nunca se oculta algo que debía verse), false positives bajos. El HZB level "óptimo" para test (donde la bbox cubre ≤16 pixels en ese nivel del HZB) se elige per-bbox.

### 3.2 Material resolve (compute, tile-classified)

El visibility buffer no tiene atributos. La fase de materials los reconstruye:

1. **Classify materials**: pass que va tile por tile (tile típico 20×12 = 240 threads, pensado para wave 32/64), escanea el visibility buffer, identifica qué material IDs aparecen → produce el **Material Range Texture** (R32G32_UINT): por tile, lista de material IDs presentes.
2. **Emit GBuffer / Material evaluation**: **un fullscreen quad por material único**, pero con **early depth/stencil rejection** configurado para descartar tiles donde ese material no está presente (usa el material range texture + un trick de depth: se escribe un "material depth" específico por material que hace que el early-Z reject haga el fan-out/fan-in). Cada pixel dentro de un tile relevante lee el clusterID+triID desde visibility, hace **fetch de atributos** (busca el cluster en la cache de vértices, interpola con barycentrics recomputados), y evalúa el material HLSL.

Beneficio: cada material cubre sólo los pixels que realmente le tocan, **sin branches por-pixel** en un über-shader. Desacopla el coste del material del coste de rasterizar la geometría — el material se ejecuta screen-space, por lo que geometría duplicada ("overdraw geométrico") no cuesta material evaluations extra.

En **UE 5.5/5.6**, Epic mueve incluso la evaluación de material a un **compute pass puro** ("Nanite Compute-Based Shading"), eliminando los fullscreen draws. Aún en beta.

### 3.3 Interacción con Virtual Shadow Maps

El SW rasterizer escribe directo a **páginas** de la VSM (no al VSM completo — el VSM es virtual). Page allocation: el HZB de cámara principal se usa para decidir qué páginas (tiles de 128×128 texels en un VSM 16k-64k²) son requeridas. Se rasteriza solo esas. Page cache inter-frame: si geometría y luz no cambiaron, reusa. Un VSM invalidación agresiva por rotación de sol → el coste sube notablemente en juegos con time-of-day dinámico (Stalker 2, Fortnite BR chapter 4).

---

## 4. HW vs SW raster — thresholds precisos y claims vs reality

| Criterio | Valor típico Nanite | Notas |
|---|---|---|
| Largo-mayor bbox en pantalla | ≤ ~32 px → SW | Karis 2021 slide, revisado en 5.x |
| Área bbox | ≤ 256 px² (variante) | candid startup analysis |
| Dentro de pantalla 100% | Requerido para SW | Sin clipping en SW path |
| Triángulos por cluster | ≤ 128 | Match con 7-bit triangle ID |
| Lanes activas por wave en HW (micro-tri) | ~25% | Por el quad overhead |
| Speedup SW vs HW en micro-tri | ~3× | Claim Epic, confirmado Bevy |
| % de triángulos por SW path en frame típico | 80-95% | City Sample, Valley of Ancient |

Claim vs reality: en **geometría non-micro** (arquitectura large-tri, personajes mid-poly), el HW path sigue siendo superior porque tiene interpolación/derivatives "gratis". El milagro de Nanite es que **ambos paths coexisten** con la misma output format, así nunca hay que elegir: la heurística per-triángulo lo resuelve. Lo malo: el SW path tiene ceiling — si tu escena es toda mid-poly (no hay micro-tri), Nanite es **más lento** que un pipeline tradicional simple porque paga el overhead de cluster culling sin obtener el speedup del SW path. Por eso Epic recomienda desactivar Nanite en foliage/hair hasta 5.5+ (donde los cases improved).

---

## 5. Limitaciones

### 5.1 Originales (UE 5.0)

- **Sólo static meshes opaque**. No skeletal, no cloth, no dynamic deformation.
- **No translucency**. El visibility buffer sólo guarda 1 layer. Materiales translucent se rasterizan con el pipeline forward tradicional.
- **No tessellation / displacement**. Micromesh inexistente.
- **No decals-on-Nanite** de forma trivial (cubierto por deferred decals en el material pass, pero con limitaciones de orden).
- **No two-sided materials** bien soportado (llegó más tarde).
- **No mask/alpha-test** (llegó en 5.1-5.2).

### 5.2 UE 5.3 — Tessellation/Displacement experimental

5.3 introdujo **Nanite Tessellation** (experimental): heightmap como input, el mesh se tessella adaptivamente por cluster. Conocido por **bugs de sombras** (especialmente con VSM: "stupid shadow artifacts on nanite landscape" hilos en polycount), **break en packaged builds** (meshes faltantes o partially-black), y GPU timeout en NaniteSplit con point-lights sombreados + tessellation. Muchos usuarios reportan que activar `r.Nanite.Tessellation=1` en 5.3 **dropea el framerate ~40%** en escenas abiertas.

### 5.3 UE 5.4 — Mejoras

Displacement de Beta → stable-ish, integración con Nanite Foliage (trees/shrubs). Fix de la mayoría de bugs 5.3, aunque los artifacts VSM persisten con foliage denso: "ShadowDepth tanking fps with Nanite/VSM foliage UE 5.3" seguía vigente en 5.4 para algunos users.

### 5.4 UE 5.5 — Skeletal + fallback bugs

**Nanite Skeletal Mesh (Beta)** — `r.Nanite.AllowSkinnedMeshes=1`. El skinning se hace en un compute pre-pass que escribe posiciones animadas a un buffer temporal, luego Nanite opera sobre ese buffer como si fuera static por ese frame. Limitaciones:
- La **Nanite LOD generation** en SkeletalMeshBuilder estaba aplicando siempre el LOD más bajo del skeletal mesh como fuente para Nanite build, así que la versión Nanite del skeletal **parecía ultra-low-poly desde ciertos ángulos** — bug reportado en múltiples foros Epic Dev Community.
- Performance: skinning compute + Nanite pipeline es notablemente más caro que static Nanite. Solo justifica si el character es high-poly (>50k tri) y se ve in-view durante mucho tiempo.
- "Nanite Displacement + Tessellation now works on Skeletal meshes" — nuevo 5.5, pero comparte los bugs anteriores.

**Fallback-bug 5.5**: "All Nanite meshes sometimes revert to fallback mesh in 5.5" — bajo carga GPU alta o ciertos combos de settings, Nanite se desactiva silenciosamente en un frame y salen los fallback meshes (los low-poly que Nanite guarda para collision/hardware ray-tracing/lumen-HWRT). Reportado y parcheado parcialmente en 5.5.3+.

### 5.5 Lo que **no** funciona bien todavía (5.5)

- **Performance en foliage muy denso** con VSM cache invalidations (19 FPS reports).
- **Two-sided materials + Nanite Tessellation + ray-traced shadows**: combinación que produce artifacts raros.
- **Custom primitive data / per-instance parameters**: limitados vs material system en non-Nanite path.
- **Overhead base alto**: si la escena es small-poly, Nanite **pierde** contra el pipeline forward tradicional.
- **Stutter al primer frame de un asset**: cluster page streaming + HZB warmup + PSO compile. Epic lo mitiga con warm-up scenes en 5.4+.

---

## 6. Competidores / tecnología similar

### 6.1 Meshoptimizer (Arseny Kapoulkine) — la librería base de todos

No es un renderer, es una **librería C++ header-lite** que da las primitivas que todos usan para construir sus Nanite-likes:
- `meshopt_buildMeshlets()` — partición a meshlets (default max_vertices=64, max_triangles=124, con cone_weight para cone culling).
- `meshopt_simplify()` + `meshopt_simplifyWithAttributes()` — Garland-Heckbert quadric error, optionally attribute-aware.
- `meshopt_simplifySparse()` — variante eficiente para subsets (p.ej. un único cluster de un mesh enorme).
- `meshopt_partitionClusters()` — partición espacial+topológica para agrupar meshlets (competencia de METIS para el grouping step del DAG).
- `meshopt_optimizeVertexCache`, `meshopt_optimizeOverdraw`, encoders de index/vertex para GPU.
La calidad de su simplify + su velocidad son la razón por la que Bevy, Tellusim, granite, y hobbyists lo usan en lugar de escribir QEM desde cero. MIT license. Repo: https://github.com/zeux/meshoptimizer.

### 6.2 Unity Mesh LOD Group — no es virtualized

Unity sigue con el modelo clásico: artista autora LOD0/LOD1/LOD2/LOD3 discretos, LOD Group switchea entre ellos por screen size. **No hay clusterización ni seamless transition**. Unity HDRP 2023-2026 no ha anunciado Nanite-equivalent. Hay **soluciones community**: "Seamless Virtual Geometry" (ZEngineStudios, free), "Massive Geometry Rendering" en Unity Asset Store, proyectos open-source como `unity-meshlet-rendering`. La lentitud oficial de Unity en este espacio es referenciada como "la mayor ventaja que UE5 tiene sobre Unity" en foros.

### 6.3 Bevy meshlet renderer (jms55 — Jasmine Shtarkman)

Estado actual (0.16, marzo 2026):
- Partición inicial via **meshoptimizer** + **METIS** para DAG quality.
- Two-pass HZB occlusion culling — ✅ implementado desde 0.14.
- LOD hierarchy (DAG) — ✅ 0.14+, con mejoras de calidad continuas 0.15 → 0.16 (partición por METIS para minimize-shared-vertex).
- Software rasterizer — ✅ 0.16, via **storage texture R64Uint** y 64-bit atomics (requiere GPU + wgpu feature `SHADER_INT64_ATOMIC_ALL_OPS` o similar).
- Streaming — ❌ no aún (en 0.15 cambiaron la data layout para futuro streaming, pero el feature no está).
- BVH per-mesh — ❌ bottleneck declarado, jms55 "burned out before finishing BVH-based culling".
- Mesh shaders — ❌ wgpu no los soporta aún; ruta actual usa draw indirect.
- Material pass compute — parcial; falta fragment shader abstraction completa.
Restricciones: opaque, non-deforming. Gap vs Nanite: sin streaming es un LOD-hierarchy-en-GPU sin virtualization real de disk-to-GPU. Blog series: https://jms55.github.io/ (3 posts principales 2024-06, 2024-11, 2025-03). PRs: #10164 initial, #16947 DAG quality, etc.

### 6.4 Frostbite GeometryFX / Wihlidal GDC 2016

**"Optimizing the Graphics Pipeline with Compute"** (Graham Wihlidal, GDC 2016) — la **pre-history** de Nanite. No tiene DAG ni LOD hierarchy. Hace:
- Cluster culling GPU: frustum + backface (via cone) + small-triangle rejection + orientation + HiZ depth.
- Index buffer compaction post-cull.
- Multi-draw indirect.
- Integrado en Frostbite + liberado como GeometryFX en AMD GPUOpen.
- Software rasterizer para **occluders** (no para geometry general) — generado CPU-side para el frame que viene (una técnica que Frostbite tiene desde Battlefield 3).
Es la base sobre la que Nanite construyó: ambas pipelines fusionan cluster culling + GPU-driven drawing, pero Wihlidal se detuvo en micro-optimization del HW pipe, Karis dió el salto al SW raster + DAG-LOD.

### 6.5 Haar + Aaltonen, SIGGRAPH 2015 — "GPU-Driven Rendering Pipelines"

Ubisoft Montreal + RedLynx, basado en Assassin's Creed Unity:
- Meshlets de ~64 triángulos.
- Cluster culling GPU: frustum + occlusion (HiZ) + triangle backface en compute.
- Two-pass occlusion culling — **este es el paper que introdujo el patrón usado por Nanite**.
- Depth pyramid generation (HiZ) en compute.
- MultiDrawIndirect con cluster index buffer compacting.
No hay DAG, no hay SW raster, no hay visibility buffer. Pero toda la infraestructura GPU-driven (indirect dispatch, persistent threads, HZB-for-cull) viene de aquí y Karis cita este trabajo directamente.

### 6.6 Snowdrop (Ubisoft Massive) — cluster culling GPU-driven

The Division / Division 2 / Star Wars Outlaws. Snowdrop tiene:
- Deferred shading + tiled light culling desde 2013.
- Cluster culling GPU-driven (compute-based) en el pipeline de geometry.
- Async compute heavy.
- **No virtualized geometry pública** — sigue un modelo LOD discreto con cluster-based culling.
GDC 2019 "Efficient Rendering in The Division 2" (Sébastien Hillaire et al.) detalla el pipeline; es "Frostbite-style" upgraded, no Nanite-style.

### 6.7 Decima (Guerrilla Games) — virtual geometry parcial

Horizon Forbidden West + Death Stranding 2 en Decima 2022+. El SIGGRAPH 2022 Real-Time Live! showcase (Malan + van der Gaag) mostró:
- Virtualized textures (desde Killzone).
- Foliage instancing masivo GPU-driven.
- **Geometry clustering + GPU culling** al estilo Frostbite-2015.
- No se confirmó públicamente un DAG-LOD tipo Nanite. Lo que Decima hace es cluster-cull + quadtree LOD selection + asset streaming por chunks.
Decima es más cercano a Snowdrop/Frostbite que a Nanite. El término "shards" aparece en el contexto de **terrain shards** (streaming units, no meshlets).

### 6.8 Granite (Maister / Hans-Kristian Arntzen) — hobbyist Nanite-like

Implementación Vulkan de geometry pipelines modernos. Blog "Modernizing Granite's mesh rendering" (2024) discute:
- Ruta primary compute software rasterizer con 64-bit atomics.
- Fallback mesh shader path donde un solo primitive ID se exporta como per-primitive varying y el fragment shader hace el atomic.
- Restricción: sin 64-bit atomics (mobile, Metal) software raster queda limited a depth-only.
Referencia útil para ver Nanite-style implementado fuera de UE, en Vulkan puro.

### 6.9 Tellusim Compute Raster

Tellusim engine (Alexander Zaprjagaev, commercial) publicó artículo "Compute versus Hardware" que compara los dos paths. Similar stack a Nanite: compute raster para <~12-16 px, HW raster arriba. Engine comercial pero con blog técnico detallado.

### 6.10 Nanite WebGPU (Scthe, open source)

https://github.com/Scthe/nanite-webgpu — implementación Nanite-like en WebGPU. Meshlet LOD hierarchy, software rasterizer, billboard impostors, culling per-instance y per-meshlet. Buen referente para ver el alcance mínimo viable.

### 6.11 Micro-Mesh (NVIDIA, 2022)

Extensión de hardware Ada (RTX 4000+) que permite subdividir un base triangle en **micro-triangles** (hasta 64 per base) con displacement comprimido, evaluados en hardware durante rasterización y ray-tracing. Es la aproximación de NVIDIA al mismo problema (micro-geometry scaling): hardware, no software. Estado: specific a RTX 4000+, poca adopción fuera de demos. Complementario a Nanite, no competidor directo.

---

## 7. Implementación en ALZE — qué se puede, qué no

### 7.1 Qué es realmente viable en GL 3.3 (state actual)

**Casi nada nativamente.** GL 3.3 no tiene:
- Compute shaders (llegó en 4.3).
- Image atomics 64-bit.
- Indirect dispatch (llegó en 4.3).
- SSBO (llegó en 4.3).
- Bindless textures.
- Mesh shaders (llegó en 4.5 + NV extension, no core).

Lo único posible en 3.3 stock: un **cluster renderer sin virtualization**, con LOD discreto, cluster-cull CPU-side + `glMultiDrawElementsIndirect` (que sí es 4.3). Esto no es Nanite, es más bien GeometryFX-lite CPU-driven.

### 7.2 Viable en GL 4.6 (upgrade ALZE v2)

- Cluster DAG offline construido con meshoptimizer + METIS. ✅ (offline, CPU, cualquier GL).
- Per-view cluster selection en compute shader. ✅ con compute (4.3+).
- Two-pass HZB occlusion culling (Haar+Aaltonen 2015 estilo). ✅ con compute + indirect.
- Visibility buffer con uint32 (cluster+triangle packed) + depth en RT separada. ✅ (16M clusters si usamos 24+8, suficiente para single-dev).
- Material pass tile-classified. ✅ con compute.
- Software rasterizer — ⚠️ **necesita `NV_shader_atomic_int64`** (propietario NV) o emulación 2× uint32 (slow). En AMD/Intel GL 4.6 no hay uint64 atomics; tendríamos que usar depth-only SW raster (write a uint32 con compare-exchange loop) — viable pero perf bajo.

### 7.3 Viable en Vulkan 1.3 (ALZE v3 aspiracional)

- Todo lo anterior. ✅
- Software raster con `VK_KHR_shader_atomic_int64` (core en 1.2) + image atomics. ✅
- Mesh shaders HW path via `VK_EXT_mesh_shader` (2022, amplio support RTX 2000+, RX 6000+, Intel Arc). ✅
- Streaming real con `VK_KHR_buffer_device_address` (core 1.2) + pool de VRAM + async IO (fuera del API). ✅

### 7.4 Qué NO tiene sentido clonar en ALZE

- **Streaming full de 100s de GB de assets** — ALZE es single-dev; no tiene el content pipeline ni los assets para amortizar.
- **Material pass fully-compute estilo UE 5.5** — demasiada infra material + shader graph.
- **Skeletal mesh + displacement Nanite path** — Beta incluso en UE, rabbit hole.
- **VSM integrada con SW raster** — multiplica el effort por 2.
- **Impostors 12×12 de 144 direcciones** — nice-to-have, bajo ROI.

### 7.5 Subset propuesto, priorizado

Prioridad **P0 (v1.5, GL 4.6)** — base GPU-driven cluster renderer:
1. Meshlet build offline con meshoptimizer (~1 sem integration).
2. Cluster buffer en SSBO + draw indirect (~1 sem).
3. Cluster culling compute (frustum + backface cone) (~1 sem).
4. Two-pass HZB occlusion (~2 sem).
5. Sin DAG todavía — usa LOD discreto por mesh (como LOD Group).

Entregable: **Nanite-lite sin virtualization**, equivalente a GeometryFX 2016 + Haar+Aaltonen 2015. Útil para escenas densas (10k+ objetos). 6-8 semanas realistas.

Prioridad **P1 (v2, GL 4.6 + NV atomic64 O Vulkan 1.3)** — virtualización básica:
6. DAG offline con METIS + simplify_with_attr (~2 sem build tool).
7. Runtime DAG traversal compute (~2 sem).
8. Visibility buffer uint32+depth (~1 sem).
9. Material pass tile-classified (sin compute resolve — full-screen quads por material OK) (~2 sem).

Entregable: **Bevy 0.14-level**. 7-9 semanas adicionales.

Prioridad **P2 (v3, Vulkan 1.3 + mesh shaders + atomic64)** — software raster:
10. SW raster compute (~3 sem).
11. HW/SW branching (~1 sem).
12. Streaming pool + page cache (~4 sem).
13. Scene BVH (~2 sem).

Entregable: **Bevy 0.16-level / Nanite UE 5.0-level minus production polish**. 10+ semanas adicionales.

Fuera de alcance (>1 año dev single-dev):
- Skeletal Nanite path.
- VSM integrada.
- Nanite Tessellation / micromesh.
- Material pass compute-based (UE 5.5+ style).
- HLOD / World Partition integration.
- Production-ready tooling (DDC, artist workflow).

### 7.6 Recomendación concreta

Dado que ALZE es single-dev y Fase 7 — hacer la **P0 lite (cluster renderer sin DAG)** primero. Da 80% del valor perceptible en escenas densas (orders of magnitude más draws) por 20% del coste. Si el producto pide mundos mega-detallados, escalar a P1 justifica. **NO empezar por P2** bajo ningún concepto — el SW raster sin el resto es inútil.

---

## 8. Referencias (con URLs + fallback)

| Ref | Autor / Año / Venue | URL |
|---|---|---|
| "A Deep Dive into Nanite Virtualized Geometry" (slides) | Brian Karis, Rune Stubbe, Graham Wihlidal. 2021. SIGGRAPH Advances in Real-Time Rendering | https://advances.realtimerendering.com/s2021/Karis_Nanite_SIGGRAPH_Advances_2021_final.pdf |
| Nanite talk video (YouTube upload) | idem, 2021 | https://x.com/BrianKaris/status/1454252365440163844 (link a video) |
| "Journey to Nanite" (extendida HPG 2022) | Karis et al., 2022, High Performance Graphics | https://www.highperformancegraphics.org/slides22/Journey_to_Nanite.pdf |
| CS 418 UIUC course notes sobre Nanite | J. Hart, 2023 | https://cs418.cs.illinois.edu/website/text/nanite.html |
| "From Navisworks to Nanite" | Candid Startup, 2023 | https://www.thecandidstartup.org/2023/04/03/nanite-graphics-pipeline.html |
| "A Macro View of Nanite" | Emilio López (elopezr), 2021 | https://www.elopezr.com/a-macro-view-of-nanite/ |
| "Notes on UE5 Nanite" | N. Papadopoulos, 2021 | https://www.4rknova.com/blog/2021/06/09/unreal-5-nanite |
| "Optimizing the Graphics Pipeline with Compute" | Graham Wihlidal (Frostbite/EA), GDC 2016 | https://www.ea.com/frostbite/news/optimizing-the-graphics-pipeline-with-compute · slides: https://www.wihlidal.com/projects/fb-gdc16/ · archive.org: https://archive.org/details/GDC2016Wihlidal |
| "GPU-Driven Rendering Pipelines" | Ulrich Haar (Ubisoft Montreal) + Sebastian Aaltonen (RedLynx/Ubisoft). SIGGRAPH 2015 Advances in Real-Time Rendering | https://advances.realtimerendering.com/s2015/aaltonenhaar_siggraph2015_combined_final_footer_220dpi.pdf |
| Bevy virtual geometry 0.14 blog | jms55 (Jasmine S.), 2024-06 | https://jms55.github.io/posts/2024-06-09-virtual-geometry-bevy-0-14/ |
| Bevy virtual geometry 0.15 blog | jms55, 2024-11 | https://jms55.github.io/posts/2024-11-14-virtual-geometry-bevy-0-15/ |
| Bevy virtual geometry 0.16 blog | jms55, 2025-03 | https://jms55.github.io/posts/2025-03-27-virtual-geometry-bevy-0-16/ |
| Bevy meshlet initial PR #10164 | jms55, 2023+ | https://github.com/bevyengine/bevy/pull/10164 |
| Bevy virtualized geometry discussion | bevy community | https://github.com/bevyengine/bevy/discussions/10433 |
| Meshoptimizer repo (buildMeshlets, simplify, partitionClusters) | Arseny Kapoulkine, 2016-2026 | https://github.com/zeux/meshoptimizer |
| "Modernizing Granite's mesh rendering" | Hans-Kristian Arntzen (Maister), 2024-01 | https://themaister.net/blog/2024/01/17/modernizing-granites-mesh-rendering/ |
| Tellusim "Compute versus Hardware" (rasterizer comparison) | A. Zaprjagaev, 2024 | https://tellusim.com/compute-raster/ |
| "Efficient Rendering in The Division 2" (Snowdrop cluster culling) | Sébastien Hillaire et al., GDC 2019 | https://gdcvault.com/play/1026293/Advanced-Graphics-Techniques-Tutorial-Efficient |
| "A Showcase of Decima Engine in Horizon Forbidden West" | Malan + van der Gaag, SIGGRAPH 2022 Real-Time Live! | https://dl.acm.org/doi/10.1145/3532833.3538681 · HTML: https://dl.acm.org/doi/fullHtml/10.1145/3532833.3538681 |
| Nanite WebGPU hobbyist port | Scthe, 2024 | https://github.com/Scthe/nanite-webgpu |
| Understanding Nanite (Epic blog, surface overview) | Epic, 2020 | https://www.unrealengine.com/en-US/blog/understanding-nanite---unreal-engine-5-s-new-virtualized-geometry-system |
| UE Public Roadmap — Nanite card | Epic | https://portal.productboard.com/epicgames/1-unreal-engine-public-roadmap/c/1155-nanite |
| Nanite Skeletal Mesh UE 5.5 forum thread | Epic community, 2024-11 | https://forums.unrealengine.com/t/nanite-skeletal-mesh-in-unreal-engine-5-5-main/1792367 |
| "Nanite on Skeletal Mesh shows extremely low poly" bug | Epic community, 2025 | https://forums.unrealengine.com/t/nanite-on-skeletal-mesh-shows-extremely-low-poly-from-certain-angles-in-ue-5-5/2573728 |
| Nanite fallback bug 5.5 | Epic community | https://forums.unrealengine.com/t/nanite-bug-all-nanite-meshes-sometimes-revert-to-fallback-mesh-in-5-5/2387326 |
| "Understanding Nanite: When to Use It and When to Skip It" | Hyperdense on Medium, 2024 | https://medium.com/@sarah.hyperdense/understanding-nanite-when-to-use-it-and-when-to-skip-it-b7fcfadc3058 |
| NVIDIA Turing Mesh Shaders intro | C. Kubisch, NVIDIA, 2018 | https://developer.nvidia.com/blog/introduction-turing-mesh-shaders/ |
| Micro-Mesh Rasterization (NVIDIA Ada) | C. Kubisch, NVIDIA, 2022 | https://developer.download.nvidia.com/ProGraphics/nvpro-samples/slides/Micro-Mesh_Rasterization.pdf |
| UE 5.3 Nanite Tessellation tutorial | Epic community | https://dev.epicgames.com/community/learning/tutorials/RBvX/unreal-engine-new-in-unreal-5-3-tessellation-displacement-in-nanite-meshes-tutorial |
| UE 5.4 Nanite Tessellation step-by-step | Epic community | https://dev.epicgames.com/community/learning/tutorials/bOda/unreal-engine-nanite-tessellation-displacement-ue-5-4-step-by-step-tutorial-any-asset-not-just-landscapes |
| `nvpro-samples/gl_vk_meshlet_cadscene` (mesh shader sample, GL + VK) | NVIDIA | https://github.com/nvpro-samples/gl_vk_meshlet_cadscene |
| GeometryFX 1.2 cluster culling (AMD GPUOpen) | AMD, 2017 | https://gpuopen.com/learn/geometryfx-1-2-cluster-culling/ |
| OpenGL 4.6 compute shader rasterizer | PepcyCh, GitHub | https://github.com/PepcyCh/opengl-cs-rasterizer |

Fallback archive.org si algún PDF Epic/advances.realtimerendering.com da 403: https://web.archive.org/web/2024*/advances.realtimerendering.com/s2021/Karis_Nanite_SIGGRAPH_Advances_2021_final.pdf (suele tener copia). La página `wihlidal.com/projects/fb-gdc16/` tuvo 403 intermitente en sesiones previas — usar archive.org si recurre.

---

## 9. Tabla ALZE applicability

Columnas: feature | v1 (GL 3.3 actual) | v2 (GL 4.6) | v3 (Vulkan 1.3 aspiracional) | dependency.

| Feature | v1 GL 3.3 | v2 GL 4.6 | v3 Vulkan 1.3 | Dependency |
|---|---|---|---|---|
| Meshlet formation offline (128 tri clusters) | ✅ CPU (offline) | ✅ | ✅ | meshoptimizer (MIT) |
| Cluster DAG con simplificación + edge lock | ✅ CPU (offline) | ✅ | ✅ | meshoptimizer + METIS |
| Per-mesh BVH offline | ✅ | ✅ | ✅ | self, simple BVH builder |
| Renderer cluster-based + indirect draw | ❌ (no indirect en 3.3 core; ARB_draw_indirect opcional) | ✅ glMultiDrawElementsIndirect | ✅ vkCmdDrawIndexedIndirect | GL 4.3+ / Vulkan |
| Cluster cull frustum + cone backface (compute) | ❌ sin compute | ✅ compute shaders | ✅ | GL 4.3 compute / Vk compute |
| Two-pass HZB occlusion culling | ❌ | ✅ | ✅ | compute + texture mip sampling |
| Persistent threads + work queue | ❌ | ⚠️ via SSBO atomic counters (perf bajo vs Vk) | ✅ | compute + atomics |
| DAG traversal runtime (per-view LOD) | ❌ | ✅ compute | ✅ | compute |
| Visibility buffer uint32 (cluster+tri packed) + depth attach | ⚠️ posible con integer RT, sin writes condicionales | ✅ | ✅ | integer MRT / compute write |
| Visibility buffer uint64 estilo Nanite | ❌ | ⚠️ sólo en NV con NV_shader_atomic_int64 | ✅ core VK 1.2 atomic_int64 | ARB/VK_KHR_shader_atomic_int64 |
| Material pass tile-classified (fullscreen quad per material) | ⚠️ feasible pero lento sin compute | ✅ | ✅ | FBO blit + stencil |
| Material pass compute-based (UE 5.5 style) | ❌ | ✅ | ✅ | compute |
| Software rasterizer depth-only (uint32 atomic CAS loop) | ❌ | ⚠️ slow (~10× hw) | ✅ | compute + image atomic CAS |
| Software rasterizer full (depth+visID en atomic uint64) | ❌ | ⚠️ NV-only | ✅ | VK 1.2 / DX12 SM 6.6 |
| HW path via mesh shaders | ❌ | ⚠️ NV_mesh_shader extension only | ✅ VK_EXT_mesh_shader | mesh shader ext |
| Streaming pages 128 KB desde disco | ⚠️ CPU-side sólo | ✅ con async IO + buffer sub updates | ✅ con DirectStorage-like | async IO layer |
| Resident pool VRAM gestionado (LRU + root-always-resident) | ⚠️ CPU track | ✅ | ✅ | buffer allocator |
| Skeletal mesh virtualized | ❌ | ❌ no realista single-dev | ⚠️ research-grade, no ship | Nanite Skeletal es Beta |
| Nanite Tessellation / displacement | ❌ | ❌ | ⚠️ fuera de alcance | micromesh ext |
| VSM integrada con cluster raster | ❌ | ❌ | ⚠️ fuera de alcance | |
| Impostors 12×12 × 144 views (far LOD) | ✅ CPU bake | ✅ | ✅ | texture atlas bake tool |

Leyenda:
- ✅ = viable con effort razonable (<6 sem).
- ⚠️ = viable pero con caveats grandes (hardware específico, perf comprometida, o effort >10 sem).
- ❌ = no viable en esa versión.

---

## 10. Notas finales para ALZE

- **Ganancia real máxima posible** para ALZE dado el staff (single-dev) y el stack (SDL2+GL): hasta el tier Bevy 0.14 (cluster DAG + two-pass cull + visibility buffer + material pass) en Vulkan 1.3. **~6 meses de dev enfocado**.
- **Falsa ganancia**: copiar el SW raster primero sin el resto. Sin DAG y sin streaming es un compute shader caro sin uplift percibido.
- **Reusar en lugar de reimplementar**: meshoptimizer hace el 70% del trabajo offline. METIS-rs equivalente en C++ (`metis` original de Karypis) para el grouping. Escribir el builder es semanas, no meses.
- **Diagnóstico del core loop**: el 80% de la innovación Nanite es **arquitectura** (cómo las piezas se conectan: DAG seamless + cluster-parallel traversal + visibility buffer desacoplado + material pass diferido + two-pass HZB) — no un algoritmo específico. Clonar los componentes aislados no reproduce el beneficio integrado hasta que todos conviven en el mismo frame.
- **Referencia más útil para empezar**: Bevy meshlet PRs + blog jms55 (código abierto, Rust pero trivial de portar el approach) + SIGGRAPH 2021 Karis slides para ideas + meshoptimizer samples para el offline pipeline.
- **Señal de que estás fuera de scope**: si piensas "necesito mesh shaders + VK_EXT + streaming + VSM" para la v1 — stop. Cut a P0 lite (cluster renderer + multi-draw-indirect + CPU-side selection) y ship eso antes.

Notas de acceso 2026-04-22:
- `advances.realtimerendering.com/s2021/Karis_Nanite_SIGGRAPH_Advances_2021_final.pdf` devolvió `maxContentLength exceeded` vía WebFetch (PDF >10 MB). Contenido cubierto vía writeup CS418/elopezr/candid-startup que citan el slide deck.
- `advances.realtimerendering.com/s2015/aaltonenhaar_...` idem (PDF binario no parseable por el fetcher). Contenido cubierto vía abstracts + Wikipedia + discussions Beyond3D.
- `wihlidal.com/projects/fb-gdc16/` y `gdcvault.com/play/1023109/` — ocasionalmente paywalled; usar archive.org fallback `archive.org/details/GDC2016Wihlidal`.
