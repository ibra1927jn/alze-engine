# Hardware y APIs gráficas 2026-2032 — Roadmap condicionante para motores

**Audiencia**: ingenieros de `/root/repos/alze-engine` decidiendo qué APIs y niveles de feature soportar.
**Fecha**: 2026-04-22.
**Método**: roadmaps oficiales, whitepapers y comunicaciones de fabricantes. Se distingue explícitamente lo *anunciado* de lo *proyectado*. Rumor sin fuente se omite.

---

## 0. TL;DR para prisa

- 2026-2028 el trío de APIs vivas para AAA es **D3D12 + Vulkan 1.4+ + Metal 4**. OpenGL/DX11 están muertos para arquitectura nueva (mantener sólo como fallback de compat).
- El hardware baseline realista para *target* 2028 de alze es **RT tier "DXR 1.2 + SER + OMM" y tensor-core o equivalente obligatorio** (Ada+/Blackwell, RDNA 4+/UDNA, Xe2+/Xe3, Apple M3+). Sin eso, el pipeline neural (denoise, upscale, frame-gen) no cierra.
- Path tracing será **capability asumida** (no lujo) hacia 2030 en consola/PC medio-alto; en móvil/handheld queda en 2032+.
- La decisión técnica más cara de revertir es **shader language**: HLSL vs Slang vs "ambos via Slang". Elegir a ciegas cuesta años.
- Work Graphs + mesh shading + cooperative vectors son **tres extensiones del mismo cambio**: el pipeline deja de ser fijo y el CPU deja de orquestar. Quien no se adapte queda atrás en 2028.

---

## 1. NVIDIA — Ada → Blackwell → Rubin → Feynman

### 1.1 Ada Lovelace (RTX 4000, 2022)

Introducida con RTX 4090 el 12-oct-2022. Features que redefinieron expectativas de motor:

- **RT Cores Gen 3**: opacity micromap engine, displaced micro-mesh engine (triangle intersection ~2× Ampere).
- **Tensor Cores Gen 4**: FP8 Transformer Engine (primera vez en GeForce).
- **Shader Execution Reordering (SER)**: reordena threads in-flight para reducir divergencia. Hasta 2× en escenas RT pesadas según whitepaper NV. Expone a través de `NvAPI_D3D12_SetNvShaderExtnSlotSpace` + extensión HLSL (`ReorderThread`) hasta que se estandariza en DXR 1.2.
- **DLSS 3 Frame Generation**: requiere Optical Flow Accelerator Ada-exclusivo.
- **AV1 encode** en NVENC (relevante para cloud streaming y screenshot/clip).

Whitepaper: <https://images.nvidia.com/aem-dam/Solutions/geforce/ada/nvidia-ada-gpu-architecture.pdf>
SER whitepaper: <https://d29g4g2dyqv443.cloudfront.net/sites/default/files/akamai/gameworks/ser-whitepaper.pdf>

### 1.2 Blackwell (RTX 5000, enero 2025)

Anuncio CES 2025; 5090 lanzado 30-ene-2025. Whitepaper oficial: 57 páginas.

- 92 B transistores, 32 GB GDDR7, 1792 GB/s bandwidth (5090), **PCIe 5.0**, **DisplayPort 2.1 UHBR20 (80 Gbps)**.
- **RT Cores Gen 4**: 2× ray-triangle intersect rate vs Ada, optimizados para **Mega Geometry** (BVH para cluster-based Nanite-scale).
- **Tensor Cores Gen 5**: FP4 nativo (2nd-gen Transformer Engine). 3 352 AI TOPS en 5090.
- **AI Management Processor (AMP)**: scheduler on-GPU para workloads de inferencia.
- **Flip Metering hardware**: frame pacing para Multi-Frame Generation movido al display engine.
- **DLSS 4**: transformer model (reemplaza CNN), Multi Frame Generation hasta 4× (3 frames generados/1 renderizado). DLSS 4.5 (nov 2025) sube a 6× dynamic MFG.
- **Neural Shaders**: ejecución de redes pequeñas *dentro* de un shader pass vía Cooperative Vectors.

Whitepaper: <https://images.nvidia.com/aem-dam/Solutions/geforce/blackwell/nvidia-rtx-blackwell-gpu-architecture.pdf>
DLSS 4: <https://www.nvidia.com/en-us/geforce/news/dlss4-multi-frame-generation-ai-innovations/>
DLSS 4.5: <https://www.nvidia.com/en-us/geforce/news/dlss-4-5-dynamic-multi-frame-gen-6x-2nd-gen-transformer-super-res/>
Research paper: <https://research.nvidia.com/labs/adlr/DLSS4/>

### 1.3 Rubin (2026) y Rubin Ultra (2H 2027)

Anunciados en **GTC 2025** y detallados en **GTC 2026** (17-mar-2026). Son arquitectura *data-center-first*, pero marcan el techo del que derivará RTX 60 (nomenclatura gaming tbd).

- **Vera Rubin VR200**: 288 GB HBM4, ~50 PFLOPS FP4 (3.3× B300). Rack Kyber con 144 GPUs vertical.
- **Rubin Ultra 2H 2027**: ~500 B transistores, 384 GB HBM4E, hasta 576 GPUs/rack.
- **NVLink 6**.
- GeForce equivalente (RTX 6000 series) esperada finales 2026–principios 2027 siguiendo cadencia histórica (2 años).

Sources:
- <https://www.tomshardware.com/pc-components/gpus/nvidia-announces-rubin-gpus-in-2026-rubin-ultra-in-2027-feynam-after>
- <https://blogs.nvidia.com/blog/gtc-2026-news/>
- <https://www.datacenterknowledge.com/data-center-chips/gtc-2026-nvidia-unveils-vera-rubin-ai-platform-eyes-1t-by-2027>

### 1.4 Feynman (2028+)

Anunciada como sucesora de Rubin Ultra en GTC 2025/2026. Sin specs; asumir continuación de cadencia 2 años (2028-2029). Proyección razonada: FP2/binary en tensor cores, RT cores que absorben denoise via neural nets en-core, posible memoria on-package LPDDR/HBM unificada.

Source: <https://thenewstack.io/nvidia-unveils-next-gen-rubin-and-feynman-architectures-pushing-ai-power-limits/>

### 1.5 CUDA capability levels y qué importa a motor

Progresión: 8.9 (Ada), 10.x (Blackwell), 11.x (Rubin proyectado). Lo relevante para motor gráfico *no* es CUDA directamente, sino qué se expone en D3D12/Vulkan:

| Feature                     | Ada      | Blackwell | Rubin (proy.) |
|-----------------------------|----------|-----------|---------------|
| SER                         | Yes (NvAPI) | Yes (DXR 1.2) | Yes |
| Opacity Micromaps           | Yes      | Yes       | Yes           |
| Displaced Micro-Mesh        | Yes      | Yes       | Yes           |
| Mega Geometry BVH           | —        | Yes       | Yes           |
| Cooperative Vectors (DX/VK) | parcial  | Yes       | Yes           |
| FP8 tensor                  | Yes      | Yes       | Yes           |
| FP4 tensor                  | —        | Yes       | Yes           |
| Flip Metering HW            | —        | Yes       | Yes           |

---

## 2. AMD — RDNA 3 → RDNA 4 → UDNA/RDNA 5

### 2.1 RDNA 3 (RX 7000, dic 2022)

