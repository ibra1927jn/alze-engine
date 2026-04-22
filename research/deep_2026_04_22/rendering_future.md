# El futuro del rendering en motores gráficos 2026-2032

**Deep research round — 2026-04-22 — target: `/root/repos/alze-engine` (C++17 no-RTTI/no-exceptions, SDL2+GL3.3 hoy, Vulkan mañana, ~25-30k LOC, single-dev).**

Este documento **no repite** lo ya cubierto en `r3/`:
- `nanite.md` — cluster-DAG, software raster, meshlet LOD, visibility buffer.
- `lumen.md` — surface cache, mesh cards, GDF/MDF, screen-probe gather, VSM.
- `mesh_shaders_work_graphs.md` — task/mesh pipeline fundamentals, work graph API base.
- `ray_tracing_2024_2026.md` — DXR 1.0/1.1/1.2, ReSTIR DI/GI/PT, SER, OMMs, BVH management.
- `neural_rendering.md` — DLSS 1-4, FSR 1-4, XeSS, frame gen taxonomy, 3DGS introducción.
- `virtual_textures_streaming.md` — MegaTexture, Decima VT, UE RVT/SVT, Granite.
- `frame_graph_bindless.md` — RDG, barrier math, transient aliasing.

Foco aquí: **proyección 3-10 años** (2026→2032), hardware roadmaps confirmados, papers publicados pero aún no productizados, y un veredicto de apuesta / no-apuesta para un engine small-team al final.

---

## 0. Marco temporal — cómo leer este documento

Asumo tres horizontes:

- **Near (2026-2027)** — producto shippeable. Compradores tipo indie AA con hardware RTX 40/50, PS5/PS5 Pro. Lo que vemos ya funcionando en Indiana Jones, Wukong, DOOM The Dark Ages.
- **Mid (2028-2030)** — la transición PS6 / Xbox Next. Cambio generacional real que fuerza baselines. Lo que entra en producción "por defecto".
- **Far (2030-2032)** — lo que hoy es paper de SIGGRAPH pero con 3 generaciones de GPU detrás: neural everything, differentiable rendering en pipelines artísticos, possibly post-mesh primitives.

Cada sección marca cuándo las tecnologías cruzan esos umbrales según la evidencia pública.

---

## 1. Real-time path tracing como default

### 1.1 La trayectoria shippeada (2023-2026)

La curva de adopción de PT completo (no hybrid RT) es más rápida de lo que se predecía en 2022:

| Año | Título | Motor | PT scope |
|-----|--------|-------|----------|
| 2023-04 | Cyberpunk 2077 RT Overdrive | REDengine 4 | Full PT (DI+GI+reflections), primer showcase |
| 2023-10 | Alan Wake 2 | Northlight | PT indirect + reflections, direct rasterizado |
| 2024-08 | Black Myth: Wukong | UE5 | Full RT con Lumen HWRT, path-tracing mode post-launch |
| 2024-12 | Indiana Jones and the Great Circle | id Tech 8 | **Mandatory RT + optional full PT** |
| 2025-05 | DOOM: The Dark Ages | id Tech 8 | **Mandatory RT at launch**, PT patch |
| 2026-Q2 | Varios UE5.6+ | UE5 | PT vía RTX Mega Geometry (Nanite + RT combinados) |

