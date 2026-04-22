# Síntesis round 3 — Rendering / GPU SOTA 2024-2026

**Fecha:** 2026-04-22
**Input:** 7 agentes paralelos, ~3,156 líneas. 0 fallos bloqueantes (varios PDFs SIGGRAPH >10MB esquivados vía abstracts + mirrors; Epic 403s esquivados vía UWA + Narkowicz blogs + SimLumen reimpl source).

Complementa [`_sintesis.md`](_sintesis.md) (R1) y [`_sintesis_round2.md`](_sintesis_round2.md) (R2). Donde R1 describió el paisaje de libs y R2 los engines AAA como productos, **R3 es el primer deep-dive algorítmico** en las técnicas bleeding-edge.

- [`r3/nanite.md`](r3/nanite.md) — Virtualized geometry (498 L). Cluster DAG + meshlet simplification + dos-pass HZB + software rasterizer heurística + UE 5.5 cambios.
- [`r3/lumen.md`](r3/lumen.md) — SW + HW Lumen + surface cache + screen probes + VSM (461 L). Compara con DDGI, ReSTIR GI, Radiance Cascades, VXGI.
- [`r3/mesh_shaders_work_graphs.md`](r3/mesh_shaders_work_graphs.md) — task/mesh/amplification + DX12 Work Graphs + Alan Wake 2 flagship (554 L). 2026 baseline HW: Turing+, RDNA2+, Xe-HPG+, M3+, Adreno 7xx+, Switch 2 Ampere.
- [`r3/ray_tracing_2024_2026.md`](r3/ray_tracing_2024_2026.md) — BVH build/refit + DXR 1.1/1.2 + SER + ReSTIR family + path tracing shipping modes (355 L).
- [`r3/neural_rendering.md`](r3/neural_rendering.md) — DLSS 1→4, FSR 1→4, XeSS, Frame Gen, NTC, Neural Radiance Caches, 3D Gaussian Splatting (514 L).
- [`r3/frame_graph_bindless.md`](r3/frame_graph_bindless.md) — O'Donnell FrameGraph + UE5 RDG + AMD FFX FG + bindless descriptor indexing (431 L).
- [`r3/virtual_textures_streaming.md`](r3/virtual_textures_streaming.md) — MegaTexture → Decima VT → UE5 RVT/SVT + sparse residency + KTX2 transcoding (343 L).

---

## Tabla cross-R3 (técnica × estado 2026 × ALZE feasibility)