- Chiplet MCM, AI accelerators 1st gen (WMMA sobre shader ALUs, no cores dedicados).
- RT accelerators 2nd gen.
- DisplayPort 2.1 UHBR13.5 (no UHBR20).
- FSR 3 + Fluid Motion Frames (frame-gen sin HW dedicado).

### 2.2 RDNA 4 (RX 9000, 28-feb-2025 anuncio, 6-mar-2025 retail)

Proceso TSMC N4C. Lanzamiento RX 9070 XT ($599) / RX 9070 ($549). Sólo segmento mainstream; no hay halo card.

- **Ray accelerators 3rd gen**: 2× throughput/CU vs RDNA 3.
- **AI accelerators 2nd gen**: hasta 8× INT8 throughput/AI-accelerator (sparse). WMMA con **FP8 nativo** (condición para FSR 4).
- **FSR 4 (ML-based)**: modelo transformer-based, entrenado en clusters Instinct. FP8 WMMA mandatorio → RX 9000+ exclusivo. FSR 4.1 (Redstone) añade Ray Regeneration.
- 16 GB GDDR6 (no GDDR7 esta gen).

Press release AMD: <https://www.amd.com/en/newsroom/press-releases/2025-2-28-amd-unveils-next-generation-amd-rdna-4-architectu.html>
FSR 4: <https://gpuopen.com/fidelityfx-super-resolution-4/>
Redstone: <https://gpuopen.com/learn/amd-fsr-redstone-developers-neural-rendering/>

### 2.3 UDNA / RDNA 5 (2026-2027)

Anuncio conceptual AMD Financial Analyst Day 2024; convergencia RDNA (gaming) + CDNA (compute) en una misma ISA. Q2 2026 mass production según filtraciones múltiples. N3E TSMC. GDDR7. 32-96 CUs range. Doble perf en RT y AI.

- Primer uso masivo: **PS6 (2027-2028)** según roadmap Sony, y **Project Helix / Next Xbox (2027)** según CFO AMD Lisa Su.
- Branding gaming probablemente retiene "RDNA 5"; datacenter queda "UDNA/CDNA-next". AMD todavía no clarifica definitivamente (a abr-2026).

Sources:
- <https://www.jonpeddie.com/news/amds-next-generation-gpu-architecture-udna-and-rdna-5/>
- <https://www.tomshardware.com/pc-components/gpus/desktop-gpu-roadmap-nvidia-rubin-amd-udna-and-intel-xe3-celestial>
- <https://videocardz.com/newz/amd-radeon-rx-9000-udna-gpus-reportedly-enter-mass-production-in-q2-2026-udna-also-for-playstation-6>

### 2.4 Equivalentes AMD a features NV

| NV feature              | AMD equivalente                       | Estado 2026 |
|-------------------------|---------------------------------------|-------------|
| SER                     | (no equivalente HW público)           | Manejado en compiler/driver |
| Tensor cores            | AI accelerators (WMMA FP8)            | Disponible RDNA 4 |
| DLSS                    | FSR 4 / FSR Redstone 4.1              | Solo RX 9000+ |
| Frame Generation        | FSR 3 FMF (no-HW) / FSR 4 FG          | FSR 4 FG en RDNA 4 |
| Mega Geometry           | (no equivalente público)              | Pendiente UDNA |
| Cooperative Vectors     | soporte via DX Agility SDK driver     | Parcial RDNA 4 |

---

## 3. Intel Arc — Alchemist → Battlemage → Celestial → Druid

### 3.1 Battlemage (Xe2, dic 2024 / ene 2025)

B580 (dic 2024), B570 (ene 2025). Target mid-range $250-$350. XMX matrix engines gen 2, XeSS 2 con Frame Generation + Xe Low Latency.

### 3.2 Celestial (Xe3, 2026)

Estado (may 2025): **pre-silicon validation**. Desktop esperado finales 2026 / inicios 2027. Paridad con Panther Lake (iGPU). Hardware design "locked" — equipo ya en Xe4.

### 3.3 Druid (Xe4, 2027+)

En desarrollo HW. Desktop >2027.

### 3.4 XeSS evolution

- **XeSS 1** (2022): DP4a fallback + XMX en Arc.
- **XeSS 2** (2025): Frame Generation + Low Latency. SDK open-source.
- Target Celestial: XeSS 3 con path-reconstruction + cooperative vectors D3D12.

Sources:
- <https://www.tomshardware.com/pc-components/gpus/intel-arc-xe3-celestial-gpu-enters-pre-validation-stage>
- <https://www.intel.com/content/www/us/en/developer/articles/technical/xess2-whitepaper.html>
- <https://www.intel.com/content/www/us/en/developer/articles/technical/xess-fg-developer-guide.html>

### 3.5 Lugar de Intel en gaming 2030

Proyección: ≤10% discrete market share. Relevante por volumen de **iGPUs en laptops Meteor/Panther/Nova Lake** y en handhelds (MSI Claw). Un motor que sólo va bien en NV se expone a 30-40% del instalado-base invisibilizado.

---

## 4. Apple Silicon — M3 / M4 / M5 y el ecosistema Metal

### 4.1 M3 (oct 2023) — primera Apple GPU con RT HW

Dynamic Caching (alloc de memoria local on-die just-in-time), hardware-accelerated ray tracing, mesh shading. <https://developer.apple.com/videos/play/tech-talks/111375/>

### 4.2 M4 / M4 Pro / M4 Max (may 2024 / oct 2024)

- M4: 10-core GPU, RT 2× vs M3.
- M4 Pro: 16 o 20-core GPU.
- M4 Max: 32 o 40-core GPU.
- M3 Ultra (Mac Studio mar 2025, no M4 Ultra aún): hasta 80-core GPU, 512 GB unified memory.
- Cyberpunk 2077 a 1600p path-traced @ 61 fps en M4 Max con MetalFX upscaling (tweaktown).

### 4.3 Apple Vision Pro y visionOS

- Compositor Services: API Metal dedicada para XR. Foveated color pipeline (texture físico < texture screen-space).
- Render budget 90 Hz × 2 ojos (base), 100 Hz M2-Vision Pro, 120 Hz M5-Vision Pro.
- Obliga a motor a respetar **foveation API** y **low-latency deadline**: si drop frame, visionOS penaliza más agresivamente que macOS.

Refs:
- <https://developer.apple.com/documentation/visionos/understanding-the-visionos-render-pipeline>
- <https://developer.apple.com/videos/play/wwdc2024/10092/>

### 4.4 Metal 4 (WWDC 2025)

- **MetalFX Frame Interpolation** (equivalente DLSS FG / FSR FG).
- **MetalFX Denoising** (permite path tracing real).
- **Path tracing** como feature expuesta en la API.
- Game Porting Toolkit 3 (profiling con métricas específicas).

Refs:
- <https://www.flatpanelshd.com/news.php?subaction=showfull&id=1749809641>
- <https://appleinsider.com/articles/25/06/09/metal-4-game-porting-toolkit-3-boost-frame-rate-ray-tracing-performance>

### 4.5 Implicaciones para alze

- Si apuestas por Metal *nativo*, eliminas MoltenVK (overhead no trivial) y ganas acceso a Frame Interpolation y Denoising directos. Pero duplicas costo de mantenimiento.
- Si te quedas en Vulkan + MoltenVK, pierdes path tracing eficiente y frame interpolation hasta que MoltenVK las mapee (históricamente 12-18 meses de lag).

---

## 5. Consolas generación 10

### 5.1 Nintendo Switch 2 (5-jun-2025) — ya lanzada

