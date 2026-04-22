# ALZE Engine — Research & Documentation Bundle

**Snapshot:** 2026-04-22
**Branch:** `docs/research-bundle-2026-04-22` (orphan — contiene solo documentación, no código)
**Origen:** [ibra1927jn/alze-engine](https://github.com/ibra1927jn/alze-engine)

Este bundle agrupa **55 archivos markdown / ~22 000 líneas** de research e investigación producidos para informar el diseño del motor ALZE (`C++17`, no-RTTI, no-exceptions, SDL2+OpenGL 3.3, ~55k LOC). Cinco rondas de research paralelizadas (R1-R5) + un deep dive final cubriendo construcción interna + futuro.

```bash
# Clone sólo esta branch (descarga ~1 MB en lugar del repo completo)
git clone -b docs/research-bundle-2026-04-22 --depth 1 \
  https://github.com/ibra1927jn/alze-engine.git alze-docs
```

---

## 🧭 Por dónde empezar

Para lectores nuevos:

1. **[`research/_MASTER_SINTESIS.md`](research/_MASTER_SINTESIS.md)** — síntesis maestra cruzando las 5 rondas. Decisiones clave + roadmap.
2. **[`repo_docs/README.md`](repo_docs/README.md)** — overview público del motor (estado actual, arquitectura, roadmap de fases).
3. **[`repo_docs/ANALISIS_PROFUNDO.md`](repo_docs/ANALISIS_PROFUNDO.md)** — auto-análisis del motor por dentro.

Para ingenieros profundizando en un área específica: ver la tabla por capas abajo.

---

## 📚 Estructura

```
.
├── README.md                            ← este archivo
├── repo_docs/                           ← snapshot de docs del repo
│   ├── README.md
│   ├── ANALISIS_PROFUNDO.md
│   ├── PROGRESS.md
│   ├── ERRORES.md
│   ├── CLAUDE.md
│   └── SECURITY_RULES.md
└── research/
    ├── _MASTER_SINTESIS.md              ← cross-rounds synthesis R1→R5
    ├── _sintesis_r1.md .. _sintesis_r5.md
    ├── _r3_r4_r5_plan.md
    ├── _errors_r1.md
    ├── r1/  (7) — libs + engines OSS + physics fundamentals
    ├── r2/  (7) — engines AAA propietarios (Rockstar, CDPR, Capcom, Ubisoft, Konami, Activision, IOI, etc.)
    ├── r3/  (7) — rendering SOTA 2024-2026 (Nanite, Lumen, mesh shaders, RT, neural, frame graph, VT)
    ├── r4/  (7) — engines AAA adicionales (Decima, Naughty Dog, Insomniac, Santa Monica, FromSoftware, id Tech 7/8, Anvil)
    ├── r5/  (7) — cross-cutting systems (audio, networking, animation, jobs, memory, editor, DCC pipeline)
    └── deep_2026_04_22/  (6) — construcción + futuro: cómo se hacen motores + hacia dónde van 2026-2032
```

---

## 🗂️ Índice completo (55 archivos)

### Síntesis (8 archivos)

| Archivo | Contenido |
|---|---|
| [`research/_MASTER_SINTESIS.md`](research/_MASTER_SINTESIS.md) | Síntesis cross-rounds R1→R5. 35 agentes, 13 800 L research, decisiones y roadmap. |
| [`research/_sintesis_r1.md`](research/_sintesis_r1.md) | R1 summary: libs + engines OSS + physics. Tabla comparativa + 15 anti-patterns. |
| [`research/_sintesis_r2.md`](research/_sintesis_r2.md) | R2 summary: 7 engines AAA propietarios. Meta-lección: ROI de engine propietario = 5+ títulos shipped. |
| [`research/_sintesis_r3.md`](research/_sintesis_r3.md) | R3 summary: rendering SOTA 2024-2026. |
| [`research/_sintesis_r4.md`](research/_sintesis_r4.md) | R4 summary: engines AAA que saltó R2. |
| [`research/_sintesis_r5.md`](research/_sintesis_r5.md) | R5 summary: cross-cutting systems. |
| [`research/_r3_r4_r5_plan.md`](research/_r3_r4_r5_plan.md) | Plan original de las rondas 3/4/5. |
| [`research/_errors_r1.md`](research/_errors_r1.md) | Errores de WebFetch de R1. |

### R1 — Motors OSS + libs + physics fundamentals (7 archivos)

| Archivo | Tema |
|---|---|
| [`r1/ue5.md`](research/r1/ue5.md) | Unreal Engine 5: Nanite + Lumen deep dive. |
| [`r1/unity_godot.md`](research/r1/unity_godot.md) | Unity DOTS/Burst + Godot 4 Servers pattern. |
| [`r1/ecs_engines.md`](research/r1/ecs_engines.md) | Bevy + Flecs + EnTT. Archetype vs sparse-set decisión. |
| [`r1/aaa_engines.md`](research/r1/aaa_engines.md) | id Tech + Source 2 + Frostbite + CryEngine + Decima + Northlight + Creation. |
| [`r1/rendering_libs.md`](research/r1/rendering_libs.md) | bgfx + Filament + Falcor + Magnum + Diligent + wgpu + Sokol + The Forge. |
| [`r1/physics_3d_industry.md`](research/r1/physics_3d_industry.md) | PhysX + Havok + Bullet + Jolt. |
| [`r1/physics_specialized.md`](research/r1/physics_specialized.md) | Rapier + Box2D v3 + MuJoCo + XPBD + FleX + PBD lib. |

### R2 — Engines AAA propietarios (7 archivos)

| Archivo | Engine · Games |
|---|---|
| [`r2/rage_rockstar.md`](research/r2/rage_rockstar.md) | RAGE · GTA V/VI, RDR2, Euphoria. |
| [`r2/redengine_cdpr.md`](research/r2/redengine_cdpr.md) | REDengine · Witcher, CP77. Case study: migración a UE5. |
| [`r2/re_engine_capcom.md`](research/r2/re_engine_capcom.md) | RE Engine · RE2-4 remake, DMC5, DD2, MHW. Dual-path VM. |
| [`r2/snowdrop_ubisoft.md`](research/r2/snowdrop_ubisoft.md) | Snowdrop · The Division, Avatar, Star Wars Outlaws. |
| [`r2/fox_engine_kojima.md`](research/r2/fox_engine_kojima.md) | Fox Engine · MGSV, PES. El engine que murió con su equipo. |
| [`r2/iw_slipspace_fps.md`](research/r2/iw_slipspace_fps.md) | IW Engine + Slipspace · Call of Duty, Halo Infinite. |
| [`r2/glacier_dunia_misc.md`](research/r2/glacier_dunia_misc.md) | Glacier 2 + Dunia + Creation 2 + Luminous · Hitman, Far Cry, Starfield, FFXV. |

### R3 — Rendering SOTA 2024-2026 (7 archivos)

| Archivo | Tema |
|---|---|
| [`r3/nanite.md`](research/r3/nanite.md) | Virtualized geometry: cluster hierarchy, streaming, software rasterizer. |
| [`r3/lumen.md`](research/r3/lumen.md) | Software + hardware Lumen, world-space radiance cache, VSM. |
| [`r3/mesh_shaders_work_graphs.md`](research/r3/mesh_shaders_work_graphs.md) | Task/mesh shaders, amplification, DX12 work graphs. |
| [`r3/ray_tracing_2024_2026.md`](research/r3/ray_tracing_2024_2026.md) | BVH build, RTXDI/ReSTIR, path tracing, shader execution reordering. |
| [`r3/neural_rendering.md`](research/r3/neural_rendering.md) | DLSS4, FSR4, XeSS, NRC, neural texture compression, 3DGS en games. |
| [`r3/frame_graph_bindless.md`](research/r3/frame_graph_bindless.md) | Frostbite FrameGraph + UE5 RDG + bindless + transient aliasing. |
| [`r3/virtual_textures_streaming.md`](research/r3/virtual_textures_streaming.md) | Megatexture → Decima → UE5 VT, sparse textures, SFS. |

### R4 — Engines AAA que saltó R2 (7 archivos)

| Archivo | Engine · Games |
|---|---|
| [`r4/decima.md`](research/r4/decima.md) | Decima · Horizon Zero Dawn/Forbidden West, Death Stranding 1/2. |
| [`r4/naughty_dog.md`](research/r4/naughty_dog.md) | ND engine · TLOU2, UC4, Intergalactic. Fiber jobs + cinematic pipeline. |
| [`r4/insomniac.md`](research/r4/insomniac.md) | Insomniac engine · Spider-Man 1/2, Ratchet Rift Apart. Fast loading. |
| [`r4/santa_monica.md`](research/r4/santa_monica.md) | Santa Monica · GoW 2018/Ragnarok. One-take camera, Norse pipeline. |
| [`r4/fromsoftware.md`](research/r4/fromsoftware.md) | FS engine · Souls/Bloodborne/Sekiro/Elden Ring. |
| [`r4/id_tech_7_8.md`](research/r4/id_tech_7_8.md) | id Tech 7/8 · Doom Eternal, Indiana Jones. Async compute + mesh shaders. |
| [`r4/anvil_ubisoft.md`](research/r4/anvil_ubisoft.md) | AnvilNext / Anvil Pipeline · AC Shadows, Valhalla. |

### R5 — Cross-cutting systems (7 archivos)

| Archivo | Tema |
|---|---|
| [`r5/audio.md`](research/r5/audio.md) | Wwise, FMOD, Steam Audio, Atmos, ray-traced audio, miniaudio. |
| [`r5/networking.md`](research/r5/networking.md) | Rollback (GGPO), replication (UE RG), Source networking, deterministic sim. |
| [`r5/animation.md`](research/r5/animation.md) | Motion matching, learned MM, physics-based, IK2, cloth/hair (Groom, HairWorks). |
| [`r5/job_systems.md`](research/r5/job_systems.md) | ND fibers, TBB, Unity Jobs+Burst, Frostbite task graph, std::execution. |
| [`r5/memory_allocators.md`](research/r5/memory_allocators.md) | Arenas, handles, streaming, virtual memory tricks, huge pages. |
| [`r5/editor_architecture.md`](research/r5/editor_architecture.md) | Undo/redo, serialization, hot reload, PIE, Inspector. |
| [`r5/dcc_asset_pipeline.md`](research/r5/dcc_asset_pipeline.md) | USD, glTF, Houdini Engine, Blender bridge, MaterialX, DDC. |

### Deep dive 2026-04-22 — Construcción + futuro (6 archivos, ~6 250 líneas)

| Archivo | Tema |
|---|---|
| [`deep_2026_04_22/graphics_engine_construction.md`](research/deep_2026_04_22/graphics_engine_construction.md) | Cómo se construye un motor gráfico moderno capa por capa: RHI, resource mgmt, frontend, frame graph, draw submission, shading, postfx, DDC, debug. 1 517 L + pseudocódigo C++17. |
| [`deep_2026_04_22/physics_engine_construction.md`](research/deep_2026_04_22/physics_engine_construction.md) | Cómo se construye un motor de física moderno: world, broadphase, narrowphase, solvers (SI/TGS/XPBD), islands, CCD, SPH, determinism. 1 209 L + pseudocódigo. |
| [`deep_2026_04_22/rendering_future.md`](research/deep_2026_04_22/rendering_future.md) | Futuro del rendering 2026-2032: RT default, neural rendering, 3DGS, Work Graphs, AI content, GPU roadmaps. 697 L. |
| [`deep_2026_04_22/physics_future.md`](research/deep_2026_04_22/physics_future.md) | Futuro de física 2026-2032: GPU-unified, differentiable, neural surrogates, AVBD, MPM, learned animation. 770 L. |
| [`deep_2026_04_22/hardware_apis_future.md`](research/deep_2026_04_22/hardware_apis_future.md) | Hardware + APIs 2026-2032: NVIDIA/AMD/Intel/Apple roadmaps, consolas gen 10, Vulkan/DX12/Metal, Slang, WebGPU. 811 L. |
| [`deep_2026_04_22/cross_cutting_patterns.md`](research/deep_2026_04_22/cross_cutting_patterns.md) | Patrones cross-cutting modernos: DOD, ECS, fibers, allocators, determinism, rollback, reflection, hot reload, scripting, testing, C++23/26. 1 247 L. |

### Repo docs (6 archivos) — snapshot del estado del repo

| Archivo | Contenido |
|---|---|
| [`repo_docs/README.md`](repo_docs/README.md) | Overview público: estado, arquitectura, roadmap de fases. |
| [`repo_docs/ANALISIS_PROFUNDO.md`](repo_docs/ANALISIS_PROFUNDO.md) | Auto-análisis del motor por dentro. |
| [`repo_docs/PROGRESS.md`](repo_docs/PROGRESS.md) | Progreso por fases. |
| [`repo_docs/ERRORES.md`](repo_docs/ERRORES.md) | Errores encontrados + lecciones. |
| [`repo_docs/CLAUDE.md`](repo_docs/CLAUDE.md) | Reglas operacionales del agente. |
| [`repo_docs/SECURITY_RULES.md`](repo_docs/SECURITY_RULES.md) | Constraints de seguridad. |

---

## 📊 Estadísticas

| Métrica | Valor |
|---|---|
| Archivos totales | 55 .md + este README |
| Líneas markdown | ~22 000 |
| Tamaño | ~1.1 MB texto |
| Rondas de research | 5 (R1-R5) + 1 deep dive |
| Agentes paralelos totales | 35 (R1-R5) + 6 (deep dive) = 41 |
| Fuentes primarias citadas | ≥800 URLs (papers SIGGRAPH/GDC/CppCon/arxiv/whitepapers oficiales) |
| Engines estudiados | 21 (UE5, Unity, Godot, Bevy, id Tech, Source 2, Frostbite, CryEngine, Decima, Northlight, Creation, RAGE, REDengine, RE Engine, Snowdrop, Fox, IW, Slipspace, Glacier 2, Dunia, Luminous) + Naughty Dog engine, Insomniac, Santa Monica, FS engine, Anvil |

---

## 🔖 Versiones y regeneración

Este snapshot se generó el **2026-04-22**. Para regenerar o añadir una nueva ronda de research, ver el patrón en [`research/_r3_r4_r5_plan.md`](research/_r3_r4_r5_plan.md).

Si detectas errores factuales o enlaces rotos, abre un issue en el repo principal marcando `docs/research-bundle`.

---

**ALZE Engine** — *Creado para ser real, no para ser un ejercicio.*