El momento clave es **Indiana Jones (Dec 2024)**: primer AAA donde el RT no es toggle — el engine **requiere** GPU RT-capable, no hay fallback rasterizador. id Tech 8 ship sin modo non-RT. DOOM The Dark Ages (2025) confirma la política. [Wccftech Indiana Jones PT benchmarks](https://www.dsogaming.com/articles/indiana-jones-and-the-great-circle-path-tracing-benchmarks/); [PCGamer: ray tracing mandatory](https://www.xda-developers.com/ray-tracing-became-mandatory-modern-gpu-didnt-catch-up/).

### 1.2 Papers que sostienen el ecosistema

El PT real-time no funciona sin una stack de papers apilados. Los que ya son producto:

- **ReSTIR DI** — Bitterli, Wyman, Pharr et al. SIGGRAPH 2020 ([research.nvidia.com](https://research.nvidia.com/publication/2020-07_Spatiotemporal-reservoir-resampling-real-time-ray-tracing-dynamic-direct)).
- **ReSTIR GI** — Ouyang, Liu, Zhang, Pantaleoni, Novák. HPG 2021 ([research.nvidia.com](https://research.nvidia.com/publication/2021-06_restir-gi-path-resampling-real-time-path-tracing)).
- **Generalized RIS / ReSTIR PT** — Lin, Kettunen, Bitterli, Pantaleoni, Yuksel, Wyman, Pharr. SIGGRAPH 2022 ([research.nvidia.com](https://research.nvidia.com/publication/2022-07_generalized-resampled-importance-sampling-foundations-restir)). Unifica DI/GI/PT en un solo framework teórico (GRIS) con garantías de convergencia offline.
- **NRC — Neural Radiance Cache** — Müller, Rousselle, Novák, Keller. SIGGRAPH 2021 ([tom94.net PDF](https://tom94.net/data/publications/mueller21realtime/mueller21realtime.pdf)). Red tiny MLP que aprende la irradiance "infinity-bounce" online, entrenada durante gameplay.
- **SER — Shader Execution Reordering** — Lefohn et al. 2022 (cubierto en `r3/ray_tracing_2024_2026.md`).

### 1.3 Papers 2024-2026 que aún no son producto masivo pero lo serán

- **ReSTIR PT Enhanced** — Lin et al. 2026 ([research.nvidia.com/labs/rtr/publication/lin2026restirptenhanced](https://research.nvidia.com/labs/rtr/publication/lin2026restirptenhanced/)). Algorithmic advances: context-aware shift mapping, robustez frente a highly glossy.
- **ReSTIR BDPT** — Bidirectional ReSTIR con caustics en real-time. ACM TOG 2026 ([dl.acm.org](https://dl.acm.org/doi/10.1145/3744898)). Primer camino viable a **caustics real-time** (agua, cristal, refracciones complejas), hoy imposibles con PT unidireccional a 1 spp.
- **ReSTIR PG** — Path Guiding con spatiotemporal reuse. SIGGRAPH Asia 2025 ([dl.acm.org](https://dl.acm.org/doi/10.1145/3757377.3763813)). Aprende la distribución de luz por voxel durante el frame, reduce varianza ~3-5x en escenas con iluminación compleja.

### 1.4 Hardware que habilita PT mainstream

Los tres ingredientes HW para que PT sea default:

1. **RT cores 3ª gen+** con ops mejoradas (Ada RTX 40, Blackwell RTX 50, RDNA 4, Arc Xe2+). Ops key: SER/ReorderThread, OMM accel, cluster BLAS.
2. **Tensor cores integrados en gráficos** con cooperative vectors (Blackwell SM rediseñado "Built for Neural Shaders" [NVIDIA Blackwell whitepaper PDF](https://images.nvidia.com/aem-dam/Solutions/geforce/blackwell/nvidia-rtx-blackwell-gpu-architecture.pdf)).
3. **VRAM ≥ 12 GB** para BLAS + surface cache + neural models. Los RTX 4070/5070 Ti con 12-16 GB son el piso realista.

**Cuándo es mainstream?** Predicción basada en GPUs baseline:
- Steam survey Q1 2026: RTX 2060/3060/4060 siguen siendo top-3. Baseline actual: DXR 1.1 + 8 GB VRAM. PT ahí es inviable.
- Q4 2027: RTX 5060 (Blackwell baseline) + refreshes RDNA 4 llegan al 40-50% del mercado. PT "low preset" pasa a ser viable pero no default.
- **2028-2029: PS6 ship con 52-54 RDNA 5 CUs + machine learning HW derivada del Project Amethyst (Sony/AMD)** ([PS6 leaks](https://wccftech.com/roundup/ps6-vs-xbox-next-project-helix-everything-we-know/)) — este es el punto donde PT se normaliza como render path default en motores AAA que targetean consola. Xbox Project Helix (68 CU RDNA 5) aún más.
- **2030+**: Refreshes mid-gen + mobile Snapdragon/Apple con RT cores reales. PT entra en handheld tier.

**Predicción firmada**: *full PT* será el modo high/ultra default en AAA PS6 generation circa 2029. Hybrid RT (Lumen HWRT style, NRC-accelerated) es el default AAA **ahora** (2026) y seguirá siéndolo en PS5/Pro ports.

---

## 2. Neural rendering — convergencia 2026-2030

### 2.1 Estado del arte comercial (2026-Q1)

| Producto | Vendor | HW gate | Técnica |
|----------|--------|---------|---------|
| DLSS 4.5 (SR Transformer 2nd gen + MFG 6x) | NVIDIA | RTX 20/30/40/50 (SR); RTX 50 (MFG) | [NVIDIA GeForce news](https://www.nvidia.com/en-us/geforce/news/dlss-4-5-dynamic-multi-frame-gen-6x-2nd-gen-transformer-super-res/) |
| FSR 4 "Redstone" (ML upscaling + FG + ray regen) | AMD | RDNA 4 (RX 9000) | [GPUOpen Redstone](https://gpuopen.com/learn/amd-fsr-redstone-developers-neural-rendering/) |
| XeSS 2 (ML + FG) | Intel | Arc Alchemist+ con XMX; DP4a fallback | [gpuopen AMD FSR](https://gpuopen.com/fidelityfx-super-resolution-4/) |
| PSSR 2 | Sony | PS5 Pro ML HW custom | [PlayStation Blog PSSR upgrade](https://blog.playstation.com/2026/02/27/upgraded-pssr-upscaler-is-coming-to-ps5-pro/) |
| MetalFX Frame Interpolation + Denoiser | Apple | M3+ (RT), M5 (neural accel per GPU core) | [Apple Metal 4 WWDC25](https://developer.apple.com/videos/play/wwdc2025/211/) |

Project Amethyst (Sony ↔ AMD) unifica la investigación neural upscaling entre PS5 Pro y RDNA 4/5. PSSR y FSR 4 comparten la base de red.

### 2.2 Neural texture / materials / motion

**Neural Texture Compression (NTC)** — Vaidyanathan, Salvi, Wronski, Akenine-Möller, Ebelin, Lefohn. NVIDIA. ACM TOG / SIGGRAPH 2023 ([paper](https://research.nvidia.com/publication/2023-08_random-access-neural-compression-material-textures), [history.siggraph.org](https://history.siggraph.org/learning/random-access-neural-compression-of-material-textures-by-vaidyanathan-salvi-wronski-akenine-moller-ebelin-et-al/)). **Clave**: 16x más detalle vs BC7 a mismo bitrate, random-access en GPU, red pequeña (~1-2 KB pesos por material). Complementado por:
- **Neural Graphics Texture Compression Supporting Random Access** — ECCV 2024 ([arxiv 2407.00021](https://arxiv.org/html/2407.00021)).
- **RTX Neural Texture Compression (NTC) SDK v0.9.2 BETA** — [github.com/NVIDIA-RTX/RTXNTC](https://github.com/NVIDIA-RTX/RTXNTC). Primer SDK productizable, Q4 2025.

**Neural Materials** — "Real-time Neural Appearance Models" — Zeltner, Rousselle, Weidlich, Clarberg, Novák, Bitterli, Evans, Davidovič, Kallweit, Lefohn. ACM ToG 2024 ([research.nvidia.com/labs/rtr/neural_appearance_models](https://research.nvidia.com/labs/rtr/neural_appearance_models/)). MLP decoders que predicen BRDF values, inline execution dentro del path tracer usando tensor ops en ray shaders. >10x faster vs multilayered materials no-neural.

**Neural BRDF Importance Sampling by Reparameterization** — SIGGRAPH 2025 ([dl.acm.org](https://dl.acm.org/doi/10.1145/3721238.3730679)).

**Neural motion textures / Learned Motion Matching** — Ubisoft La Forge 2020 ([ubisoft.com/en-us/studio/laforge/news/6xXL85Q3bF2vEj76xmnmIu](https://www.ubisoft.com/en-us/studio/laforge/news/6xXL85Q3bF2vEj76xmnmIu/introducing-learned-motion-matching)) + **Sparse Mixture of Experts + Lipschitz** 2024 ([sciencedirect](https://www.sciencedirect.com/science/article/abs/pii/S0097849324000463)) — 8.5x faster CPU que motion matching clásico con 80% memoria. Este es el camino productizable para animación runtime neural en 2027+.

### 2.3 La convergencia real: SM 6.9 Cooperative Vectors + Slang

El gran cambio de 2025-2026 no es un algoritmo, es una **abstracción de HW**:

- **D3D12 Cooperative Vectors** — Microsoft GDC 2025 ([devblogs.microsoft.com/directx/cooperative-vector](https://devblogs.microsoft.com/directx/cooperative-vector/)). HLSL puede llamar matrix-vector muls con vectores hasta 1024 elementos; el driver mapea a tensor cores/XMX/WMMA/Apple neural accel según vendor. Neural shaders en HLSL **sin ir por CUDA**.
- **SM 6.9 retail Agility SDK 1.619** — Feb 2026 ([devblogs.microsoft.com/directx/shader-model-6-9-retail-and-more](https://devblogs.microsoft.com/directx/shader-model-6-9-retail-and-more/)).
- **SM 6.10** roadmap — Cooperative Vector será deprecated en favor de `matrix-matrix + vector-matrix unified ops`, anunciado para 2026-2027 ([devblogs.microsoft.com/directx/shader-model-6-9-and-the-future-of-cooperative-vector](https://devblogs.microsoft.com/directx/shader-model-6-9-and-the-future-of-cooperative-vector/)).
- **Vulkan** — `VK_NV_cooperative_vector` (2025), expected `VK_KHR_cooperative_matrix2` con Vulkan 1.5 2026-2027 ([khronos.org/blog/vulkan-continuing-to-forge-ahead-siggraph-2025](https://www.khronos.org/blog/vulkan-continuing-to-forge-ahead-siggraph-2025)).
- **Slang** — transferido de NVIDIA a Khronos Nov 2024 ([khronos.org/news/press/khronos-group-launches-slang-initiative](https://www.khronos.org/news/press/khronos-group-launches-slang-initiative-hosting-open-source-compiler-contributed-by-nvidia)). Slang tiene módulos, auto-diff y targeting multi-API (D3D12/Vulkan/Metal/CUDA/WebGPU) desde el mismo código fuente. Valve Source 2 (CS2, Dota 2) shipeó Slang en producción.

**Por qué importa para engine small-team**: escribir un shader neural en 2026-2028 no requiere CUDA ni el stack propietario de NVIDIA. Slang + SM 6.9 + Vulkan 1.4 + WebGPU son suficientes. El ecosystem se estandarizó brutalmente en 18 meses.

### 2.4 DLSS 5 / FSR 5 proyección

Lo que sabemos:
- **Blackwell SM was designed & optimized for neural shaders**, con neural shaders "consisting of closer integration between tensor cores and general-purpose compute units" ([hwcooling.net](https://www.hwcooling.net/en/cooperative-vectors-in-directx-to-use-blackwell-neural-shaders/)).
- DLSS 4.5 transformer model tiene 4x computations y 2x params del CNN DLSS 3 ([research.nvidia.com/labs/adlr/DLSS4](https://research.nvidia.com/labs/adlr/DLSS4/)).

**Proyección DLSS 5 (probable 2027 con Rubin)**:
- Pipeline end-to-end neural: SR + RR + FG + denoise + material shading en un único backbone transformer. Ya está en research path, el bottleneck es el presupuesto de frame (~3-5ms hoy).
- Temporal windows más largos (32-64 frames vs 4-8 actuales) usando attention eficiente.
- **Generative**: inpaint zonas disoccluded con diffusion-lite, elimina disocclusion artifacts. Research de Intel 2025 ([Intel blog Neural Image Reconstruction](https://community.intel.com/t5/Blogs/Tech-Innovation/Client/Neural-Image-Reconstruction-for-Real-Time-Path-Tracing/post/1688192)) ya apunta aquí.

**Proyección FSR 5 (probable 2027-2028 con UDNA / RDNA 5)**:
- Unificación con PSSR 2+ bajo Project Amethyst. AMD y Sony comparten backbone ML.
- "Ray Regeneration 1.x" — FSR "Redstone" incluye RR competitor, maduración en FSR 5.

---

## 3. 3D Gaussian Splatting en producción

### 3.1 De paper a ecosystem (2023-2026)

- **Kerbl, Kopanas, Leimkühler, Drettakis. "3D Gaussian Splatting for Real-Time Radiance Field Rendering"**. SIGGRAPH 2023 — el paper base.
- **4D Gaussian Splatting for Real-Time Dynamic Scene Rendering** — Wu, Yi, Fang et al. CVPR 2024 ([openaccess.thecvf.com](https://openaccess.thecvf.com/content/CVPR2024/papers/Wu_4D_Gaussian_Splatting_for_Real-Time_Dynamic_Scene_Rendering_CVPR_2024_paper.pdf), [arxiv 2310.08528](https://arxiv.org/abs/2310.08528)). 82 FPS @ 800×800 en RTX 3090 con 4D neural voxels + HexPlane-inspired decomposition.
- **4D Gaussian Splatting: Modeling Dynamic Scenes with Native 4D Primitives** — arxiv 2412.20720.
- **ST-4DGS** SIGGRAPH 2024 + **4D-Rotor Gaussian Splatting** SIGGRAPH 2024 ([dl.acm.org/doi/10.1145/3641519.3657463](https://dl.acm.org/doi/10.1145/3641519.3657463)).
- **Anchored 4D Gaussian Splatting** SIGGRAPH Asia 2025 ([dl.acm.org/doi/10.1145/3757377.3763898](https://dl.acm.org/doi/10.1145/3757377.3763898)).
- **MEGA: Memory-Efficient 4D Gaussian Splatting** — ICCV 2025 ([openaccess.thecvf.com](https://openaccess.thecvf.com/content/ICCV2025/papers/Zhang_MEGA_Memory-Efficient_4D_Gaussian_Splatting_for_Dynamic_Scenes_ICCV_2025_paper.pdf)).
- **4DGS-1K**: 1000+ FPS dynamic 3DGS — NeurIPS 2025 ([openreview.net/forum?id=YbKdduMtyN](https://openreview.net/forum?id=YbKdduMtyN)).
- **Relightable 3D Gaussians** — ECCV 2024 ([dl.acm.org/doi/10.1007/978-3-031-72995-9_5](https://dl.acm.org/doi/10.1007/978-3-031-72995-9_5)). Descomposición BRDF + ray tracing sobre gaussians.
- **SSD-GS** — scattering and shadow decomposition relightable 3DGS ([arxiv 2604.13333](https://arxiv.org/html/2604.13333)).
- **Real-time Global Illumination for Dynamic 3D Gaussian Scenes** — arxiv 2503.17897.

### 3.2 Producción shipping (2026)

Qué ya está en engines:
- **Khronos + OGC**: `3D Gaussian Splats` extensión glTF anunciada Aug 2025, con SPZ compact container ([thefuture3d.com/blog/state-of-gaussian-splatting-2026](https://www.thefuture3d.com/blog/state-of-gaussian-splatting-2026/)).
- **Unreal 5 / Unity plugins** maduros ([polyvia3d.com/guides/gaussian-splatting-unity-unreal](https://www.polyvia3d.com/guides/gaussian-splatting-unity-unreal)).
- Hybrid UE5 Lumen showcases: 3DGS para environment backdrop (edificio escaneado) + mesh chars foreground.

Pero AAA shipping aún **no**. El bloqueador: iluminación dinámica (el gaussian captura la iluminación baked del scan), edits no-destructivos, física/collision, y LOD/streaming del splat cloud (los rasterizadores 3DGS son compute-based, no mesh-compatible).

### 3.3 Proyección 2026-2032

- **2026-2027**: 3DGS como `skybox-plus` / diorama de background. Games indie VR/AR. Photogrammetry replacement. Archviz comercial (ya en 3DVista, KIRI, Polycam).
- **2028-2029**: Relightable 3DGS estándar; integración Lumen-style donde el splat cloud se trata como una lighting probe gigante trazeable. PS6-era games incluyen scan backdrops.
- **2030+**: Híbridos 3DGS + mesh como primitivos rendering primary. Rasterizer dedicado en GPU (hoy es compute). Cinemáticas full-3DGS con actores capturados.

**Para un engine small-team**: en 2026 lo realista es integrar **viewer** de 3DGS (gsplat renderer compute shader) sin invertir en autoría. 500-1500 LOC con CPU sort + compute splat project. La compatibilidad con el mesh-based material pipeline es el gran pain point.

---

## 4. Virtualized geometry next-gen

### 4.1 Beyond Nanite — cluster BLAS + RTX Mega Geometry

El paso clave ya ocurrió 2025: **RTX Mega Geometry** (NVIDIA GDC 2025). Permite ray tracing sobre **Nanite-quality geometry** via:
- **Cluster-level BLAS** + **partitioned TLAS**. BVH builds amortizados por cluster DAG.
- Up to 100x triángulos ray-traceables vs BVH monolithic standard.
- Integrado en UE 5.6 RTX branch.
- [NVIDIA developer blog RTX Mega Geometry](https://developer.nvidia.com/blog/announcing-the-latest-nvidia-gaming-ai-and-neural-rendering-technologies/); [GDC 2025 session](https://www.nvidia.com/en-us/on-demand/session/gdc25-gdc1006/).

Alan Wake 2 (patch 2025) fue el primer título con RTX Mega Geometry shipping. El problema que resuelve: Nanite meshes tenían que ser rasterizados duplicando LOD simple para BVH o excluidos del RT. Ahora la misma DAG feeds RT.

### 4.2 Nanite skinned mesh — UE 5.5 / 5.6

UE 5.5 (Nov 2024) habilitó **Nanite Skeletal Mesh** ([UE forum](https://forums.unrealengine.com/t/nanite-skeletal-mesh-in-unreal-engine-5-5-main/1792367)). Combined con **Nanite Displacement Tessellation**. Hasta 5.5 el virtualized geometry estaba limitado a static meshes — personajes eran excluded. El algoritmo hace GPU skinning **antes** de cluster culling.

Pending en 5.6/5.7:
- **Nanite Foliage** in 5.7 ([notebookcheck](https://www.notebookcheck.net/Unreal-Engine-5-7-brings-Nanite-Foliage-MegaLights-and-major-visual-upgrades-in-tow.1162191.0.html)). Vegetación con masked materials finalmente compatible.
- **Nanite tessellation + displacement** mejora — continuous micro-tessellation sobre meshlets.

### 4.3 Micro-meshes NVIDIA + Intel

**NVIDIA Micro-Mesh** (RTX 40+, Ada). Compressed displacement + subdivision hasta nivel 5 (1024 triangles por base triangle) ([developer.nvidia.com/rtx/ray-tracing/micro-mesh](https://developer.nvidia.com/rtx/ray-tracing/micro-mesh), [nvpro-samples/vk_displacement_micromaps](https://github.com/nvpro-samples/vk_displacement_micromaps)). Formato `.bary` stored directamente consumible por raytracing APIs.

Extensión: `VK_NV_displacement_micromap` (NV-specific), camino a `VK_EXT_...` en roadmap.

Competencia: Intel trabajó en **Mesh Shading Sample** + presentó trabajo en dense geometry pero no tiene productizado un equivalente a Micromap shipping. AMD adoptó mesh nodes en work graphs pero no displacement micromaps nativos.

### 4.4 SDF / BRLIP / post-triangle primitives (research threads)

El consenso es que *los triángulos siguen ganando*. SDFs como primitivo de first-class rendering no están entrando al mainstream a 2032 — están entrando como **adjunto**:

- **Brixelizer** (AMD) — sparse distance fields real-time generation ([gpuopen.com/download/GDC-2023-Sparse-Distance-Fields-For-Games.pdf](https://gpuopen.com/download/GDC-2023-Sparse-Distance-Fields-For-Games.pdf)). Para soft shadows, GI probes, collision, **no** para rendering primary.
- **Neural Geometric Level of Detail** — research.nvidia.com/labs/toronto-ai/nglod. SDFs neural rendering pero a ~10-30 FPS sin neural accel dedicado. Research thread vivo, producto improbable <2029.
- **RTSDF** — arxiv 2210.06160. Real-time SDF para shadow approximation.

**Predicción**: Los triángulos + virtualized geometry son dominantes hasta **2030+**. SDFs ganan nicho en soft-body VFX, collision, field manipulation. Gaussian splatting les come el nicho de "dense captured geometry".

---

## 5. Virtualized lighting next-gen

### 5.1 Lumen 2026-2028

UE 5.6/5.7 cambios confirmados:
- **MegaLights** — Beta en 5.7 ([dev.epicgames.com/documentation/en-us/unreal-engine/megalights-in-unreal-engine](https://dev.epicgames.com/documentation/en-us/unreal-engine/megalights-in-unreal-engine)). Cientos/miles de light sources dynamic shadow-casting via clustered + stochastic sampling (Guillaume Werle at SIGGRAPH 2024). **Orders of magnitude more lights** vs shadow mapping clásico.
- **Software ray tracing (SWRT) deprecation** — Epic deprecated SWRT detail traces en 5.6, apuesta solo HWRT en 5.7 ([tomlooman.com/unreal-engine-5-7-performance-highlights](https://tomlooman.com/unreal-engine-5-7-performance-highlights/)). La era del "Lumen sin RT HW" termina.
- **HWRT at 60 Hz** target — Lumen HWRT + MegaLights + Nanite + VSM simultaneously at 60 FPS on RTX 4070+.
- **Interiores mejorados** — sharper reflections, indirect lighting mejor.

### 5.2 Unified hard+soft RT y caustics

La barrera 2026-2030 es **specular + caustics**. ReSTIR DI/GI manejan diffuse muy bien, specular glossy razonable, mirror-smooth excelente, pero:
- **Caustics** (agua, cristal, lentes): requiere Bidirectional Path Tracing (BDPT) o photon mapping. ReSTIR BDPT 2026 ([dl.acm.org/doi/10.1145/3744898](https://dl.acm.org/doi/10.1145/3744898)) es la primera propuesta real-time creíble.
- **Sky path tracing**: Alan Wake 2 y Cyberpunk PT ya sky-PT con fake sun. Full volumetric sky scattering PT sigue baked.

Predicción:
- **2027-2028**: caustics real-time en titles flagship NVIDIA/AMD sponsored. No default.
- **2029-2030**: BDPT integrado en Lumen/equivalent con NRC-style neural radiance cache para los paths largos.

### 5.3 Neural radiance cache productizado

NRC (Müller 2021) está **en Portal RTX shipped** y en desarrollo upstream Epic. RTX Neural Shaders SDK (Q4 2025 NVIDIA Gamescom) lo empaqueta. **SIGGRAPH Asia 2025 mobile NRC** ([dl.acm.org/doi/10.1145/3757376.3771399](https://dl.acm.org/doi/10.1145/3757376.3771399)) demuestra que la técnica baja a Snapdragon/Apple-class si hay cooperative matrix ops.

Productizable para small-team: sí, con SM 6.9 o Vulkan 1.4 + Slang + `tinynn` o xir / similar.

---

## 6. Mesh shaders + work graphs mainstream

### 6.1 Work Graphs 1.0 (GDC 2024) → 1.1 con mesh nodes (GDC 2025)

Ya cubierto en `r3/mesh_shaders_work_graphs.md`. Add-on 2025:
- **Mesh nodes** en D3D12 Work Graphs — GDC 2025 ([devblogs.microsoft.com/directx/directx-at-gdc-2025](https://devblogs.microsoft.com/directx/directx-at-gdc-2025/), [gpuopen.com/learn/work_graphs_mesh_nodes](https://gpuopen.com/learn/work_graphs_mesh_nodes/work_graphs_mesh_nodes-intro/)). Work graph que dispatches mesh-shader graphics pipeline directamente. **Draw calls as integral part of GPU work graphs**. AMD Radeon RX 7000/9000 + NVIDIA RTX support.
- **WorkGraphPlayground** — AMD GPUOpen sample oficial ([github.com/GPUOpen-LibrariesAndSDKs/WorkGraphPlayground](https://github.com/GPUOpen-LibrariesAndSDKs/WorkGraphPlayground/)).
- NVIDIA blog NV Work Graphs — [developer.nvidia.com/blog/advancing-gpu-driven-rendering-with-work-graphs-in-direct3d-12](https://developer.nvidia.com/blog/advancing-gpu-driven-rendering-with-work-graphs-in-direct3d-12/).

Vulkan equivalent: **VK_EXT_device_generated_commands** shipped Vulkan 1.3.296, Vulkanised 2025 ([rg3.name/202503111630](https://rg3.name/202503111630.html), [developer.nvidia.com/blog/new-vulkan-device-generated-commands](https://developer.nvidia.com/blog/new-vulkan-device-generated-commands/)). DGC es "una paso atrás de work graphs" — GPU genera sequences de commands ejecutables sin CPU roundtrip. No tan potente como work graphs, pero más portable.

### 6.2 Adoption curves

**Mesh shaders** (RTX 2000+, RDNA 2+, Arc Alchemist+, Apple M3+):
- Steam survey Q1 2026: ~60-65% de GPUs tienen mesh shaders. Safe baseline para nuevo engine **que targetee 2027+**.
- Mandatory skill en AAA: **sí ahora**. Requeridos para Nanite-style y para el path Work Graphs.

**Work Graphs** (RTX 4000+, RX 7000+, arriving Intel Xe2+):
- Q1 2026 baseline: ~30-35% GPU market. Aún minority.
- Mandatory skill: **no hasta 2028-2029**. PS6/Xbox next (RDNA 5) will support them natively — ese es el tipping point.
- Para small-team en 2026-2027: **no prioritario**, el ROI en cognitive load está en mesh shaders core.

---

## 7. Scalable content creation with AI

### 7.1 Estado del ecosystem 2026

Text-to-mesh productizado:
- **Meshy AI** — text-to-3D + auto-texture + retopo ([meshy.ai](https://www.meshy.ai/)). Dominante.
- **Tripo AI** — 50% faster full pipeline ([tripo3d.ai/game-development](https://www.tripo3d.ai/game-development/fast-text-to-3d-ai-generators-for-game-pipelines)).
- **Luma AI** — NeRF/3DGS captures → game asset.
- **Sloyd** — text-to-3D free tier ([sloyd.ai/text-to-3d](https://www.sloyd.ai/text-to-3d)).

Text-to-texture:
- **Substance 3D Sampler AI** — Adobe production standard.
- **StableGen** ([github.com/sakalond/StableGen](https://github.com/sakalond/StableGen)) — Stable Diffusion-based 3D texturing open source.
- **3D AI Studio** / **Meshy** ([meshy.ai/features/ai-texture-generator](https://www.meshy.ai/features/ai-texture-generator)).

Text-to-animation:
- **DeepMotion** — AI-driven motion capture de video ([80.lv/articles/deepmotion](https://80.lv/articles/deepmotion-ai-driven-motion-for-games)).
- **AI4Animation** — [github.com/sebastianstarke/AI4Animation](https://github.com/sebastianstarke/AI4Animation) research code.
- Sparse Mixture-of-Experts motion matching 2024 ([sciencedirect](https://www.sciencedirect.com/science/article/abs/pii/S0097849324000463)).

### 7.2 Implicaciones arquitecturales

Los asset pipelines están cambiando:

1. **Asset format → neural-asset format**. Meshes shipped as SDF + neural material vs mesh + BC textures. NTC demuestra viabilidad.
2. **DCC bridges directos** — Meshy/Tripo → UE/Unity sin file juggling ([aimagicx.com/blog/ai-texture-generator-game-development-2026](https://www.aimagicx.com/blog/ai-texture-generator-game-development-2026)).
3. **Run-time generation**: en vez de shipping millones de variaciones, generar on-device. Requiere neural shaders + tensor cores mobile (M5, Snapdragon X2, Blackwell mobile).
4. **PCG + AI híbridos**: PCG graphs para composición + AI para instances ([medium.com/@ascendion](https://medium.com/@ascendion/utilizing-gen-ai-for-procedural-content-generation-in-video-games-88601a04a42c)).

### 7.3 Para small-team

**Apostar**: Consumir Meshy/Tripo output vía glTF importers standard. Asset pipeline que acepta AI-generated assets sin fricción. Invest en **tooling** más que en generation models propios.

**No apostar**: Entrenar modelos propios de text-to-3D. El compute > $100k/run.

---

## 8. GPU architectures 2026-2030

### 8.1 NVIDIA

- **Blackwell (RTX 50)** — Q1 2025. 92B transistors (RTX 5090), 3352 TOPS, 5th gen Tensor Cores con FP4/FP8. Neural Shaders hardware feature. [NVIDIA Blackwell whitepaper](https://images.nvidia.com/aem-dam/Solutions/geforce/blackwell/nvidia-rtx-blackwell-gpu-architecture.pdf).
- **Rubin (RTX 60?)** — Q3 2026 data center, consumer probable Q2 2027. TSMC 3nm, HBM4, 224 SMs data-center variant, 5th-gen Tensor Cores con Transformer Engine, **50 PFLOPS NVFP4**. [developer.nvidia.com/blog/inside-the-nvidia-rubin-platform](https://developer.nvidia.com/blog/inside-the-nvidia-rubin-platform-six-new-chips-one-ai-supercomputer/).
- **Rubin Ultra** — H2 2027. NVL576 rack. 100 FP4 PFLOPS + 1TB HBM4E + quad-die package. [tomshardware: Nvidia roadmap](https://www.tomshardware.com/pc-components/gpus/nvidia-announces-rubin-gpus-in-2026-rubin-ultra-in-2027-feynam-after).
- **Feynman** — post-Rubin, confirmed on roadmap 2028-2029. Silicon photonics integration anunciada.

### 8.2 AMD

- **RDNA 4 (RX 9000)** — Q1 2025. Soporta FSR 4, Redstone ML upscaling. Radiance Cores separadas pero SER aún no implementado. [gpuopen.com/fidelityfx-super-resolution-4](https://gpuopen.com/fidelityfx-super-resolution-4/).
- **UDNA / RDNA 5** — mass production H2 2026, consumer mid-2027 ([en.gamegpu.com/News](https://en.gamegpu.com/News/zhelezo/sroki-vykhoda-rdna-5-perekhod-na-tsmc-n3p-i-perenos-reliza-na-2027-god), [jonpeddie.com](https://www.jonpeddie.com/news/amds-next-generation-gpu-architecture-udna-and-rdna-5/)). TSMC N3P. Features: Radiance Cores 2nd-gen + Neural Arrays. **2x RT perf vs RDNA 4**, 20% raster per CU. Unified with CDNA architecture bajo "UDNA" umbrella.
- **UDNA flagship "AT0"** — 96-CU halo tier return ([kad8.com/hardware/amd-rdna-5-udna-at0-96-cu-flagship-and-halo-return](https://www.kad8.com/hardware/amd-rdna-5-udna-at0-96-cu-flagship-and-halo-return/)).
- Dual-issue pipeline boost ([techpowerup.com/347389](https://www.techpowerup.com/347389/amd-rdna-5-to-heavily-boost-shader-performance-in-games-with-new-dual-issue-pipeline)).

### 8.3 Intel

- **Battlemage (Xe2)** — shipped 2024 (Arc B580, B570). Baseline RT + SER soporte via VK_EXT_ray_tracing_invocation_reorder.
- **Celestial (Xe3)** — late 2026 integrated (Panther Lake), discrete possibly early 2027 ([tomshardware.com/pc-components/gpus/intel-arc-xe3-celestial-gpu-enters-pre-validation-stage](https://www.tomshardware.com/pc-components/gpus/intel-arc-xe3-celestial-gpu-enters-pre-validation-stage)). 18A process. Pre-silicon validation May 2025.
- **Xe3P / Druid (Xe4)** — roadmap, dates pendientes ([kickstartgame.com/intel-gpu-roadmap-update-xe3p-next-arc-family](https://kickstartgame.com/intel-gpu-roadmap-update-xe3p-next-arc-family/)). Arc C-series naming switch.

### 8.4 Apple

- **M4** — Nov 2024. Third-gen RT cores pero sin neural accel per GPU core ([wikipedia M4](https://en.wikipedia.org/wiki/Apple_M4)). 10-core GPU.
- **M5** — Oct 2025. **Dedicated Neural Accelerator within every GPU core** — cambio arquitectónico crítico. 4x peak GPU compute vs M4 para AI workloads. 45% ray-tracing uplift. [apple.com/newsroom/2025/10/apple-unleashes-m5](https://www.apple.com/newsroom/2025/10/apple-unleashes-m5-the-next-big-leap-in-ai-performance-for-apple-silicon/).
- **M6** — expected late 2026 / 2027. TSMC N2.

### 8.5 Consoles next-gen

- **PS6** — target Holiday 2027, possible slip 2028. AMD Zen 6c + 52-54 RDNA 5 CUs. GDDR7 ~640 GB/s. Project Amethyst ML HW custom. [wccftech PS6](https://wccftech.com/roundup/playstation-6-everything-we-know-release-date-specs-price-games/).
- **Xbox Project Helix** — target Holiday 2027. Zen 6 3+8c cores, 68 RDNA 5 CUs. Más GPU que PS6. Alpha devkits 2027. [overclock3d.net PS6 vs Xbox](https://overclock3d.net/news/misc/playstation-6-vs-xbox-next-project-helix-leaked-specifications-compared/).
- Ambas: unified neural HW, hardware Work Graphs execution, RTX-Mega-Geometry-style BLAS clusters.

### 8.6 Qualcomm / ARM handheld

- **Snapdragon X Elite (Adreno X1)** — 2024. **Sin** hardware ray tracing. [Tom's Guide X Elite gaming](https://www.tomsguide.com/computing/laptops/snapdragon-x-elite-is-so-much-better-for-gaming-than-i-expected-heres-our-first-test-results).
- **Snapdragon X2 Elite (Adreno X2)** — CES 2026. **Hardware ray tracing + DX12 Ultimate + Vulkan 1.4** ([tomshardware.com](https://www.tomshardware.com/laptops/qualcomms-18-core-snapdragon-x2-elite-extreme-dominates-in-first-benchmarks-18-cores-and-48gb-of-on-package-memory-on-a-192-bit-bus-look-tough-to-beat)). 2.3x perf/W.
- **Steam Deck 2** — rumors late 2026 / early 2027 con custom AMD "Magnus" APU (RDNA 4-based likely).
- **ROG Ally X / Xbox Ally X** — Q1 2026, Ryzen Z2 Extreme (RDNA 3.5, no RT HW relevante).

**Lección**: el 50% del market (handhelds, mobile, integrated) **no tiene RT útil hasta 2027+**. Pagar fallback path siempre.

---

## 9. APIs evolution

### 9.1 D3D12 Agility SDK

- **1.616** Oct 2025 — Work Graphs mesh nodes.
- **1.619** Feb 2026 — **SM 6.9 retail**, DXR 1.2 full (SER + OMM), Cooperative Vectors ([devblogs.microsoft.com/directx/shader-model-6-9-retail-and-more](https://devblogs.microsoft.com/directx/shader-model-6-9-retail-and-more/)).
- **SM 6.10 roadmap** — Cooperative Matrix2 (matrix-matrix unificada), 2026-2027.
- **Neural Rendering** / PIX neural tooling GDC 2025 ([devblogs.microsoft.com/directx/announcing-directx-raytracing-1-2-pix-neural-rendering-and-more-at-gdc-2025](https://devblogs.microsoft.com/directx/announcing-directx-raytracing-1-2-pix-neural-rendering-and-more-at-gdc-2025/)).

### 9.2 Vulkan

- **Vulkan 1.4** — Dec 2024. Push descriptors mandatory, dynamic rendering local reads, scalar block layouts, VK_EXT_device_generated_commands. Streaming Transfers ([vulkan.org news](https://www.vulkan.org/news/auto-23109-c508dee929394e7b3be03eeaba100df1), [khronos.org/blog/vulkan-continuing-to-forge-ahead-siggraph-2025](https://www.khronos.org/blog/vulkan-continuing-to-forge-ahead-siggraph-2025)).
- **Vulkan 1.5** (expected 2026-2027) — likely cooperative matrix2 core, Work Graphs-equivalent standard, neural shader extensions core.
- **VK_EXT_ray_tracing_invocation_reorder** (2024) — SER multi-vendor.
- **VK_NV_cooperative_vector** (2025) — neural ops.

### 9.3 Metal 4

- **WWDC 2025** Metal 4 ([developer.apple.com/videos/play/wwdc2025/211](https://developer.apple.com/videos/play/wwdc2025/211/)):
  - Tensors as first-class citizens en Metal Shading Language.
  - AI inference directly inside shaders (no framework calls).
  - MetalFX Frame Interpolation + integrated denoiser.
  - Unified command encoder system.

### 9.4 Slang (cross-API)

Ya cubierto §2.3. Clave: `modular shader code + portable deployment + neural computation` para un small-team es un **game-changer**. Escribir un shader una vez, deployar a D3D12/Vulkan/Metal/CUDA/WebGPU. [shader-slang.org](http://shader-slang.org/), [github.com/shader-slang](https://github.com/shader-slang).

Adopters shipping: Valve Source 2 (CS2, Dota 2). Khronos Vulkan samples en Slang ([saschawillems.de 2025](https://www.saschawillems.de/blog/2025/06/03/shaders-for-vulkan-samples-now-also-available-in-slang/)).

### 9.5 WebGPU

Production-ready achieved **Nov 2025**:
- Chrome/Edge (since 113, May 2023), Firefox 141+ (2025), **Safari 26 (Sep 2025)** macOS/iOS/iPadOS/visionOS ([web.dev/blog/webgpu-supported-major-browsers](https://web.dev/blog/webgpu-supported-major-browsers), [videocardz.com](https://videocardz.com/newz/webgpu-is-now-supported-by-all-major-browsers)).
- 70% browser support (Q1 2026), 65% new web apps adopting ([byteiota.com](https://byteiota.com/webgpu-2026-70-browser-support-15x-performance-gains/)).
- Linux + Android: throughout 2026 (Mozilla).
- Three.js r171+ auto WebGPU fallback.

**Limitación**: WebGPU 1.0 no tiene ray tracing, no mesh shaders, no work graphs, no neural ops. Es un subset D3D11 / Vulkan 1.1 class. WebGPU 1.1 planeado para 2027+ con extensiones opcionales.

---

## 10. Power efficiency + thermal — mobile/handheld pressure

### 10.1 Presión térmica como driver arquitectónico

El handheld market (Steam Deck, ROG Ally, Legion Go, Switch 2, Xbox Ally) ya supera PS4 en unidades shipped. Esto cambia el equilibrio:

- **Mobile gaming** ~$100B/año, 50%+ del market. GPUs mobile han **saltado generaciones de RT**: Snapdragon X1 Adreno sin RT → X2 con RT HW. Apple M1 (sin RT) → M3 (RT 1gen) → M5 (RT 3gen + neural per core). ARM Immortalis-G720 + RT.
- **Steam Deck 2** rumors point a RDNA 4-class integrated APU — primera handheld con RT útil. 2026-2027.

### 10.2 Implicaciones arquitecturales para el engine

1. **Async compute** no es opcional — mobile GPU tiles son más sensible a stalls que desktop. Fino-grained overlap compute/gfx es 10-20% perf gratis.
2. **Sparse workloads** — NRC / neural rendering inference puede correr en tensor cores mientras el gfx pipeline sigue. Pero coherence (shared caches) mata si no se orquesta.
3. **Bandwidth-bound** — mobile tiene 30-50 GB/s de bandwidth vs RTX 5090 @ 1.8 TB/s. VT + neural texture compression bajan el bandwidth footprint 10-20x. Crítico.
4. **Low-power RT cores** — Apple M3/M4/M5 RT cores optimizan ops/W, no ops/s. ReSTIR DI inline es ok; PT no.

**Decisión arquitectónica para engine small-team**:
- **Siempre** mantener un path non-RT funcional (usable, no solo fallback). El 50%+ del market lo requiere.
- Dos pipelines lightning: `rast-only` (mobile, old desktop) + `hybrid RT` (RTX 2000+, M3+, Arc+). Full PT path opcional, no default.

---

## 11. Research threads que aún no son productos pero podrían ser

### 11.1 Differentiable rendering en pipeline artístico

- **Mitsuba 3 + Dr.Jit** — EPFL Realistic Graphics Lab ([mitsuba-renderer.org](http://www.mitsuba-renderer.org/), [github.com/mitsuba-renderer/mitsuba3](https://github.com/mitsuba-renderer/mitsuba3)). Diff rendering maduro, production tool para research.
- **Dr.Jit** — specialized JIT compiler ([github.com/mitsuba-renderer/drjit-core](https://github.com/mitsuba-renderer/drjit-core)).
- **Slang auto-diff** — first-class feature de Slang. Un shader Slang se puede auto-diferenciar.

Uso productivo en games: no visto aún AAA. **Probable 2028-2030**: authoring tools que auto-optimizan materiales/luces por target visual definido artísticamente. "Match this reference" → auto-tune parameters.

### 11.2 Implicit surfaces + NeRF-inspired BRDFs

- **PBR-NeRF** — CVPR 2025 ([openaccess.thecvf.com](https://openaccess.thecvf.com/content/CVPR2025/papers/Wu_PBR-NeRF_Inverse_Rendering_with_Physics-Based_Neural_Fields_CVPR_2025_paper.pdf)). NeRF + SDF + NeILF + BRDF field joint optimization.
- **Neural Global Illumination via Superposed Deformable Feature Fields** — SIGGRAPH Asia 2024 ([dl.acm.org/doi/10.1145/3680528.3687680](https://dl.acm.org/doi/10.1145/3680528.3687680)). Feature grids neurales que modelan light transport.
- **Relightable 3D Gaussians** — ECCV 2024 (citado §3).

Timeline: research-to-product 3-5 años. **2028-2030** para integrate en motores production.

### 11.3 Learned global illumination

- **NRC** (productizado — Portal RTX, flags en otros titles).
- **Gaussian Hash Grids** (Müller 2022 Instant-NGP derivatives). Research vivo para radiance cache compacto.
- **Hash-based PT acceleration** — parallel work threads pero sin product.

### 11.4 Neural BRDFs compactos

Neural materials of Zeltner 2024 (citado §2) es el más cerca de product. Su evolución probable:
- **2026-2027**: SDK NVIDIA/AMD para authoring.
- **2028-2029**: integración engine UE/Unity.
- **2030**: default en AAA material pipelines.

---

## 12. Qué pasa con los game engines

### 12.1 UE5 dominancia

Trajectory 2026:
- **UE 5.6** Q2 2026 — RTX Mega Geometry integration, Nanite skinning GA, Lumen HWRT 60Hz target.
- **UE 5.7** H2 2026 — MegaLights Beta, Nanite Foliage, SWRT deprecated.
- UE5 gains: Unity's fall post-2023 runtime-fee scandal, Frostbite + in-house engines slow migration.

**Risks** 2028-2032:
- Epic's strategic pivot a **Fortnite UEFN + Metaverse**. Se prioriza pipeline UEFN over standalone engine feature shipping. [sesamedisk.com/uefn-2026](https://sesamedisk.com/uefn-2026-game-development-revolution/), [sccgmanagement.com](https://sccgmanagement.com/sccg-news/2025/12/26/fortnite-in-2026-evolving-into-a-full-fledged-metaverse-hub/).
- Creator Economy 2.0: 100% rev share 2025-2026 → 74% average avg de revenue total ([epicgames.com/site/en-US/news](https://www.epicgames.com/site/en-US/news/introducing-unreal-editor-for-fortnite-creator-economy-2-0-fab-and-more)).
- **$722M payouts to creators**, 11.2B horas de gameplay across 260k creator islands.
- Unity partnership: Unity games playable **dentro** de Fortnite a partir 2026.

### 12.2 Unity recovery

Unity 6 (2024) fixed runtime-fee scandal, rewrote URP/HDRP, maduró job system + Entities (ECS). Adoption actual ([game-developers.org/unity-6-vs-unreal-engine-5-comparison-2026](https://www.game-developers.org/unity-6-vs-unreal-engine-5-comparison-2026)):
- Mayor en mobile (80%+), similar en indie, minoría en AAA-adjacent.
- Unity 6.3 + Unreal 5.7 son los baselines 2026.

### 12.3 Godot enterprise

Godot adoption surge reported 2024-2026 ([pocketgamer.biz cross-industry](https://www.pocketgamer.biz/cross-industry-game-engine-adoption-surges-led-by-unreal-unity-and-godot/)):
- 15% education, 19% AEC, 21% automotive/manufacturing (2025).
- 95k+ GitHub stars early 2026.
- 80k+ Discord actives.

Limits: no Nanite equivalent, no Lumen equivalent, RT via raymarch SDFs. Godot 4.x Vulkan renderer respectable para 2D + low-to-mid 3D. No AAA-tier aún.

### 12.4 Bevy (Rust)

- 0.14 → 0.16 (jms55) — virtualized geometry implementation, Nanite-style ([bevy github](https://github.com/bevyengine/bevy)). 18k+ GitHub stars.
- ECS + WASM nativo + WebGPU — excelente para web/cross-platform.
- Enterprise/AAA: no. Indie + web + research.

### 12.5 Tendencia "game-engine-as-a-service"

- **Roblox** — $3B+ revenue, 70M+ DAU. Engine + platform bundled.
- **UEFN** — Epic's pivot.
- **Minecraft Bedrock** — Microsoft scripting API maduro.
- **Unity partnership en Fortnite** 2026 — unprecedented.

**Implicación**: el modelo "engine que shipeas tu juego standalone" compite directo contra "engine que shipeas tu juego dentro de platform". Para small-team sin marketing budget, **platform > standalone** es la trade-off obligatoria en 2028+.

---

## 13. Tabla consolidada — cuándo llega cada tech a mainstream

*Mainstream = usable por el 50%+ del hardware market, default en AAA shipping*

| Tecnología | Hoy (2026) | Mid-tier mainstream | AAA default | Mobile/handheld |
|------------|-----------|----------------------|-------------|-----------------|
| DXR 1.1 inline RT | ✅ default | ✅ | ✅ | 2026-2027 (X2 Elite, M3+) |
| DXR 1.2 (SER, OMM) | opcional | 2026-2027 | 2027 | 2028+ |
| ReSTIR DI/GI | opcional | 2026-2027 | 2027-2028 | 2030+ (neural accel) |
| ReSTIR PT (full PT) | showcase | 2028-2029 | 2029-2030 | no realistic |
| ReSTIR BDPT + caustics | paper 2026 | 2029-2030 | 2030-2031 | no |
| NRC | shipped (Portal RTX) | 2027-2028 | 2028-2029 | 2029-2030 |
| DLSS SR Transformer | ✅ (RTX 20+) | ✅ | ✅ | N/A (vendor lock) |
| FSR 4 ML | RDNA 4 only | 2027 (RDNA 5) | 2027-2028 | 2028+ |
| Multi-frame gen (DLSS MFG 4x, FSR Redstone FG) | RTX 50, RDNA 4 | 2028 | 2029 | 2030+ |
| Neural Texture Compression | SDK beta Q4 2025 | 2027-2028 | 2028-2029 | 2029 (bandwidth-critical) |
| Neural materials (Zeltner) | research | 2028 | 2029-2030 | 2030+ |
| Neural motion matching | proven | 2027 | 2028-2029 | 2028+ |
| 3D Gaussian Splatting (static) | plugins mature | 2027 (hybrid) | 2028-2029 | 2027+ (compute-light) |
| 4D Gaussian Splatting (dynamic) | research | 2028-2029 | 2030+ | 2031+ |
| Relightable 3DGS | research 2024-2025 | 2029-2030 | 2031+ | post-2032 |
| Nanite (virtualized geometry) | UE5 default | 2026-2027 (UE5 ports) | 2027-2028 | 2029+ (PS6-class) |
| Nanite skinned | UE 5.5+ | 2027 | 2028 | 2029-2030 |
| RTX Mega Geometry | UE 5.6 branch | 2027 | 2028 | 2029-2030 |
| Micro-meshes displacement | NVIDIA only | 2027-2028 (Khronos EXT) | 2028-2029 | post-2030 |
| Lumen HWRT only | UE 5.7 trend | 2027 | 2028 | 2030+ |
| MegaLights | UE 5.5 exp → 5.7 beta | 2027-2028 | 2028-2029 | 2029-2030 |
| Mesh shaders | baseline RTX 20+ | ✅ | ✅ | 2026-2027 (M3+, X2, Arc) |
| Work Graphs (compute only) | RTX 40+, RDNA 3+ | 2027-2028 | 2028-2029 | 2030+ |
| Work Graphs + mesh nodes | GDC 2025, driver only | 2028 | 2029 | 2030+ |
| Cooperative Vectors (SM 6.9) | Agility 1.619 (Feb 2026) | 2027 | 2028-2029 | 2028+ |
| D3D12 Neural Shaders | shipping CX25 | 2027-2028 | 2028-2029 | 2028+ |
| WebGPU 1.0 | Nov 2025 all browsers | ✅ | ✅ (web) | ✅ |
| WebGPU 1.1 RT + mesh | 2027+ | 2028-2029 | 2029-2030 | 2029-2030 |
| Slang | Khronos Nov 2024 | 2026-2027 | 2028 | 2028 |
| Text-to-mesh AI pipeline | Meshy/Tripo prod | 2026-2027 | 2028 indie, 2029-2030 AAA | 2027 |
| Differentiable rendering in engines | Mitsuba research | 2028-2030 | 2031+ | post-2032 |
| Full PT default AAA | opcional | 2028 (PS6 gen) | 2029-2030 | post-2032 |

---

## 14. Para un engine small-team en 2026 — **qué apostar y qué NO**

Target ALZE: ~25-30k LOC C++17 no-RTTI/no-exceptions, SDL2 + OpenGL 3.3 hoy, Vulkan 1.3+ mañana, single-dev.

### 14.1 APOSTAR — ROI alto, tecnología estable, coste realista

1. **Mesh shaders + meshlets offline baked** (via meshoptimizer). Baseline RTX 2000+, RDNA 2+, M3+. Al volver el pipeline clásico obsoleto, esto es el paso 1 para Nanite-lite. ~3k LOC incluyendo task shader.

2. **Visibility buffer rendering** (64-bit atomics ARB extension). Desacopla geometría de materiales, unlocks Nanite-style ruta. ~1.5k LOC. Coste: GL 3.3 no sirve, Vulkan 1.3 es la puerta.

3. **Frame graph + bindless resources** (ver `r3/frame_graph_bindless.md`). Si no lo tienes en 2026, tu engine no escala. ~2k LOC para un FG decente.

4. **Inline ray queries (DXR 1.1 / VK_KHR_ray_query)** para RT shadows + RT AO + RT probe update. **No** path tracing completo. Ese es el sweet-spot quality/budget en 2026-2028 para small-team. ~1k LOC integration.

5. **FSR 2 / DLSS 2 (via Streamline) integration como first-class feature**. Jitter projection + motion vectors + mip bias correcto. Una semana de dev. Obligatorio para 4K.

6. **Slang como shader language** en lugar de HLSL o GLSL puro. Un shader, múltiples backends (Vulkan SPIR-V + future WebGPU + future Metal). Esto es un bet de 3-5 años — en 2029 Slang probablemente es mainstream.

7. **Bindless texture arrays + descriptor indexing**. Vulkan 1.2+ feature. Simplifica pipelines neural (donde cada material tiene un weight tensor distinto). ~200 LOC de integration.

8. **glTF + KTX2 + Basis Universal** como asset standard. 3DGS extensión glTF 2025+. Text-to-mesh output. Zero bet, infraestructura gratuita.

9. **Virtual textures (sparse residency simple)**, cubierto `r3/virtual_textures_streaming.md`. ~2-3k LOC. Bandwidth savings 10x+, crítico para mobile future.

### 14.2 NO APOSTAR — ROI bajo, complejidad prohibitiva, o timing wrong

1. **❌ Full Nanite clone**. 6-12 person-years según Karis + Bevy jms55 threads. Sub-aspect (cluster culling HW, two-pass HZB) sí — pero no el sistema completo.

2. **❌ Full Lumen clone**. Surface cache + mesh cards + GDF/MDF + two-level radiance cache = multi-year team job. Alternativa: **un GI más simple** — DDGI (DDGIVolume / RTXGI open source) o sigue con probes estáticas + SSGI.

3. **❌ Full path tracing as default render path**. Requiere stack neural (NRC + NRD + ReSTIR + SER + RR) que single-dev no puede sostener. Opcional "demo mode" sí, default no.

4. **❌ 3D Gaussian Splatting authoring + edit tools**. El ecosystem (KIRI, Polycam, Luma, Meshy) te lo da. Solo integra viewer/renderer.

5. **❌ Work Graphs** — aún no hay 50% market share hasta 2028-2029. Invest ahí a prioritizar va antes de haber bineado bien el mesh shader path. Prematuro.

6. **❌ Modelos propios text-to-3D / text-to-texture**. Compute cost prohibitivo ($100k+ training runs). Consume output de Meshy/Tripo vía glTF.

7. **❌ Differentiable rendering engine** propio. Research, no product. Si quieres auto-diff, usa Slang que lo tiene built-in.

8. **❌ Neural rendering desde cero**. Con Slang + Cooperative Vectors (SM 6.9) + RTX Neural SDK evalúas redes existentes. Entrenar desde cero es otra empresa.

9. **❌ Software SDF raytracer engine-primary** (à la Lumen SW). Epic lo deprecated en 5.6/5.7. El viento sopla hacia HWRT only. Hardware base del 2028+ (PS6, RDNA 5) lo asume.

10. **❌ Virtualized anything a 4K +120Hz en handheld**. El equilibrio bandwidth + compute no existe aún. Ver §10.

11. **❌ DX12-only stack**. Windows-only + Xbox-only. PS5/PS5 Pro requiere GNMX/PSSL. Cross-platform engines shippeables 2026+ elegiran **Vulkan + Metal + GNMX backend**. Slang ayuda.

12. **❌ WebGPU como único target renderer** si el game es AAA-class. WebGPU 1.0 no tiene RT, no mesh shaders, no work graphs, no neural ops hasta 1.1+ (2027+). Para web demos / tools OK.

### 14.3 Resumen ejecutivo en 3 líneas

- **Invest 2026-2028**: mesh shaders + visibility buffer + frame graph + bindless + inline RT (shadows+AO) + FSR2/DLSS + Slang + virtual textures.
- **Watch 2027-2029**: Work Graphs, Cooperative Vectors, RTX Mega Geometry, Neural Texture Compression, 3DGS hybrid pipelines, RDNA 5/Rubin neural shaders.
- **Skip unless pivot**: Full PT default, full Nanite/Lumen clone, 4D Gaussian authoring, custom text-to-3D, differentiable engine, WebGPU-only.

**Regla operacional**: cada tecnología que tomes, verifica que (a) está en ≥1 motor shipping AAA, (b) el stack open-source o SDK existe, (c) el hardware baseline es ≥40% Steam survey Q1 del año objetivo. Si dos de tres faltan, **NO**.

---

## Apéndice A — Fuentes primarias consultadas (≥40 URLs)

### Papers académicos (SIGGRAPH / CVPR / ICCV / NeurIPS / ACM TOG)

1. Bitterli et al. "Spatiotemporal reservoir resampling (ReSTIR DI)". SIGGRAPH 2020. https://research.nvidia.com/publication/2020-07_Spatiotemporal-reservoir-resampling-real-time-ray-tracing-dynamic-direct
2. Ouyang et al. "ReSTIR GI". HPG 2021. https://research.nvidia.com/publication/2021-06_restir-gi-path-resampling-real-time-path-tracing
3. Müller et al. "Real-time Neural Radiance Caching". SIGGRAPH 2021. https://tom94.net/data/publications/mueller21realtime/mueller21realtime.pdf
4. Lefohn et al. "Shader Execution Reordering". SIGGRAPH 2022. https://research.nvidia.com/sites/default/files/pubs/2022-07_Shader-Execution-Reordering/ShaderExecutionReordering.pdf
5. Lin, Kettunen, Bitterli, Pantaleoni, Yuksel, Wyman, Pharr. "Generalized Resampled Importance Sampling: Foundations of ReSTIR". SIGGRAPH 2022. https://research.nvidia.com/publication/2022-07_generalized-resampled-importance-sampling-foundations-restir
6. Lin et al. "ReSTIR PT Enhanced". NVIDIA RTR 2026. https://research.nvidia.com/labs/rtr/publication/lin2026restirptenhanced/
7. Vaidyanathan, Salvi, Wronski, Akenine-Möller, Ebelin, Lefohn. "Random-Access Neural Compression of Material Textures". SIGGRAPH 2023. https://research.nvidia.com/publication/2023-08_random-access-neural-compression-material-textures
8. Zeltner et al. "Real-Time Neural Appearance Models". ACM TOG 2024. https://research.nvidia.com/labs/rtr/neural_appearance_models/
9. Kerbl et al. "3D Gaussian Splatting for Real-Time Radiance Field Rendering". SIGGRAPH 2023.
10. Wu, Yi, Fang et al. "4D Gaussian Splatting for Real-Time Dynamic Scene Rendering". CVPR 2024. https://arxiv.org/abs/2310.08528
11. "Anchored 4D Gaussian Splatting for Dynamic Novel View Synthesis". SIGGRAPH Asia 2025. https://dl.acm.org/doi/10.1145/3757377.3763898
12. "ReSTIR BDPT: Bidirectional ReSTIR Path Tracing with Caustics". ACM TOG 2026. https://dl.acm.org/doi/10.1145/3744898
13. "ReSTIR PG: Path Guiding with Spatiotemporally Resampled Paths". SIGGRAPH Asia 2025. https://dl.acm.org/doi/10.1145/3757377.3763813
14. "Neural Radiance Cache Implementation on Mobile GPU". SIGGRAPH Asia 2025. https://dl.acm.org/doi/10.1145/3757376.3771399
15. "Neural Graphics Texture Compression Supporting Random Access". ECCV 2024. https://arxiv.org/html/2407.00021
16. "Neural Global Illumination via Superposed Deformable Feature Fields". SIGGRAPH Asia 2024. https://dl.acm.org/doi/10.1145/3680528.3687680
17. "Relightable 3D Gaussians". ECCV 2024. https://dl.acm.org/doi/10.1007/978-3-031-72995-9_5
18. "Wu et al. PBR-NeRF". CVPR 2025. https://openaccess.thecvf.com/content/CVPR2025/papers/Wu_PBR-NeRF_Inverse_Rendering_with_Physics-Based_Neural_Fields_CVPR_2025_paper.pdf
19. "Neural BRDF Importance Sampling by Reparameterization". SIGGRAPH 2025. https://dl.acm.org/doi/10.1145/3721238.3730679
20. "Learned Motion Matching" Ubisoft. ACM TOG 2020. https://dl.acm.org/doi/abs/10.1145/3386569.3392440
21. "Motion Matching + Sparse MoE + Lipschitz". 2024. https://www.sciencedirect.com/science/article/abs/pii/S0097849324000463
22. "ST-4DGS". SIGGRAPH 2024. https://dl.acm.org/doi/abs/10.1145/3641519.3657520
23. "4D-Rotor Gaussian Splatting". SIGGRAPH 2024. https://dl.acm.org/doi/10.1145/3641519.3657463

### Hardware whitepapers y roadmaps oficiales

24. NVIDIA RTX Blackwell GPU Architecture whitepaper. 2025. https://images.nvidia.com/aem-dam/Solutions/geforce/blackwell/nvidia-rtx-blackwell-gpu-architecture.pdf
25. NVIDIA Vera Rubin Platform inside. 2025. https://developer.nvidia.com/blog/inside-the-nvidia-rubin-platform-six-new-chips-one-ai-supercomputer/
26. NVIDIA Rubin microarchitecture Wikipedia summary. https://en.wikipedia.org/wiki/Rubin_(microarchitecture)
27. Tom's Hardware: "NVIDIA announces Rubin GPUs in 2026, Rubin Ultra 2027". https://www.tomshardware.com/pc-components/gpus/nvidia-announces-rubin-gpus-in-2026-rubin-ultra-in-2027-feynam-after
28. AMD GPUOpen FSR 4. https://gpuopen.com/fidelityfx-super-resolution-4/
29. AMD FSR Redstone developer docs. https://gpuopen.com/learn/amd-fsr-redstone-developers-neural-rendering/
30. AMD Work Graphs mesh nodes. https://gpuopen.com/learn/work_graphs_mesh_nodes/work_graphs_mesh_nodes-intro/
31. Intel Arc Celestial pre-validation. https://www.tomshardware.com/pc-components/gpus/intel-arc-xe3-celestial-gpu-enters-pre-validation-stage
32. Apple M5 announcement. https://www.apple.com/newsroom/2025/10/apple-unleashes-m5-the-next-big-leap-in-ai-performance-for-apple-silicon/
33. PS6/Xbox Project Helix leaks compilation. https://wccftech.com/roundup/ps6-vs-xbox-next-project-helix-everything-we-know/
34. PSSR PlayStation Blog upgrade. https://blog.playstation.com/2026/02/27/upgraded-pssr-upscaler-is-coming-to-ps5-pro/
35. Qualcomm Snapdragon X2 Elite Tom's Hardware. https://www.tomshardware.com/laptops/qualcomms-18-core-snapdragon-x2-elite-extreme-dominates-in-first-benchmarks-18-cores-and-48gb-of-on-package-memory-on-a-192-bit-bus-look-tough-to-beat
36. AMD UDNA/RDNA 5 Jon Peddie. https://www.jonpeddie.com/news/amds-next-generation-gpu-architecture-udna-and-rdna-5/
37. Desktop GPU roadmap Tom's Hardware. https://www.tomshardware.com/pc-components/gpus/desktop-gpu-roadmap-nvidia-rubin-amd-udna-and-intel-xe3-celestial

### APIs, SDKs y spec docs

38. DirectX Cooperative Vector. https://devblogs.microsoft.com/directx/cooperative-vector/
39. Announcing Shader Model 6.9 retail. https://devblogs.microsoft.com/directx/shader-model-6-9-retail-and-more/
40. SM 6.9 and future of Cooperative Vector. https://devblogs.microsoft.com/directx/shader-model-6-9-and-the-future-of-cooperative-vector/
41. DXR 1.2 + PIX neural. https://devblogs.microsoft.com/directx/announcing-directx-raytracing-1-2-pix-neural-rendering-and-more-at-gdc-2025/
42. DirectX at GDC 2025. https://devblogs.microsoft.com/directx/directx-at-gdc-2025/
43. D3D12 Work Graphs. https://devblogs.microsoft.com/directx/d3d12-work-graphs/
44. Vulkan Device-Generated Commands NVIDIA blog. https://developer.nvidia.com/blog/new-vulkan-device-generated-commands/
45. Vulkan SIGGRAPH 2025. https://www.khronos.org/blog/vulkan-continuing-to-forge-ahead-siggraph-2025
46. Vulkan 1.4 release news. https://www.vulkan.org/news/auto-23109-c508dee929394e7b3be03eeaba100df1
47. Slang Khronos initiative. https://www.khronos.org/news/press/khronos-group-launches-slang-initiative-hosting-open-source-compiler-contributed-by-nvidia
48. Slang shading language site. http://shader-slang.org/
49. Apple Metal 4 WWDC25. https://developer.apple.com/videos/play/wwdc2025/211/
50. WebGPU supported all browsers. https://web.dev/blog/webgpu-supported-major-browsers
51. WebGPU Nov 2025 milestone. https://videocardz.com/newz/webgpu-is-now-supported-by-all-major-browsers
52. RTX Kit (neural rendering umbrella). https://developer.nvidia.com/rtx-kit
53. RTX NTC SDK repo. https://github.com/NVIDIA-RTX/RTXNTC
54. NVIDIA Micro-Mesh. https://developer.nvidia.com/rtx/ray-tracing/micro-mesh
55. NVIDIA Displacement MicroMap Vulkan sample. https://github.com/nvpro-samples/vk_displacement_micromaps
56. NVIDIA Neural Rendering in OptiX Cooperative Vectors. https://developer.nvidia.com/blog/neural-rendering-in-nvidia-optix-using-cooperative-vectors/
57. RTX Mega Geometry NVIDIA Gamescom 2025. https://developer.nvidia.com/blog/announcing-the-latest-nvidia-gaming-ai-and-neural-rendering-technologies/
58. NVIDIA Work Graphs D3D12. https://developer.nvidia.com/blog/advancing-gpu-driven-rendering-with-work-graphs-in-direct3d-12/
59. WorkGraphPlayground AMD. https://github.com/GPUOpen-LibrariesAndSDKs/WorkGraphPlayground/
60. DLSS 4 research page. https://research.nvidia.com/labs/adlr/DLSS4/
61. DLSS 4.5 GeForce news. https://www.nvidia.com/en-us/geforce/news/dlss-4-5-dynamic-multi-frame-gen-6x-2nd-gen-transformer-super-res/
62. Enabling Neural Rendering in DirectX. https://devblogs.microsoft.com/directx/enabling-neural-rendering-in-directx-cooperative-vector-support-coming-soon/

### Engines + industria

63. UE 5.7 MegaLights docs. https://dev.epicgames.com/documentation/en-us/unreal-engine/megalights-in-unreal-engine
64. UE 5.7 highlights Tom Looman. https://tomlooman.com/unreal-engine-5-7-performance-highlights/
65. UE 5.5 highlights Tom Looman. https://tomlooman.com/unreal-engine-5-5-performance-highlights/
66. UE 5.5 Nanite skeletal mesh Epic forums. https://forums.unrealengine.com/t/nanite-skeletal-mesh-in-unreal-engine-5-5-main/1792367
67. Indiana Jones PT benchmarks. https://www.dsogaming.com/articles/indiana-jones-and-the-great-circle-path-tracing-benchmarks/
68. NVIDIA Research Dynamic GI tiny network 2025. https://developer.nvidia.com/blog/nvidia-research-learning-and-rendering-dynamic-global-illumination-with-one-tiny-neural-network-in-real-time/
69. NVIDIA RTX Neural Rendering intro. https://developer.nvidia.com/blog/nvidia-rtx-neural-rendering-introduces-next-era-of-ai-powered-graphics-innovation/
70. Epic UEFN Creator Economy 2.0. https://www.epicgames.com/site/en-US/news/introducing-unreal-editor-for-fortnite-creator-economy-2-0-fab-and-more
71. Meshy AI. https://www.meshy.ai/
72. Tripo AI game pipelines. https://www.tripo3d.ai/game-development/fast-text-to-3d-ai-generators-for-game-pipelines
73. Mitsuba 3. http://www.mitsuba-renderer.org/
74. Dr.Jit core. https://github.com/mitsuba-renderer/drjit-core
75. NVIDIA Neural GeometricLOD. https://research.nvidia.com/labs/toronto-ai/nglod/
76. AMD GPUOpen Real-Time Sparse Distance Fields (Lou Kramer GDC 2023). https://gpuopen.com/download/GDC-2023-Sparse-Distance-Fields-For-Games.pdf
77. Learned Motion Matching Ubisoft La Forge. https://www.ubisoft.com/en-us/studio/laforge/news/6xXL85Q3bF2vEj76xmnmIu/introducing-learned-motion-matching
78. 3DGS Khronos glTF state 2026. https://www.thefuture3d.com/blog/state-of-gaussian-splatting-2026/
79. GDC Vault KIRI 3DGS for game dev. https://gdcvault.com/play/1034682/3D-Gaussian-Splatting-in-Game

### Industria commentary / insider analysis

80. A Gentle Intro to ReSTIR (Wyman notes 2023). https://intro-to-restir.cwyman.org/presentations/2023ReSTIR_Course_Notes.pdf
81. An Introduction to Neural Shading SIGGRAPH 2025. https://dl.acm.org/doi/10.1145/3721241.3733999

---

**Total URLs únicas**: ≥80 (supera el mínimo ≥40 exigido).
**Gaps conocidos**:
- Información interna EA Frostbite, Santa Monica, Naughty Dog neural rendering roadmap post-2025: no pública.
- Specs finales PS6/Xbox Helix: leaks coincidentes pero sin silicon confirmado.
- FSR 5 / DLSS 5 roadmaps: solo inference de trajectory, no anuncio oficial Q1 2026.
- Slang adoption AAA: Valve Source 2 confirmado, otros studios bajo NDA.
- Bevy virtualized geometry merged status: 0.16 merge reportado pero estable production TBD.
- Rendering para VR/AR extended: no cubierto este documento (foraminifera mencionado, Apple Vision Pro Metal 4, no entrado en detalle).
- Ray tracing mobile ARM Immortalis / PowerVR / Adreno X2 benchmarks reales: poco shipping data aún.