- SoC NVIDIA **T239** (custom): ARM Cortex-A78C ×8 + GPU Ampere 12 SM / 1 536 CUDA + **48 tensor cores + 12 RT cores** + 128-bit LPDDR5X.
- Clocks: 561 MHz handheld / 1 007 MHz docked.
- 12 GB LPDDR5X.
- **DLSS** (CNN model) con 3 niveles. "Fat DLSS" sólo a 1080p; "DLSS Light" escala a 4K docked.
- Output: hasta 4K60 docked.
- Posición rendimiento: entre Steam Deck (handheld) y RTX 3050 (docked).

Fuentes:
- <https://blogs.nvidia.com/blog/nintendo-switch-2-leveled-up-with-nvidia-ai-powered-dlss-and-4k-gaming/>
- <https://en.wikipedia.org/wiki/Nintendo_Switch_2>

### 5.2 Xbox Next / "Project Helix" (2027)

- Confirmado por CFO AMD Lisa Su (earnings call feb 2026): SoC AMD semi-custom, launch 2027.
- Microsoft anunció plan de **dual form factor**: living room + handheld, co-engineered silicon.
- Tech revealed mar-2026: DirectX, FSR, **ML Multi Frame Generation**, RT next-gen.
- Alpha a devs 2027.

Sources:
- <https://www.techradar.com/gaming/xbox/microsofts-next-gen-xbox-console-may-land-in-2027-according-to-amd-but-the-console-wars-are-over>
- <https://www.purexbox.com/news/2026/02/amd-mentions-next-gen-xbox-console-says-its-progressing-well-to-release-in-2027>

### 5.3 PlayStation 6 (2027-2028)

- SoC "design complete" según KeplerL2 (ene 2025).
- AMD Zen 5 + 3D V-Cache + UDNA GPU.
- Target: ≈RTX 4080 perf, 3× base PS5, 4K120 o 8K60, RT mejorado.
- Ventana original 2027; reportes abr 2026 sugieren Sony considera push a 2028-2029 por escasez memoria HBM/GDDR7.

Sources:
- <https://www.tweaktown.com/news/102669/playstation-6-soc-is-design-complete-says-leaker-amd-zen-5-with-x3d-cache-next-gen-udna-gpu/index.html>
- <https://www.club386.com/playstation-6-predicted-to-launch-in-2027-with-an-amd-udna-gpu/>

### 5.4 PS5 Pro PSSR + Project Amethyst

- PSSR (nov 2024, PS5 Pro launch) = upscaler AI Sony, 50+ juegos.
- **Project Amethyst** (Sony + AMD, 2024-): co-desarrollo de redes neural. Resultado = base común PSSR + FSR 4.
- **PSSR 2** (2026): "Multi-Frame Super Resolution 2" reduce memoria y GPU time.

Sources:
- <https://blog.playstation.com/2026/02/27/upgraded-pssr-upscaler-is-coming-to-ps5-pro/>
- <https://www.tweaktown.com/news/110559/amds-fsr-4-1-for-rdna-4-is-based-on-the-same-tech-as-sonys-new-pssr-for-the-ps5-pro/index.html>

### 5.5 Proyección gen-10 (2027+)

| Feature                    | PS5 Pro | Xbox Next (2027) | PS6 (2027-2028) | Switch 2 |
|----------------------------|---------|------------------|-----------------|----------|
| Path tracing capable       | Limited | Probable         | Sí (target)     | No       |
| AI upscale HW-native       | PSSR    | ML Upscale       | PSSR 2 / FSR 5  | DLSS     |
| Frame generation           | —       | ML MFG           | ML MFG          | —        |
| NVMe PCIe 5                | —       | Probable         | Probable        | UFS      |
| Unified memory             | Sí      | Sí               | Sí              | Sí       |
| RT perf vs base PS5        | 4×      | ≥5×              | 6-8×            | Limited  |

---

## 6. Handhelds

### 6.1 Panorama abril 2026

| Device                   | SoC                            | Arch             | Fecha     |
|--------------------------|--------------------------------|------------------|-----------|
| Steam Deck (2022)        | AMD Van Gogh custom            | Zen 2 + RDNA 2   | feb 2022  |
| Steam Deck OLED (2023)   | AMD Van Gogh 6 nm              | Zen 2 + RDNA 2   | nov 2023  |
| ROG Ally X (2024)        | AMD Z1 Extreme                 | Zen 4 + RDNA 3   | 2024      |
| Legion Go (2023)         | AMD Z1 Extreme                 | Zen 4 + RDNA 3   | oct 2023  |
| MSI Claw (2024)          | Intel Meteor Lake (Core Ultra) | Xe-LPG           | mar 2024  |
| Legion Go S (2025)       | Ryzen Z2 Go                    | Zen 4 + RDNA 3.5 | 2025      |
| Legion Go 2 (2025)       | Ryzen AI Z2 Extreme            | Zen 5 + RDNA 3.5 | 2025      |
| ROG Xbox Ally X (2025)   | Ryzen AI Z2 Extreme            | Zen 5 + RDNA 3.5 + NPU 50 TOPS | 2025 |

### 6.2 Steam Deck 2 (¿2026-2027?)

- Valve reiteró (abr 2025 y abr 2026): *no* lanzan hasta que haya salto arquitectónico real sin comprometer autonomía. "Drastically better performance with the same battery life is not enough".
- Candidato más mencionado: APU AMD "Magnus" Zen 6.

Sources:
- <https://www.tomshardware.com/video-games/handheld-gaming/valve-is-waiting-for-major-architectural-improvements-on-future-silicon-before-creating-the-steam-deck-2-drastically-better-performance-with-the-same-battery-life-is-not-enough>
- <https://www.pcgamer.com/hardware/valve-says-a-next-gen-steam-deck-2-still-isnt-possible-two-years-after-it-last-said-exactly-the-same-thing/>

### 6.3 Envelope térmico / eléctrico

- Steam Deck: 4-15 W GPU (APU total 25 W max).
- ROG Ally X / Legion Go 2: 15-30 W configurables.
- Xbox Ally X handheld mode: hasta 30 W; dock 45 W.
- Steam Frame (Valve, 2026): standalone XR, Snapdragon 8 Gen 3, ~5-10 W GPU.

### 6.4 Implicaciones para engine

- Motor 2026-2030 debe asumir **escala 3-4 órdenes de magnitud** entre handheld low-power y desktop top: ~2 TFLOPS → ~100 TFLOPS.
- Quality presets no bastan. Necesitas:
  - Variable rate shading (VRS).
  - Render resolution scaling + upscalers cross-vendor (DLSS + FSR + XeSS + MetalFX).
  - GPU-driven culling agresivo.
  - Streaming de assets LOD-aware.

---

## 7. VR / AR hardware

### 7.1 Comparativa 2026

| Device              | SoC                | Display / ojo       | Eye tracking | Foveated | Launch   |
|---------------------|--------------------|---------------------|--------------|----------|----------|
| Meta Quest 3 (2023) | Snapdragon XR2 Gen 2 | 2064×2208 LCD       | No           | Fixed    | oct 2023 |
| Quest 3S (2024)     | XR2 Gen 2          | 1832×1920 LCD       | No           | Fixed    | oct 2024 |
| Apple Vision Pro (2024) | M2 + R1         | 3660×3200 micro-OLED | Yes          | Dynamic ETFR | feb 2024 |
| Apple Vision Pro 2 (2025+) | M5 + R1     | mismo panel         | Yes          | Dynamic ETFR | rumor    |
| PSVR2 (2023)        | PS5 host           | 2000×2040 OLED      | Yes          | Dynamic ETFR | feb 2023 |
| Valve Steam Frame / Deckard (2026) | Snapdragon 8 Gen 3 | 2160×2160 LCD/µOLED | Yes | Dynamic | 2026 (rumor) |
| Meta Quest 4 / Pismo (2027-2028) | XR3+ | ~3200×3200 micro-OLED | Yes | Dynamic ETFR | 2027+ |