| Técnica | Shipping engines | HW baseline | ALZE v1 (GL 3.3) | ALZE v2 (Vulkan) | ALZE v3 (aspiracional) |
|---|---|---|---|---|---|
| Nanite full | UE5 (Fortnite, Wukong) | RTX 20+, RDNA2+ | ✗ | Meshlet + cluster cull posible (~12k LOC), software raster NO (inviable solo-dev) | Full Nanite clone = 2+ años dedicados |
| Meshlet + cluster culling | Decima, Snowdrop, id Tech 8 | Cualquier HW con compute | Parcial (compute-less GL 3.3, solo CPU-side) | Sí — **buena ROI** ~3-5k LOC | — |
| Lumen SW (SDF + surface cache) | UE5 baseline | ~GTX 1060 | ✗ | Muy caro en complejidad (~8-12k LOC) | Posible si hay compute |
| Lumen HW (RT bounces) | UE5 Epic preset | RTX 30+, PS5 Pro | ✗ | Requiere VK_KHR_ray_tracing | Aspiracional |
| DDGI (Majercik 2019) | UE5 alt, bevy demos, Godot 4.x candidate | RT HW ideal, SW fallback ok | ✗ | **Recomendable** — 2-3k LOC, publicado clean | — |
| Mesh shaders | Alan Wake 2, Indy GC | Turing+, RDNA2+ | ✗ (no extensión cross-vendor en GL) | Path opt-in detrás de feature flag (~2-4 eng-mo) | — |
| Work Graphs DX12 | Ninguno shipping aún 2026 | Ada+, RDNA3+, DX12.2 | ✗ | ✗ (no Vulkan equiv) | Monitor 2027+ |
| Path tracing | CP77 RT Overdrive, Alan Wake 2, Portal RTX, Indy, DTDA | RTX 40+ ideal | ✗ | ✗ | Aspiracional only, denoiser stack (NRD/Ray Reconstruction) domina dificultad |
| ReSTIR DI/GI | UE5 Lumen, NVIDIA RTXDI | RT HW | ✗ | Estudiar paper (Bitterli 2020) | — |
| Shader Execution Reordering | NVIDIA Ada+, DXR 1.2 | Ada only via NVAPI | ✗ | Vulkan NV-ext only, no portable | Depende de adopción vendor |
| DLSS 2/3/4 | 800+ títulos | RTX 20+ | Via Streamline SDK | Integración SDK ~1-2 sem | — |
| FSR 2.x (open source) | Cross-vendor | Cualquier GPU | **Recomendable** — reference impl MIT | — | — |
| Frame Generation (DLSS 3 FG, FSR 3 FG) | CP77, BG3, Starfield, etc | Ada / RDNA3+ | ✗ | Vendor SDK integration | — |
| Neural Texture Compression | Vaidyanathan 2023, Intel demo | Shader compute | ✗ | Investigación, runtime decode posible ~4-8× smaller | — |
| 3D Gaussian Splatting | Unity/UE5 community plugins, Aras-p impl | Shader compute | ✗ | Rasterizer custom ~5-8k LOC | Hybrid w/ mesh-based scene |
| Frame Graph DAG | Frostbite (O'Donnell), UE5 RDG | Any compute API | Sort-key only | FG ~2-3k LOC después de RHI abstraction | — |
| Bindless resources | Frostbite, UE5, Snowdrop | Vulkan 1.2, DX12 | ✗ (GL no bindless real) | Descriptor indexing = futuro correcto | — |
| Virtual Textures RVT+SVT | UE5 landscape, Decima | Sparse residency (GL 4.4+) | Parcial (terrain RVT posible ~4-6k LOC) | SVT completo ~10-15k LOC | — |
| MegaTexture (id Tech 5 legacy) | Obsoleto | — | ✗ | ✗ | Lección histórica, no copiar |
| KTX2 + Basis Universal transcoding | Cross-engine | — | **Adoptar YA** — sustituye stb_image+DXT | — | — |

---

## Top 12 ideas concretas R3 para ALZE

Ordenadas por **ROI real a solo-dev** (leverage/esfuerzo):

### 1. KTX2 + Basis Universal transcoding — adoptar **ya** en v1
Reemplaza stb_image + BC1/3/7 pre-baked por transcoder universal. ~300 LOC integración, 30-50% ahorro disco + ~2x speedup en texture load. Work today con GL 3.3. R3/virtual_textures_streaming.md §6.

### 2. Sort-key submission layer (bgfx pattern) — "primer deliverable" R3→R4
v1 antes de FrameGraph. ~1 semana de ingeniería. Ordena draws por layer/view/blend/shader/material/depth, reduce state changes drásticamente, sienta runtime data structures reutilizables por FG v2. R3/frame_graph_bindless.md §13.6.

### 3. Nubis clouds (Schneider 2015+2017) — ~4-6 semanas
Perlin-Worley 128³ + Worley 32³ + curl noise + HG phase + temporal reprojection 1-in-16 pixels. Shipped en Killzone SF 2013 + HZD 2017 + MGS V. **La técnica más portable + espectacular** de toda la research. R4/decima.md §4.

### 4. Meshlet + cluster culling en v2 Vulkan — ~3-5k LOC
Sin mesh shaders: CPU-side clusterizar offline con meshoptimizer + GPU compute culling per-frame + indirect draw. ~60-80% reducción draws. No requiere DXR ni mesh shaders HW. Shipping pattern: Frostbite, Decima, Snowdrop, id Tech 8 fallback path.

### 5. DDGI (Majercik 2019) — 2-3k LOC — ganador de dynamic GI para ALZE
Mejor que Lumen para solo-dev: paper clean, no requiere BVH propia, funciona sin RT HW (SW fallback a SDF). Bevy y Godot lo consideran. R3/lumen.md tabla comparativa.

### 6. FSR 2.x como único upscaler v1
Open source, MIT, cross-vendor. Integración ~2 semanas + motion vectors + TAA hookup. DLSS solo si hay demanda usuario (Streamline SDK wrapper). R3/neural_rendering.md.

### 7. Velocity-aware cone streaming (Rockstar/Decima)
Ya identificada en R2 pero R3 aporta: distance = clamp(camera_velocity × k, min, max). Implementable ~2 eng-semanas sobre cualquier chunk-based world. RDR2 shipped con 8GB RAM + 75km². R4/decima.md.

### 8. Checkerboard rendering + TAAU (temporal AA+upscaling)
Render half pixels por frame, temporal reconstruct. 4K output con 1080p cost. Shipping: HZD Pro, HFW, Spider-Man PS5. ~3-4 eng-semanas. R4/decima.md §6.

### 9. Two-pass HZB occlusion culling (Nanite pattern, pero portable)
Pass 1: draw objects visible last frame, build HZB. Pass 2: test objects not drawn last frame against HZB, draw if visible. ~500 LOC shader. Funciona sin Nanite. R3/nanite.md §3.

### 10. Bindless descriptor indexing como norte arquitectónico v2
Vulkan 1.2 VK_EXT_descriptor_indexing. API diseñada en v1 (opaque handles) debe soportar bindless en v2 sin rewrite. Pedir a cada material un u32 index, no un descriptor binding. R3/frame_graph_bindless.md §8.

### 11. Neural Texture Compression (Vaidyanathan 2023) — para v3 texture streaming
4-8x smaller textures, shader-side decode. ~250 MB texture budget → 35 MB. Interesting research direction, no shipping title aún en 2026 (Intel demo only). Puede ser moat si ALZE llega a v3 scope. R3/neural_rendering.md §6.

### 12. RVT para terrain (Runtime Virtual Textures) — en v2 si terrain es relevante
Solo RVT (no SVT). ~4-6k LOC. Terrain pinta material graph a tile atlas once, sampling posterior es indirecto lookup simple. Evita recalcular splat mapping cada frame. R3/virtual_textures_streaming.md §4.

---

## Anti-patterns R3 (nuevos)

16-24 (continúan numeración de R1 = 1-15 y R2 = 16):

16. **Intentar Nanite clone para un solo dev** — es 3+ años full-time. UE5 Nanite tiene 4-5 senior engineers activos. Usar cluster culling tradicional v2.
17. **Path tracing en v1** — denoiser stack (NRD / Ray Reconstruction) es MÁS difícil que el path tracer. Aspiracional v3 o pagar NVIDIA RTX Direct Illumination SDK.
18. **DLSS sin FSR** — vendor lock. Si integras upscaler, pon FSR 2.x primero, DLSS segundo vía Streamline.
19. **Frame Graph antes de tener 10+ passes** — complejidad no justificada. Sort-key sobra hasta ahí.
20. **VT autoritarian** — VT source-of-truth para todo es el error de MegaTexture / Rage 2011. VT solo para terrain y detail maps; resto = mip streaming clásico.
21. **Work Graphs DX12 chasing** — 2026 no hay shipping title en producción. Monitor, no adoptar.
22. **Mesh shaders como prerequisite** — fallback classic VS + GPU-driven indirect draw cubre 95% de escenarios. Solo Alan Wake 2 + Indy son mesh-shader-baseline shipped.
23. **HRT Lumen en v1** — requiere BVH propia + denoiser. 5+ eng-semanas ANTES de ver GI. DDGI es mejor ROI.

---

## Update al stack ALZE (delta sobre R1+R2)

### v1 (GL 3.3, hoy) — añadir
- **KTX2 + Basis transcoding** (§ top idea #1) — reemplaza stb_image texture pipeline
- **Sort-key render queue** (§ top idea #2) — antes de cualquier FG
- **Nubis clouds** (§ top idea #3) — el mayor bang-per-buck espectacular del engine
- **Velocity cone streaming** (§ top idea #7) — si ALZE tiene open world
- **Two-pass HZB occlusion** (§ top idea #9) — compatible GL 3.3 con compute extension

### v2 (Vulkan 1.2+ migration) — añadir
- **Frame Graph DAG** O'Donnell-style (~2-3k LOC) reemplaza sort-key
- **Bindless descriptor indexing** — diseñar APIs para esto desde día 1 v2
- **Meshlet + cluster culling** (CPU-clusterize + GPU cull + indirect draw)
- **DDGI** para dynamic GI (2-3k LOC, Majercik paper)
- **FSR 2.x** upscaler (cross-vendor, MIT)
- **Checkerboard + TAAU** (4K output a 1080p cost)
- **Path tracing research** — solo shader experimental, NO producción
- **RVT terrain** si ALZE tiene terrain relevante

### v3 (aspiracional, si alcance crece)
- **Hardware Lumen / RT GI** — con DXR/VK_KHR_ray_tracing
- **Neural Texture Compression** — offline bake, shader decode
- **3D Gaussian Splatting hybrid** — para scenes scanned
- **Work Graphs** cuando sea standard cross-vendor

---

## Ranking de densidad R3

De más a menos útil **para ALZE a corto plazo**:

1. **frame_graph_bindless.md** — sort-key v1 es accionable YA, bindless es el norte v2. Aplicable inmediato.
2. **virtual_textures_streaming.md** — KTX2 es adopción YA; VT parcial para terrain es v2 decision limpia.
3. **neural_rendering.md** — FSR integration es v2 quick-win; DLSS via Streamline es follow-up. Gaussian Splatting es research track.
4. **nanite.md** — NO implementable en solo-dev. Pero "two-pass HZB + meshlet cluster culling (sin Nanite DAG)" SÍ, y el file lo explica bien.
5. **lumen.md** — mejor lección es **NO copiar Lumen**. DDGI es la alternativa. File explica por qué.
6. **mesh_shaders_work_graphs.md** — v2 opcional detrás de feature flag. Work Graphs = no hoy.
7. **ray_tracing_2024_2026.md** — aspiracional. Valor está en las referencias para cuando se vuelva relevante.

---

## Transición a R4 + R5

R3 cubrió **técnicas** de rendering. R4 cubre **engines AAA que saltamos** en R2 (Decima, Naughty Dog, Insomniac, Santa Monica, FromSoftware, id Tech 7/8, Anvil). R5 cubre **sistemas cross-cutting** no-gráficos (audio, net, anim, jobs, memoria, editor, DCC).

Después de R3+R4+R5 el engine ALZE tiene el mapa completo: paisaje (R1) + historia AAA (R2) + técnicas GPU (R3) + engines missed (R4) + systems (R5).