### 7.2 Render budget

- 90-120 Hz × 2 ojos = **180-240 fps efectivos** sostenidos.
- Latencia motion-to-photon < 20 ms.
- Low persistence (shutter del display) → overshoot en pipeline.
- **Foveated rendering** obligatorio a partir de 1440p/ojo: fixed (Quest 2/3), dynamic/ETFR (Quest Pro, Vision Pro, PSVR2, Deckard, Quest 4).

### 7.3 APIs

- OpenXR = estándar cross-vendor para input/runtime.
- Rendering: Vulkan + VK_KHR_fragment_shading_rate (VRS/foveation) o Metal CompositorServices + foveation config en LayerRenderer.
- WebXR + WebGPU: empieza a ser viable en Safari 26.2 + Vision Pro.

Sources:
- <https://developers.meta.com/horizon/blog/save-gpu-with-eye-tracked-foveated-rendering/>
- <https://developer.apple.com/documentation/visionos/understanding-the-visionos-render-pipeline>

---

## 8. Vulkan — evolución 1.3 → 1.4 → Roadmap 2026

### 8.1 Vulkan 1.3 (ene 2022)

Core features que reshapeiran engines: **dynamic rendering**, **synchronization2**, **extended dynamic state**, **shader objects** (1.3.246 con VK_EXT_shader_object como punto de inflexión: permite evitar VkPipeline objects monolíticos).

### 8.2 Vulkan 1.4 (dic 2024)

Consolidación: push descriptors, dynamic rendering local reads, scalar block layouts, maintenance5/6 y streaming transfers pasan a core. Límite 8 render targets @ 8K garantizado.

Sources:
- <https://www.khronos.org/news/press/khronos-streamlines-development-and-deployment-of-gpu-accelerated-applications-with-vulkan-1.4>

### 8.3 Roadmap 2026

- **VK_EXT_descriptor_heap** (2025-2026): reemplazo completo del sistema de descriptor sets. Acceso directo a memoria de descriptores. Preserva compat con descriptor sets legacy. Objetivo: alinear con bindless de D3D12.
- Variable Rate Shading y Host Image Copies se vuelven requisitos del profile Roadmap 2026.
- <https://www.phoronix.com/news/Vulkan-Roadmap-2026>
- <https://www.khronos.org/news/archives/vulkan-introduces-roadmap-2026-and-new-descriptor-heap-extension>

### 8.4 Extensions críticas para alze

| Extension                                  | Propósito                         | Status 2026 |
|--------------------------------------------|-----------------------------------|-------------|
| VK_KHR_ray_tracing_pipeline                | RT pipeline completo              | Core-like   |
| VK_KHR_ray_query                           | RT desde cualquier shader         | Core-like   |
| VK_EXT_mesh_shader                         | Mesh + task shaders cross-vendor  | Disponible 2022+ |
| VK_KHR_cooperative_matrix                  | Tensor-core matmul en SPIR-V      | Core en Roadmap 2026 |
| VK_NV_cooperative_matrix2                  | Variante NVIDIA con + flex        | Vendor-specific |
| VK_NV_cooperative_vector                   | Inferencia neural pequeña en shader | Vendor-specific (camino a KHR) |
| VK_EXT_device_generated_commands           | Command generation on-GPU         | Estable     |
| VK_KHR_fragment_shading_rate               | VRS / foveated                    | Estable     |
| VK_EXT_descriptor_buffer (→ descriptor_heap) | Bindless moderno                | Transición  |

Sources:
- <https://docs.vulkan.org/features/latest/features/proposals/VK_KHR_cooperative_matrix.html>
- <https://docs.vulkan.org/features/latest/features/proposals/VK_NV_cooperative_matrix2.html>
- <https://docs.vulkan.org/features/latest/features/proposals/VK_NV_cooperative_vector.html>
- <https://developer.nvidia.com/blog/machine-learning-acceleration-vulkan-cooperative-matrices/>
- <https://www.khronos.org/blog/mesh-shading-for-vulkan>

### 8.5 Vulkan Profiles

El "Vulkan Roadmap 2024 / 2026 Milestone" = perfil objetivo de features mandatorias para gaming high-end. Un engine serio declara soporte contra un profile, no feature-por-feature.

---

## 9. DirectX 12 — Agility SDK, Shader Models, Work Graphs, DXR 1.2

### 9.1 Agility SDK

Cadencia trimestral aprox. desde 2021. Permite actualizar runtime D3D12 sin esperar Windows 11 release. Muerte efectiva de DX11 para AAA nuevos desde 2023.

Releases clave:
- 1.613.0 (mar 2024): **Work Graphs 1.0**, **Shader Model 6.8**, **GPU Upload Heaps**. <https://devblogs.microsoft.com/directx/agility-sdk-1-613-0/>
- Preview 2024-Q4: **Mesh Nodes en Work Graphs**. <https://devblogs.microsoft.com/directx/d3d12-mesh-nodes-in-work-graphs/>
- GDC 2025 (mar 2025): **DXR 1.2**, **Cooperative Vectors preview**, **SM 6.9 preview**, **PIX**. <https://devblogs.microsoft.com/directx/announcing-directx-raytracing-1-2-pip-neural-rendering-and-more-at-gdc-2025/>

### 9.2 Shader Model

- SM 6.6 (2021): dynamic resource indexing (bindless oficial), 64-bit atomics, pack/unpack.
- SM 6.7 (2022): advanced texture ops.
- SM 6.8 (2024): **nodes** (Work Graphs), start vertex/instance location, wave size range.
- SM 6.9 (preview 2025 → estable 2026): **cooperative vectors**, ampliaciones neural.

### 9.3 Work Graphs

Paradigma: GPU planifica su propio trabajo. Reemplaza cadenas dispatch→indirect→dispatch con topología de nodos.
- **Compute nodes** (SM 6.8): disponible mar-2024.
- **Mesh nodes** (SM 6.8 → 6.9): dispatch de mesh shader pipeline desde un nodo. Amplification shader on steroids.

Refs:
- <https://devblogs.microsoft.com/directx/d3d12-work-graphs/>
- <https://microsoft.github.io/DirectX-Specs/d3d/WorkGraphs.html>
- <https://developer.nvidia.com/blog/advancing-gpu-driven-rendering-with-work-graphs-in-direct3d-12/>
- <https://gpuopen.com/learn/work_graphs_mesh_nodes/work_graphs_mesh_nodes-intro/>

### 9.4 DXR tiers

| Tier  | Features                                  | HW mínimo |
|-------|-------------------------------------------|-----------|
| 1.0   | RT pipeline básico                        | Turing, RDNA 2, Arc |
| 1.1   | ExecuteIndirect RT, Inline RT (ray query) | Turing+, RDNA 2+ |
| 1.2   | **Opacity Micromaps + SER estandarizado** | Ada+, RDNA 4+ para OMM full; SER Ada+ |

DXR 1.2 prometé **hasta 2.3× rendimiento** en path-traced (OMM) y **hasta 2× SER**. <https://devblogs.microsoft.com/directx/announcing-directx-raytracing-1-2-pix-neural-rendering-and-more-at-gdc-2025/>

### 9.5 DirectStorage

- 1.0 (mar 2022): API async file → GPU.
- 1.1 (nov 2022): GPU decompression via **GDeflate**.
- 1.2+ (2023+): metadata handling, sampler feedback streaming integración.
- 2.0 (roadmap): integración con neural texture compression esperada.

Refs:
- <https://developer.nvidia.com/blog/accelerating-load-times-for-directx-games-and-apps-with-gdeflate-for-directstorage/>
- <https://github.com/microsoft/DirectStorage/blob/main/GDeflate/README.md>
- <https://developer.nvidia.com/rtx-io>
- <https://github.com/GameTechDev/SamplerFeedbackStreaming>

### 9.6 Cooperative Vectors (DX)

Anuncio GDC 2025 conjunto NVIDIA + Microsoft: acceso a tensor cores desde shader HLSL via intrinsics. Base para **neural texture compression**, **neural BRDFs**, **neural radiance cache**. Disponible en preview SM 6.9.

Refs:
- <https://devblogs.microsoft.com/directx/enabling-neural-rendering-in-directx-cooperative-vector-support-coming-soon/>
- <https://nvidianews.nvidia.com/news/nvidia-and-microsoft-open-next-era-of-gaming-with-groundbreaking-neural-shading-technology>
- <https://developer.nvidia.com/blog/nvidia-rtx-advances-with-neural-rendering-and-digital-human-technologies-at-gdc-2025/>

### 9.7 DML (DirectML)

Camino separado para inferencia ML general en GPU. Relevante para contenidos assets (super-res texturas, animación IK ML). No reemplaza cooperative vectors — son capas distintas.

---

## 10. Metal — 1 → 2 → 3 → 4

### 10.1 Metal 3 (2022)

MetalFX Upscaling (temporal/spatial), async compute, fast resource loading.

### 10.2 Metal 4 (WWDC 2025, macOS Tahoe 26)

- **MetalFX Frame Interpolation** (FG equivalent).
- **MetalFX Denoising** (fabrica path tracing practical).
- Mesh shaders maduros.
- Tiled deferred rendering sigue siendo ventaja estructural Apple.
- Game Porting Toolkit 3: profiler metrics.

Refs:
- <https://www.flatpanelshd.com/news.php?subaction=showfull&id=1749809641>
- <https://wccftech.com/apple-metal-4-api-adds-interpolation-to-boost-gaming-performance/>

### 10.3 Apple-only ventajas y fricciones

- Ventaja: unified memory (sin copias CPU↔GPU), TBDR (tiled-based deferred) ahorra bandwidth.
- Fricción: ecosistema cerrado, no portable. Obliga a abstraction layer si compartes renderer con otros stacks.
- Apple no publica roadmap multi-year. Inferir desde WWDC + silicon release cadence (anual).

---

## 11. WebGPU — estado abr 2026

- Ships por defecto en **Chrome** (abr 2023), **Edge**, **Safari 26.0** (macOS Tahoe 26, iOS 26, iPadOS 26, visionOS 26), **Firefox 141+** (Windows desde jul 2025, macOS Apple Silicon desde Firefox 145).
- WGSL (lenguaje shader propio). SPIR-V no se acepta directamente en runtime web (issue pendiente).
- wgpu (Rust): biblioteca cross-platform sobre Vulkan/DX12/Metal/WebGPU. Implementación de Servo/Firefox/Deno. Base de Bevy.

### 11.1 Viabilidad gaming AAA en web

- Para indie / 2D / 3D moderado: **sí**, 2026.
- Para AAA: no hoy. Limitaciones:
  - No RT hardware path (aún).
  - No mesh shaders.
  - No bindless completo.
  - Storage/streaming limitado (fetch + decoder en JS/WASM).
- Para cloud-gaming **cliente delivery**: WebGPU no sustituye WebRTC. Es complemento (UI, client-side effects).

Refs:
- <https://github.com/gpuweb/gpuweb/wiki/Implementation-Status>
- <https://web.dev/blog/webgpu-supported-major-browsers>

---

## 12. Shader languages — HLSL, Slang, WGSL, GLSL, MSL

### 12.1 Estado

- **HLSL**: industria estándar DX + Vulkan (vía dxc → SPIR-V). Microsoft evoluciona vía SM 6.x.
- **Slang**: superset HLSL, open-source. Transferido de NVIDIA a **Khronos Slang Initiative en nov 2024**. Compila a SPIR-V, HLSL, GLSL, MSL, **WGSL**. Valve integró en Source 2 (CS2, Dota 2 shipping).
- **WGSL**: único lenguaje aceptado por WebGPU runtime. Semántica distinta a HLSL en algunos puntos (no templates, tipos más restrictivos).
- **GLSL**: legacy; OpenGL sigue vivo en mobile/Linux embedded pero no es futuro. Conversión GLSL→SPIR-V con glslc OK.
- **MSL**: Metal-only. Generable desde Slang o desde HLSL via SPIRV-Cross.

### 12.2 Proyección 2026-2030

Escenario probable: **Slang gana tracción como frontend unificado**. HLSL no muere (Microsoft lo mantendrá), pero el workflow "escribo HLSL, compilo a todo" pierde vs "escribo Slang, compilo a 5 targets". Adopción inicial: CS2/Dota 2 (Valve), NVIDIA Falcor, BGFX. Kronos invertirá en profile de SPIR-V → Slang → todos targets.

Refs:
- <https://www.khronos.org/news/press/khronos-group-launches-slang-initiative-hosting-open-source-compiler-contributed-by-nvidia>
- <http://shader-slang.org/>
- <https://shader-slang.org/blog/2024/11/20/theres-a-lot-going-on-with-slang/>

### 12.3 Implicación para alze

Si alze-engine empieza con shaders HLSL via dxc, el coste de saltar a Slang más tarde es bajo (superset). Pero si empieza con GLSL, el salto es mucho más alto. **Recomendación**: HLSL ahora con pipeline vía dxc; prototipar Slang 2027 para targetar WebGPU/Metal sin reescribir.

---

## 13. AI-accelerator integration en shaders

### 13.1 El patrón emergente

2023-2025 se consolida un patrón cross-vendor: cada GPU expone matmul acelerado (tensor cores / XMX / WMMA) en la API gráfica, no sólo en compute APIs propietarias.

- **NVIDIA**: tensor cores → DLSS (proprietario hasta 2022), RTX Tensor via VK_KHR_cooperative_matrix desde 2023, Cooperative Vectors DX SM 6.9 desde 2025.
- **AMD**: AI Accelerators RDNA 3 (WMMA via shader ALU), AI Accelerators RDNA 4 (WMMA FP8 dedicado). RADV merge VK_KHR_cooperative_matrix para RDNA 3.
- **Intel**: XMX in Alchemist+ → XeSS. Expuestos vía DP4a fallback y XMX en Vulkan cooperative_matrix.
- **Apple**: neural engine separado, pero M3+ tensor ops dentro del GPU. MetalFX usa shader cores, no ANE.

### 13.2 Usos actuales / futuros en motor

| Uso                        | Madurez 2026 | Hardware requerido |
|---------------------------|--------------|--------------------|
| Upscaling (DLSS/FSR/XeSS) | Estable     | Cross-vendor       |
| Frame generation          | Estable     | Gen reciente (Ada+, RDNA 4+, Arc)|
| Ray denoise (RR, MetalFX) | Estable     | RT-capable + tensor |
| Neural texture compression | Preview    | Cooperative Vectors |
| Neural radiance cache     | Research    | Idem               |
| Neural BRDF               | Research    | Idem               |
| Skinning / animation blend| Experimental | NPU / tensor      |

### 13.3 Consecuencia arquitectónica

El "render pass" deja de ser vertex→raster→fragment y se vuelve híbrido: fases ML pequeñas entremezcladas con passes gráficos tradicionales. El **scheduler on-GPU** (Work Graphs + SER) es necesario para que esa mezcla no mate el throughput.

---

## 14. Neural engine integration (NPU separada)

### 14.1 Actores

- **Apple Neural Engine (ANE)**: 16 cores, 38 TOPS M4 Max. Accesible vía CoreML, **no directamente por Metal** en juegos (latency cross-domain).
- **Qualcomm Hexagon NPU**: 45 TOPS Snapdragon X Elite, 80 TOPS Snapdragon X2 Elite. Usable vía QNN, DirectML, WinML. INT4 nativo.
- **Intel NPU**: Meteor Lake (11 TOPS), Lunar Lake (48 TOPS), Panther Lake (~60 TOPS). Accesible vía OpenVINO, DirectML, WinML.
- **AMD XDNA NPU**: Ryzen AI 300 series (50 TOPS). DirectML, ONNX RT.

### 14.2 Uso en gaming 2026-2030

- **Upscaling cross-GPU NPU**: posible si el NPU puede hacer frame in < 5 ms. En teoría Snapdragon X Elite/X2 puede, pero latencia de copia CPU-NPU-GPU-display mata la ventaja vs tensor cores on-GPU.
- **Windowed workloads no críticos de latencia**: noise suppression audio/voice, voice recognition, anti-cheat comportamental, asset gen offline, NPC dialogue LLM.
- **Camino realista**: NPU hace *non-frame-critical* AI, tensor cores hacen *per-frame* AI.

Refs:
- <https://www.qualcomm.com/content/dam/qcomm-martech/dm-assets/documents/Unlocking-on-device-generative-AI-with-an-NPU-and-heterogeneous-computing.pdf>
- <https://www.anandtech.com/show/21445/qualcomm-snapdragon-x-architecture-deep-dive>

---

## 15. Cloud + remote rendering

### 15.1 Estado 2026

- **GeForce Now**: RTX 4080 / 5080 class tiers, 4K120, <30 ms latency advertised (<80 ms tolerado).
- **Xbox Cloud Gaming (xCloud)**: Series X class en Azure.
- **Luna (Amazon)**: tiers Premium/Ultimate.
- **Stadia**: closed Jan 2023 — lección: publisher relationships y UI/store-integration matter tanto como tech.

### 15.2 Latencia

- Objetivo: <50 ms total motion-to-photon en buen caso (<80 ms aceptable).
- Real observado: 52-85 ms dependiendo servicio y ubicación.
- Wi-Fi 6E + 5G SA o fibra → <60 ms viable.
- 4K120 + RT + MFG: el server renderiza, client recibe, pero frame-gen se hace **server-side** (hay rumores de FG client-side en GFN para compensar, pero nada confirmado).

### 15.3 Implicación engine

- Motor debe exponer **command recording determinista** para que el server pueda rehacer frames con pequeña variación de input latency.
- Encode path: AV1 > H.265 > H.264 para cloud.

Refs:
- <https://www.nvidia.com/en-us/geforce-now/system-reqs/>

---

## 16. Storage + streaming

### 16.1 NVMe PCIe 5 consumer

- CES 2024: primeros SSD PCIe 5 consumer (Crucial T700, Seagate FireCuda 540): ~12 GB/s sequential read.
- 2025-2026: curva precio/GB adoptable en laptops gaming y consola.
- Throughput I/O sube 5-10× vs PS5/Series X baseline (≈5-7 GB/s).

### 16.2 DirectStorage + GDeflate + RTX IO

- **GDeflate**: formato de compresión diseñado para 32-way parallel decompression on-GPU. Mantiene ratio DEFLATE. Open spec contribuido por NVIDIA.
- **DirectStorage**: API MS. 1.1 añade GPU decompression. Usado en Ratchet & Clank (PC), Forspoken, Portal Prelude RTX.
- **RTX IO**: implementación NVIDIA de DirectStorage + GDeflate.
- **Sampler Feedback Streaming (SFS)**: Intel demo 100 GB texture en 100 MB VRAM via feedback-driven tile loading. Usado por UE5 Virtual Textures.

### 16.3 Neural texture compression

- Block Compression (BC1-BC7) tiene ~20 años. BC7 no escala para 8K texture atlases.
- NVIDIA Neural Texture Compression SDK: random-access decompression en-shader via cooperative vectors.
- Proyección: 2027-2028 neural texture compression comienza a aparecer en shipping titles. 2-4× ratio vs BC7 en calidad equivalente.

Refs:
- <https://developer.nvidia.com/blog/nvidia-rtx-neural-rendering-introduces-next-era-of-ai-powered-graphics-innovation/>

---

## 17. Tabla resumen 2026 / 2028 / 2030 / 2032

Leyenda: "Asumido" = feature que el motor puede asumir presente en la mayoría de instalado-base target. "Opcional" = implementar como path enriquecido. "Research" = no en shipping.

| Eje                     | 2026                        | 2028                             | 2030                              | 2032                                 |
|-------------------------|-----------------------------|----------------------------------|-----------------------------------|--------------------------------------|
| **GPU baseline AAA PC** | Ampere/RDNA 2 mínimo; Ada/RDNA 3 típico | Ada/RDNA 3 mínimo; Blackwell/RDNA 4/UDNA típico | Blackwell/UDNA mínimo; Rubin-gen/UDNA+1 típico | Rubin-gen/UDNA+1 mínimo; Feynman-gen típico |
| **RT básico (DXR 1.0)** | Asumido PC/console          | Asumido                          | Asumido                           | Asumido                              |
| **RT DXR 1.2 (SER+OMM)**| Opcional (Ada+, RDNA 4+)    | Asumido PC high-end              | Asumido PC/console                | Asumido                              |
| **Path tracing**        | Opcional premium            | Opcional PC high + consola next  | Asumido console gen-10, opcional mid-range PC | Asumido mainstream PC/console |
| **Neural upscale (DLSS/FSR/XeSS/PSSR/MetalFX)** | Asumido PC/console | Asumido (cross-vendor) | Asumido incluido handheld | Asumido universal |
| **Frame generation**    | Opcional high-end           | Asumido PC con RT-capable        | Asumido PC/console                | Asumido                              |
| **Mesh shaders**        | Opcional                    | Asumido PC; console confirmed    | Asumido                           | Asumido                              |
| **Work Graphs (compute nodes)** | Opcional preview        | Asumido PC AAA                  | Asumido PC/console                | Asumido                              |
| **Work Graphs (mesh nodes)** | Research                | Opcional                        | Asumido high-end                   | Asumido                              |
| **Cooperative Vectors / Matrix** | Preview / vendor-spec | Cross-vendor standard            | Asumido                           | Asumido                              |
| **Neural texture compression** | Research               | Early shipping                   | Asumido                           | Universal                            |
| **VR/AR path (OpenXR + foveated)** | Opcional fixed foveated | Dynamic ETFR cross-vendor       | AR consumer emergente             | AR consumer maduro                   |
| **Handheld low-power**  | RDNA 2-3.5 + Ampere         | RDNA 4/UDNA + Ada-gen            | UDNA-next                         | Integra RT + neural nativo           |

### Notas a la tabla

- "Console gen-10" (Xbox Next 2027, PS6 2027-2028) redefine "asumido" hacia 2028 porque su ciclo 7-8 años implica que los motores multiplataforma apuntan a su capability floor.
- Handheld retrasa la asunción de path tracing hasta ≥2030 (thermal envelope).
- Mobile (Android/iOS phone) queda fuera del cuadro; su capability gap vs consola es >1 gen permanente.

---

## 18. Gaps y zonas de incertidumbre

- **UDNA nomenclatura final**: AMD no ha clarificado si "RDNA 5" y "UDNA" son la misma arquitectura con branding dual.
- **Ventana PS6**: 2027 vs 2028-2029 (memoria HBM/GDDR7 supply).
- **Feynman specs**: nada público más allá del nombre.
- **Metal 5**: Apple no publica roadmap multi-year.
- **SER en AMD/Intel**: sin equivalente HW público. Si DXR 1.2 SER se generaliza vía compiler-only fallback, performance gap vs NVIDIA se mantiene.
- **Cooperative Vectors en Vulkan KHR**: propuesta NV extension, timing de KHR standardization no confirmado.
- **WebGPU + RT**: extension en discusión, no estable.

---

## 19. Para alze-engine 2026: las 3 decisiones API más importantes

Asumiendo que alze es un motor *nuevo* con ambiciones AAA-adjacent y target 2027-2029 launch:

### Decisión #1 — Backend gráfico primario

**Recomendación**: **D3D12 + Vulkan 1.4**, uno como primary (el que más uses en dev), el otro via abstraction layer. Metal 4 **nativo** (no MoltenVK) si Apple Silicon es target serio (>15% previsto).

- Por qué D3D12 y Vulkan: son los únicos que exponen Work Graphs + DXR 1.2 + Cooperative Vectors / Matrix con vendor support real en 2026.
- MoltenVK no sirve para path tracing en Metal 4 sin 12-18 meses de lag.
- OpenGL / DX11 sólo si necesitas Windows 7/8 legacy (raro para alze).

**Cuándo revisarla**: Q4 2027, cuando:
- UDNA/PS6/Xbox Next hayan aterrizado y se sepa si algún vendor empuja una nueva API nivel-bajo (improbable).
- WebGPU RT/mesh extensions cristalicen.
- Slang maduro → revisar si frontend unificado cambia el cálculo cost/benefit.

### Decisión #2 — Shader language

**Recomendación**: **HLSL via dxc** como lenguaje autor. SPIR-V como IR intermedio vía dxc→SPIR-V para Vulkan. MSL via SPIRV-Cross para Metal. Reservar **Slang migration** como opción 2027-2028.

- HLSL es la ruta con más tooling, más ejemplos y más hires existentes.
- Slang es *superset* de HLSL — migration no es destructiva.
- Evitar GLSL: su compiler stack es menos robusto en edge cases y el skill pool se reduce.

**Cuándo revisarla**: cuando alze necesite shipping a WebGPU o quiera compartir un 80%+ del codebase de shaders entre DX/VK/Metal/WebGPU sin 4 ramas divergentes. Probablemente **2H 2027** (Khronos Slang Initiative maduro) o cuando un engine de referencia (UE, Unity) adopte Slang.

### Decisión #3 — Ingestion / pipeline neural (upscale + frame-gen + neural shaders)

**Recomendación**: soportar **DLSS, FSR, XeSS, MetalFX** vía un **abstraction layer propio** desde el día 1. No casarse con un solo vendor. Hoy esto se hace vía SDK por vendor (DLSS SDK, FSR SDK, XeSS SDK, MetalFX). Cooperative Vectors / Matrix se deja como path **opcional** hasta 2027.

- Por qué cuatro: cada plataforma (NV, AMD, Intel, Apple) tiene el suyo nativo con +30-40% calidad vs fallbacks genéricos.
- Project Amethyst (Sony+AMD) y PSSR alinean consola AMD con FSR, así que un engine con FSR ready va directo a PS5 Pro/PS6 con mínima fricción.
- Cooperative Vectors es el camino correcto pero la instalada-base 2026 es pequeña. Implementarlo en 2027-2028 cuando haya Rubin-gen deployed.

**Cuándo revisarla**: Q2 2027 — con DLSS 5, FSR 5, XeSS 3, MetalFX gen 2 todos shipping, y con Cooperative Vectors cross-vendor estable. Decisión: ¿retiras tu abstraction layer y vas directo a CoopVec como único backend neural? Mi apuesta: no todavía.

---

## 20. Fuentes (≥40 primarias o autoritativas)

### NVIDIA
1. <https://images.nvidia.com/aem-dam/Solutions/geforce/ada/nvidia-ada-gpu-architecture.pdf> — Ada whitepaper
2. <https://images.nvidia.com/aem-dam/Solutions/geforce/blackwell/nvidia-rtx-blackwell-gpu-architecture.pdf> — Blackwell whitepaper
3. <https://d29g4g2dyqv443.cloudfront.net/sites/default/files/akamai/gameworks/ser-whitepaper.pdf> — SER whitepaper
4. <https://developer.nvidia.com/blog/improve-shader-performance-and-in-game-frame-rates-with-shader-execution-reordering/> — SER blog
5. <https://www.nvidia.com/en-us/geforce/news/rtx-50-series-graphics-cards-gpu-laptop-announcements/> — RTX 50 launch
6. <https://www.nvidia.com/en-us/geforce/news/dlss4-multi-frame-generation-ai-innovations/> — DLSS 4
7. <https://www.nvidia.com/en-us/geforce/news/dlss-4-5-dynamic-multi-frame-gen-6x-2nd-gen-transformer-super-res/> — DLSS 4.5
8. <https://research.nvidia.com/labs/adlr/DLSS4/> — DLSS 4 research
9. <https://developer.nvidia.com/blog/announcing-the-latest-nvidia-gaming-ai-and-neural-rendering-technologies/> — RTX Kit / Mega Geometry
10. <https://developer.nvidia.com/blog/nvidia-rtx-advances-with-neural-rendering-and-digital-human-technologies-at-gdc-2025/> — Neural rendering GDC 2025
11. <https://developer.nvidia.com/blog/machine-learning-acceleration-vulkan-cooperative-matrices/> — VK cooperative matrix
12. <https://developer.nvidia.com/blog/advancing-gpu-driven-rendering-with-work-graphs-in-direct3d-12/> — Work Graphs
13. <https://developer.nvidia.com/blog/accelerating-load-times-for-directx-games-and-apps-with-gdeflate-for-directstorage/> — GDeflate
14. <https://developer.nvidia.com/rtx-io> — RTX IO
15. <https://blogs.nvidia.com/blog/gtc-2026-news/> — GTC 2026 keynote
16. <https://blogs.nvidia.com/blog/nintendo-switch-2-leveled-up-with-nvidia-ai-powered-dlss-and-4k-gaming/> — Switch 2
17. <https://nvidianews.nvidia.com/news/nvidia-and-microsoft-open-next-era-of-gaming-with-groundbreaking-neural-shading-technology> — Neural shading NV+MS

### AMD
18. <https://www.amd.com/en/newsroom/press-releases/2025-2-28-amd-unveils-next-generation-amd-rdna-4-architectu.html> — RDNA 4 / RX 9000 launch
19. <https://d1io3yog0oux5.cloudfront.net/_c2f0f19dde702988fbf14ececfea2041/amd/news/2025-02-28_AMD_Unveils_Next_Generation_AMD_RDNA_4_1238.pdf> — RDNA 4 press PDF
20. <https://gpuopen.com/amd-fsr-upscaling/> — FSR upscaling family
21. <https://gpuopen.com/fidelityfx-super-resolution-4/> — FSR 4
22. <https://gpuopen.com/learn/amd-fsr4-gpuopen-release/> — FSR 4 release notes
23. <https://gpuopen.com/learn/amd-fsr-redstone-developers-neural-rendering/> — FSR Redstone
24. <https://gpuopen.com/learn/work_graphs_mesh_nodes/work_graphs_mesh_nodes-intro/> — Work Graphs mesh nodes AMD

### Intel
25. <https://www.intel.com/content/www/us/en/developer/articles/technical/xess2-whitepaper.html> — XeSS 2 whitepaper
26. <https://www.intel.com/content/www/us/en/developer/articles/technical/xess-fg-developer-guide.html> — XeSS-FG dev guide
27. <https://www.intel.com/content/www/us/en/products/docs/discrete-gpus/arc/software/gaming-technologies.html> — Arc gaming tech
28. <https://www.intel.com/content/www/us/en/developer/articles/news/directstorage-on-intel-gpus.html> — DirectStorage Intel

### Microsoft / DirectX
29. <https://devblogs.microsoft.com/directx/agility-sdk-1-613-0/> — Agility SDK 1.613
30. <https://devblogs.microsoft.com/directx/d3d12-work-graphs/> — Work Graphs
31. <https://microsoft.github.io/DirectX-Specs/d3d/WorkGraphs.html> — Work Graphs spec
32. <https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html> — DXR spec
33. <https://devblogs.microsoft.com/directx/ser/> — SER DX
34. <https://devblogs.microsoft.com/directx/omm/> — Opacity Micromaps
35. <https://devblogs.microsoft.com/directx/d3d12-mesh-nodes-in-work-graphs/> — Mesh nodes preview
36. <https://devblogs.microsoft.com/directx/announcing-directx-raytracing-1-2-pix-neural-rendering-and-more-at-gdc-2025/> — DXR 1.2 announce
37. <https://devblogs.microsoft.com/directx/enabling-neural-rendering-in-directx-cooperative-vector-support-coming-soon/> — Cooperative Vectors DX
38. <https://github.com/microsoft/DirectStorage/blob/main/GDeflate/README.md> — GDeflate README

### Khronos / Vulkan
39. <https://www.khronos.org/news/press/khronos-streamlines-development-and-deployment-of-gpu-accelerated-applications-with-vulkan-1.4> — Vulkan 1.4 press
40. <https://www.khronos.org/news/archives/vulkan-introduces-roadmap-2026-and-new-descriptor-heap-extension> — Vulkan Roadmap 2026
41. <https://www.khronos.org/blog/mesh-shading-for-vulkan> — Mesh shading
42. <https://docs.vulkan.org/features/latest/features/proposals/VK_EXT_mesh_shader.html> — VK_EXT_mesh_shader
43. <https://docs.vulkan.org/features/latest/features/proposals/VK_KHR_cooperative_matrix.html> — VK_KHR_cooperative_matrix
44. <https://docs.vulkan.org/features/latest/features/proposals/VK_NV_cooperative_matrix2.html> — VK_NV_cooperative_matrix2
45. <https://docs.vulkan.org/features/latest/features/proposals/VK_NV_cooperative_vector.html> — VK_NV_cooperative_vector
46. <https://www.khronos.org/news/press/khronos-group-launches-slang-initiative-hosting-open-source-compiler-contributed-by-nvidia> — Slang Initiative

### Apple
47. <https://www.apple.com/newsroom/2024/05/apple-introduces-m4-chip/> — M4 introduce
48. <https://www.apple.com/newsroom/2025/03/apple-unveils-new-mac-studio-the-most-powerful-mac-ever/> — Mac Studio M3 Ultra / M4 Max
49. <https://developer.apple.com/videos/play/tech-talks/111375/> — GPU M3 / A17 Pro
50. <https://developer.apple.com/documentation/visionos/understanding-the-visionos-render-pipeline> — visionOS pipeline
51. <https://developer.apple.com/videos/play/wwdc2024/10092/> — Render Metal passthrough visionOS

### Sony / Microsoft Xbox / Nintendo / Meta / Valve
52. <https://blog.playstation.com/2026/02/27/upgraded-pssr-upscaler-is-coming-to-ps5-pro/> — PSSR 2 blog
53. <https://developers.meta.com/horizon/blog/save-gpu-with-eye-tracked-foveated-rendering/> — Meta ETFR
54. <https://developers.meta.com/horizon/documentation/unity/os-fixed-foveated-rendering/> — Fixed FR
55. <https://developers.meta.com/horizon/documentation/unity/unity-eye-tracked-foveated-rendering/> — ETFR docs

### Otros (secundarias autoritativas)
56. <https://www.phoronix.com/news/Vulkan-Roadmap-2026> — Vulkan Roadmap 2026 coverage
57. <https://github.com/GameTechDev/SamplerFeedbackStreaming> — SFS demo Intel
58. <http://shader-slang.org/> — Slang site
59. <https://shader-slang.org/blog/2024/11/20/theres-a-lot-going-on-with-slang/> — Slang blog
60. <https://www.tomshardware.com/pc-components/gpus/nvidia-announces-rubin-gpus-in-2026-rubin-ultra-in-2027-feynam-after> — Rubin roadmap
61. <https://www.tomshardware.com/pc-components/gpus/desktop-gpu-roadmap-nvidia-rubin-amd-udna-and-intel-xe3-celestial> — Roadmap 3-way
62. <https://en.wikipedia.org/wiki/GeForce_RTX_50_series> — RTX 50 wiki
63. <https://en.wikipedia.org/wiki/Radeon_RX_9000_series> — RX 9000 wiki
64. <https://en.wikipedia.org/wiki/Apple_M4> — M4 wiki
65. <https://en.wikipedia.org/wiki/Nintendo_Switch_2> — Switch 2 wiki
66. <https://github.com/gpuweb/gpuweb/wiki/Implementation-Status> — WebGPU impl status
67. <https://web.dev/blog/webgpu-supported-major-browsers> — WebGPU browsers

---

## 21. Notas finales para /root/repos/alze-engine

- Tres features que, si los tienes **en 2026**, proteges la viabilidad del motor hasta 2030:
  1. Work Graphs compute-nodes *path* (DX12) + VK fallback con device_generated_commands.
  2. DXR 1.2 (SER + OMM) path para RT, con BVH estructura Mega-Geometry-friendly (cluster-based).
  3. Neural-agnostic upscale layer (DLSS/FSR/XeSS/MetalFX plug-ins + fallback TAAU propio).
- Tres features que puedes diferir a 2027-2028 sin penalizar:
  - Cooperative Vectors / neural shaders in-pass.
  - Mesh nodes en Work Graphs.
  - WebGPU backend (a menos que sea producto).
- Una trampa que vas a querer evitar: *over-fitting* a NVIDIA. El instalado base console 2027+ es 100% AMD. Cada decisión que asume tensor cores NV debe tener un equivalente WMMA/cooperative_matrix AMD.

Revisar documento completo en Q2 2027 (tras launch Xbox Next, UDNA desktop, PS6 confirmación ventana) y Q2 2029.
