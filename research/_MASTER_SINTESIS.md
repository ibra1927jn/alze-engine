# Master Síntesis — alze_engine research completa (R1-R5)

**Fecha:** 2026-04-22
**Input total:** 35 agentes paralelos en 5 rondas, ~13,800 líneas research, ~60 archivos.

| Ronda | Foco | Agentes | Líneas | Archivos |
|---|---|---|---|---|
| R1 | Paisaje engines + libs + physics | 7 | 1,242 | 7 |
| R2 | 7 engines AAA (estado salvamento) | 7 | 1,606 | 7 |
| R3 | Rendering / GPU SOTA 2024-2026 | 7 | 3,156 | 7 |
| R4 | 7 engines AAA prosperando/únicos | 7 | 3,328 | 7 |
| R5 | Cross-cutting systems (no-rendering) | 7 | 4,470 | 7 |
| **Total** | 21 engines + 14 subsistemas | **35** | **13,802** | **35 research + 5 sintesis + 1 master** |

Este documento **reemplaza** la pregunta "¿qué debe ser ALZE?" con una respuesta concreta.

---

## Target reiterado

`/root/repos/alze-engine` — C++17, -fno-rtti -fno-exceptions, SDL2 + OpenGL 3.3 hoy, Vulkan 1.2+ futuro. Solo-dev. Fase 7 (PBR+ECS+Physics+Audio planned). ~25-30k LOC currently, target v1 final ~50-70k LOC, v2 final ~100-120k LOC.

**Scope decisión crítica (no tomada aún)**: 1-2 géneros de juego que shippeará ALZE. R2+R4 demuestran que engines sin scope mueren (Luminous) o se ahogan en cross-genre (Frostbite en Mass Effect Andromeda). **Recomendación mental**: narrow narrative-action (tipo Santa Monica/FromSoft) o FPS-arena (tipo id Tech). NO open-world continent scale.

---

## Stack definitivo ALZE v1 (GL 3.3, hoy mismo)

Por capa, con LOC estimado y refs a archivos research:

### Core infrastructure (~3,500 LOC)
- **Handle-based allocator**: `Handle<T,Gen>` 32+32 bit. `~500 LOC`. R5/memory_allocators.md §5.
- **Per-frame arena 32 MB + per-type pools**: zero malloc hot paths. `~800 LOC`. R5/memory_allocators §§2-3.
- **Job system**: N=min(cores,8) std::thread pool + task DAG explicit deps. `~1,500 LOC`. R5/job_systems §§1-2.
- **C++20 coroutines for I/O**: asset loading, texture streaming. `~300 LOC`. R5/job_systems §5.
- **xsimd hot loops**: 3-5 loops (culling, particle, anim, physics). `~400 LOC measurable`. R5/job_systems §9.

### ECS (~3,000 LOC)
- **Archetype SoA storage** (Bevy/Flecs/Mass pattern): BlobVec + per-type pools.
- **ComponentAccess<R,W> scheduler**: automatic parallelism detection.
- **Scene via PackedScene composition** (Godot pattern) + `(relation,target)` pairs (Flecs).
R1/ecs_engines.md has the C++17 blueprint explicitly.

### Rendering RHI abstraction (~4,500 LOC)
- **Opaque Handle API like bgfx** (no link bgfx): 64-bit gen+index handles for buffers/textures/pipelines/shaders.
- **Backend único GL 3.3** v1. Design for Vulkan 1.2 port v2 without game-code changes.
- **Sort-key render queue** 64-bit: `[layer:4][view:4][blend:2][shader:18][material:18][depth:18]`. `~600 LOC`. R3/frame_graph_bindless §13.6.
- **HLSL shaders → dxc → SPIR-V → SPIRV-Cross → GLSL 330**. Single source, N outputs. R1/rendering_libs.

### Rendering features (~5,000 LOC)
- **PBR GGX + split-sum IBL** (Filament math ported). `~800 LOC shader + baker offline`.
- **Deferred base + forward transparents**. `~600 LOC`.
- **Cascaded Shadow Maps** (no VSM yet). `~400 LOC`.
- **Nubis volumetric clouds** (Schneider 2013+2017): Perlin-Worley 128³+ Worley 32³ + curl noise + HG phase + temporal reproject 1-in-16. **~1,500 LOC total**. R4/decima §4 — *the biggest bang-per-buck spectacle in entire engine*.
- **Two-pass HZB occlusion culling** (Nanite-style but classic): `~500 LOC` compute. R3/nanite §3.
- **Sort-key + indirect draw** (CPU-side cluster culling). `~300 LOC`.
- **KTX2 + Basis Universal**: replace stb_image DXT pipeline. `~200 LOC`. R3/virtual_textures §6.

### Physics (~6,500 LOC)
- **Sequential-Impulse solver** (Catto GDC 2006): box, sphere, capsule. `~1,500 LOC`.
- **Dbvt broadphase**: dynamic AABB tree. `~600 LOC`.
- **GJK + EPA narrowphase** (Ericson RTCD ch 9) + SAT fast path for box-box. `~800 LOC`.
- **Islands + graph coloring** parallelism (Jolt + Rapier + Box2D v3 pattern). `~400 LOC`.
- **XPBD cloth** single-chain spring (capes, hair secondary). `~500 LOC`. R5/animation §7.
- **Character controller**: capsule + raycasts + step-up + slope. `~800 LOC`.
- **Ragdoll** with active constraints for hit reactions. `~1,500 LOC`.
- **Vehicle** (optional, scope-dependent): `~400 LOC`.

### Animation (~4,200 LOC)
- **Ozz-animation port**: skeletal + blend trees + IK. `~2,500 LOC`. R5/animation §§1-2, 10.
- **2-bone IK** (CCD or closed-form) for feet placement uneven terrain. `~800 LOC`. R5/animation §6.
- **Spring chain**: hair/cape/tail secondary motion. `~500 LOC`.
- **Procedural layer**: foot IK + head-look + weapon IK. `~400 LOC`.

### Audio (~3,000 LOC)
- **miniaudio** (David Reid, MIT single-header). Integrated directly.
- **Custom DSP graph** in C++17: filters, reverb, mixing. `~1,500 LOC`. R5/audio.
- **Event layer** (animation event → audio event). `~500 LOC`.
- **HRTF fallback** via OpenAL Soft when 3D becomes gameplay-relevant (v1.5 upgrade). `~1,000 LOC integration`.

### Editor (~5,500 LOC)
- **Dear ImGui** inside game exe. Toggle play-mode / edit-mode. R5/editor_architecture §1.
- **ImGuizmo** for viewport manipulation. Zero custom LOC.
- **JSON scene serialization** (git-friendly). `~600 LOC`. R5/editor §2.
- **Hand-written inspectors** per component (no reflection). `~20 LOC × N components = ~2k LOC`.
- **Undo/redo Command pattern** with slider coalescing. `~400 LOC`. R5/editor §3.
- **Asset browser + thumbnails** via offline generation. `~800 LOC`.
- **Shader hot-reload**: filewatcher + dxc recompile. `~300 LOC`. R5/editor §4.

### Input + windowing (~500 LOC)
- **SDL2** (SDL3 when stable). Already in place. ALZE existente.

### DCC + asset pipeline (~1,500 LOC importer code, 0 runtime)
- **Blender + glTF 2.0** source. **cgltf + stb_image + KTX2-software** runtime. Already integrated.
- **Baker CLI** (offline tool): glTF → engine pack format (bin mesh + KTX2 textures + JSON manifest). `~1,000 LOC`.
- **Content-hash DDC**: cache derived outputs by hash(src+importer_ver+platform). `~500 LOC`.
- **Substance Painter** (license $240/yr hero-tier only) + **Megascans** (free con Epic account).

**TOTAL v1 sum**: ~37,200 LOC across subsystems. **Current ALZE is ~25-30k** → ~12-17k LOC remaining to reach v1 complete.

---

## Roadmap v1→v2→v3

### v1 (current → next 12-18 meses solo-dev)
Completar los 12-17k LOC restantes. Target: **vertical slice defendible** — 1 nivel jugable con ECS + rendering PBR + physics + anim + audio + editor todo funcionando. NO Vulkan, NO RT, NO mesh shaders, NO motion matching, NO networking.

Prioridad milestone order:
1. **[M1]** RHI + ECS foundations (2-3 meses). Cubo rendering PBR con IBL. 100 entities con 3 sistemas paralelizados.
2. **[M2]** Physics + character controller (3 meses). 100 cajas cayendo + walking character sobre terreno.
3. **[M3]** Animation + Ozz + IK (2 meses). Skeletal char con walking blend + foot IK.
4. **[M4]** Audio + events (1-2 meses). Footsteps + music stems + event system.
5. **[M5]** Editor (3-4 meses). ImGui inside game + JSON scenes + ImGuizmo + inspectors.
6. **[M6]** Nubis clouds + sort-key renderer polish + KTX2 pipeline (1-2 meses).
7. **[M7]** Vertical slice content: un nivel playable end-to-end.

### v2 (año 2-3)
- **Vulkan 1.2 migration**: backend alternativo detrás de RHI abstraction. Bindless descriptor indexing diseñado aquí.
- **Frame Graph DAG** (O'Donnell pattern ~2-3k LOC).
- **Meshlet + cluster culling** (CPU offline + GPU compute cull + indirect draw) ~3-5k LOC.
- **DDGI** dynamic GI (Majercik 2019) ~2-3k LOC.
- **FSR 2.x upscaler** integration (MIT open source).
- **XPBD full cloth + soft body** (Macklin Small Steps 2019).
- **Motion matching** — classical (Kovar 2002 + Clavet 2016) si hay budget motion database. Alternativa: blend trees mejorados.
- **C++ hot-reload** (Live++ o RCC++) si flujo editor lo justifica.
- **Optional**: RVT (Runtime Virtual Textures) para terrain.
- **Optional**: spatial audio HRTF via OpenAL Soft.

### v3 (aspiracional, si ALZE crece a AAA-aspirant)
- **Hardware RT**: DXR / VK_KHR_ray_tracing para reflections + shadows.
- **Neural Texture Compression** (Vaidyanathan 2023) offline bake.
- **Learned Motion Matching** (Holden 2020 ~3k LOC + asset pack).
- **Rollback netcode** (GGPO pattern) si multiplayer se vuelve scope.
- **3D Gaussian Splatting** para scenes scanned.
- **Work Graphs DX12** si cross-vendor Vulkan equivalent emerge.
- **USD** si ALZE se vuelve DCC-heavy (Omniverse-style).

---

## Top 40 ideas destiladas (R1+R2+R3+R4+R5)

Agrupadas por capa. Cada una apuntada a archivo origen.

### Data + architecture
1. Archetype SoA ECS (Bevy/Flecs pattern) — R1/ecs_engines
2. ComponentAccess<R,W> scheduler auto-paralelismo — R1/ecs_engines
3. Handle<T,Gen> en todas partes — R5/memory
4. Per-frame arena + per-type pools — R5/memory
5. `-fno-rtti -fno-exceptions` disciplina (ALZE ya así) — R1/rendering_libs
6. Content-hash DDC cache — R1/aaa_engines
7. Stable plugin ABI (Creation lesson, si mod support) — R2/glacier_dunia_misc

### Rendering
8. RHI opaque handles (bgfx pattern) — R1/rendering_libs
9. HLSL → dxc → SPIR-V → SPIRV-Cross → GLSL — R1/rendering_libs
10. Sort-key render queue v1 — R3/frame_graph_bindless
11. Frame Graph DAG v2 (O'Donnell) — R3/frame_graph_bindless
12. Bindless descriptor indexing v2 — R3/frame_graph_bindless
13. PBR GGX + split-sum IBL (Filament math) — R1/rendering_libs
14. Nubis volumetric clouds (Schneider) — R4/decima + R1/aaa_engines
15. Two-pass HZB occlusion — R3/nanite
16. Meshlet + cluster culling v2 — R3/nanite + R3/mesh_shaders
17. DDGI dynamic GI (Majercik 2019) v2 — R3/lumen tabla comparativa
18. FSR 2.x upscaler v2 (cross-vendor MIT) — R3/neural_rendering
19. Checkerboard + TAAU (HZD Pro pattern) — R4/decima §6
20. Velocity-aware cone streaming (Rockstar/Decima) — R4/decima §2 + R2/rage
21. KTX2 + Basis transcoding v1 — R3/virtual_textures
22. RVT terrain v2 — R3/virtual_textures

### Physics
23. Sequential-Impulse solver (Catto) — R1/physics_3d_industry
24. Dbvt broadphase — R1/physics_3d_industry
25. Islands + graph coloring parallelism — R1/physics_specialized
26. XPBD cloth (Macklin Small Steps) — R1/physics_specialized
27. Active ragdoll (ND TLOU2 pattern) — R4/naughty_dog
28. Grid-based propagation sim (fire/disease) — R2/glacier_dunia_misc

### Animation
29. Ozz-animation lib port — R5/animation
30. 2-bone IK closed-form — R5/animation §6
31. Spring chain secondary motion — R5/animation §9
32. Motion matching classical v2 (Kovar/Clavet) — R5/animation §3
33. Learned MM v3 (LaForge 2020) — R5/animation §4

### Audio
34. miniaudio + custom DSP graph v1 — R5/audio
35. OpenAL Soft HRTF v2 — R5/audio §5

### Cross-cutting
36. Fixed timestep + input-as-commands (net-ready) — R5/networking
37. Bark dialogue system (Spider-Man/RDR2) — R4/insomniac + R2/rage
38. Hero-state FSM (web swing / grapple pattern) — R4/insomniac §3
39. 4-cubemap weather probe (Fox Engine) — R2/fox_engine_kojima
40. Spring-arm rig + camera-aware nav modifier (Santa Monica) — R4/santa_monica

---

## Anti-patterns consolidados (44 total across R1-R5)

**No repetir** — errores documentados en múltiples engines o literatura:

### Architectural
1. Tres render pipelines paralelos (Unity) — R1
2. GameObject.Find + reflection (Unity legacy) — R1
3. GDScript/Blueprint first-class scripting — R1
4. Custom UI framework — R1
5. Custom archive format en v1 — R1 + R5/dcc
6. Custom physics desde 0 complet — R1
7. Shader permutation explosion — R1
8. EnTT sparse-set para iteración-heavy worlds — R1
9. IL2CPP-style transpilation — R1
10. Rust borrow model en C++ — R1
11. Full FEM tetrahedra soft body v1 — R1
12. CUDA-only physics (FleX) — R1
13. Frostbite trauma cross-genre — R1
14. Tooling rewrites cada 5 años — R1
15. Virtual textures en v1 — R1

### Engine strategy (R2)
16. Engine propietario sin 3+ títulos shipped — R2/redengine
17. Engine rewrite while shipping — R2/slipspace
18. Under-staffed engine team paralelo a game team — R2
19. Cross-genre forcing — R2/fox_engine
20. Licensing Euphoria — R2/rage
21. Custom scripting sin dual-path — R2/re_engine
22. Continent-scale sim sin 100+ devs — R2/rage
23. P2P netcode para MP (scaling players baja tick) — R2
24. Annual ship cadence (CoD) — R2/iw_slipspace
25. Engine dissolution antes de estabilizar — R2/luminous

### Rendering (R3)
26. Intentar Nanite clone solo-dev — R3/nanite
27. Path tracing v1 (denoiser harder than tracer) — R3/ray_tracing
28. DLSS sin FSR fallback — R3/neural_rendering
29. Frame Graph antes de 10+ passes — R3/frame_graph
30. VT autoritarian (MegaTexture error) — R3/virtual_textures
31. Work Graphs DX12 chasing 2026 — R3/mesh_shaders
32. Mesh shaders como prerequisite — R3/mesh_shaders
33. Hardware Lumen v1 (DDGI es mejor ROI) — R3/lumen

### Engines missed (R4)
34. Engine rewrite mid-ship (Slipspace repeat) — R4/fromsoftware contra-ejemplo
35. PS5-specific SSD architecture en PC engine — R4/insomniac
36. Continent-scale world sin 20-año team — R4/decima
37. Copiar fiber system sin profiling evidence — R4/naughty_dog
38. Photogrammetry sin scan budget — R4 (Megascans alternative)
39. One-take camera hardcoded (policy vs capability) — R4/santa_monica
40. Mod-support sin stable ABI — R4/fromsoftware

### Cross-cutting (R5)
41. Wwise en v1 solo-dev (licensing friction) — R5/audio
42. Networking implementado v1 (arquitectura sí, ship no) — R5/networking
43. Motion matching sin motion database — R5/animation
44. C++ hot reload (Live++) v1 — R5/editor

---

## Decisiones críticas — orden de commit

Cada una tiene impacto 10+ años. **Tómalas antes de escribir código nuevo**:

### Decisión #1 — Scope del engine
¿Qué 2-3 juegos shippeará ALZE en próximos 5-10 años? Mismo género? Recomendación: narrative-action narrow scope (Santa Monica model) o arena FPS (id Tech model). NO open world, NO cross-genre.

### Decisión #2 — ECS storage
Archetype SoA (Bevy/Flecs/Mass/UE Mass pattern) vs sparse-set (EnTT). Recomendación: **archetype SoA**. R1 consensus.

### Decisión #3 — Scripting model
Dev build interpreter + release AOT-to-C++ (RE Engine REVM) es el killer feature. Alternativas: AngelScript + JIT, o Lua + static-compile-option, o "C++ only no scripting". Si no hay scripting, editor tooling loops se alargan. Decision abierta — si solo-dev + un género, "C++ only" es válido.

### Decisión #4 — Engine evolution strategy
FromSoftware model (iterate forever, no rewrite) vs Slipspace/REDengine (rewrite mid-life = death). **No negociable**: evolve one engine 10+ años.

### Decisión #5 — RHI abstraction level
bgfx-like opaque handles desde día 1. Backend único GL 3.3 v1, Vulkan 1.2 v2. Diseño de API contempla bindless v2 (opaque handles → u32 index later).

### Decisión #6 — Determinism opt-in
Fixed timestep + input-as-commands desde día 1. Enables future rollback/CS/demo replay sin rewrite. ~500 LOC discipline. R5/networking.

### Decisión #7 — Archive format
glTF + KTX2 + JSON scenes en v1. Custom .pack format solo si razón concreta (compression, anti-tamper, patch deltas). No inventar sin motivo. R5/dcc.

---

## LOC budget consolidado

| Layer | v1 LOC | v2 LOC | v3 LOC | Total |
|---|---|---|---|---|
| Core infrastructure | 3,500 | +2,000 (Vulkan RHI) | +1,000 | 6,500 |
| ECS | 3,000 | +1,000 | — | 4,000 |
| Rendering RHI + features | 9,500 | +8,000 (FG + bindless + meshlet + DDGI + FSR) | +10,000 (RT + NTC + 3DGS) | 27,500 |
| Physics | 6,500 | +3,000 (XPBD full + MPM research) | — | 9,500 |
| Animation | 4,200 | +3,000 (MM classical) | +3,000 (LMM + physics-based) | 10,200 |
| Audio | 3,000 | +1,000 (HRTF + Steam Audio-lite) | +2,000 (ray-traced audio) | 6,000 |
| Editor | 5,500 | +3,000 (reflection inspector + C++ hot reload) | — | 8,500 |
| Network | 0 | +4,000 (CS/lockstep) | +3,000 (rollback GGPO) | 7,000 |
| DCC tooling | 1,500 | +1,000 (Houdini Engine integration) | +2,000 (USD partial) | 4,500 |
| **Total** | **36,700** | **+26,000** | **+21,000** | **~83,700 LOC** |

**Realidad solo-dev**: escribir 36,700 LOC en v1 a cadencia 500 LOC/semana = **~74 semanas = 17 meses**. v2 adds ~12 meses. Total v1+v2 = ~2.5 años full-time. Frostbite/UE5 comparison: 100+ engineers, 10+ años.

---

## Engines por lección dominante (14 engines)

| Engine | Lección — ordenada por leverage para ALZE |
|---|---|
| **FromSoftware** | **Evolve one engine 15+ años, no rewrite**. Modelo solo-dev. (R4) |
| **Santa Monica** | Narrow-scope engine es válido. 6 tricks portable concretos. (R4) |
| **Decima** | Predictive cone + Nubis clouds + checkerboard TAAU. (R2+R4) |
| **RE Engine** | Dual-path scripting VM (REVM) = killer feature si se adopta. (R2) |
| **id Tech 7/8** | Frame budget discipline + ubershader policy. Cultura. (R2+R4) |
| **Bevy** | ECS archetype + data-oriented día 1. (R1) |
| **Naughty Dog** | Principles > copiar fiber system. Animation layering TLOU2. (R4) |
| **Godot** | MIT + community resilience. Servers pattern. (R1) |
| **UE5** | Virtualized geometry + unified GI = vanguardia aspiracional. (R1) |
| **Snowdrop** | Graphs + build-time compile tooling. Artist UX sin perf hit. (R2) |
| **Insomniac** | SSD exploit no portable. LRU streaming + hero-state FSM sí. (R4) |
| **Anvil/Ubisoft** | LaForge research papers > copiar engine. (R4) |
| **RAGE (Rockstar)** | Predictive streaming es the truth. Open world scale. (R2) |
| **Glacier 2 (IOI)** | Deterministic NPC schedules. Crowd LOD. (R2) |

**Unity**: vendor-licensing cambio puede matar trust. No dependencia single-vendor.
**Frostbite**: cross-genre forcing kills engines. Scope discipline.
**Fox Engine**: engine muere con su equipo. Succession planning.
**Slipspace**: rewrite-while-shipping = pesadilla textbook.
**Luminous**: no dissolve engine team pre-3-successful-ships.
**Creation 2**: mod ecosystem = moat 20 años (Skyrim).
**REDengine**: proprietary engine ROI = 5+ shipped titles. Si no, UE5.

---

## Qué hacer hoy (actionable)

Si el objetivo es avanzar ALZE HOY mismo, los primeros commits concretos:

1. **Adoptar KTX2 + Basis Universal** reemplazando stb_image DXT. ~200 LOC integración. Impacto inmediato en texture load speed.
2. **Implementar Handle<T,Gen> allocator** y migrar alguna estructura existente (ej. MaterialRef → `Handle<Material>`). ~500 LOC, foundational para todo lo futuro.
3. **Migrar a per-frame arena** en los sistemas que hacen malloc frecuente. Target: <50 allocs/frame CI rule.
4. **Documentar engine evolution strategy** — escribir 1-página "ALZE sigue modelo FromSoftware: evolución incremental, nunca rewrite. Archive format estable. Version bump convention."
5. **Scope decision** — 1 página "ALZE shippea juegos [X] en [género Y] en próximos 5 años." Sin esta decisión, las ideas de los 35 archivos son premature optimization.

---

## Qué NO hacer

1. NO fire más research agents. 13,800 líneas es suficiente. Pasar a implementación.
2. NO empezar Vulkan migration antes de v1 GL 3.3 completo.
3. NO adopt RT / mesh shaders / Nanite clone / Lumen — ninguno aplica solo-dev pragmatic.
4. NO multiplayer v1. Forward-compat arquitectura sí (fixed timestep), implementación no.
5. NO USD v1. glTF es suficiente.
6. NO Wwise v1. miniaudio es suficiente.
7. NO motion matching v1 sin mocap database budget. Blend trees + Ozz es enough.
8. NO custom archive format v1. glTF + KTX2 + JSON cubren todo.
9. NO C++ hot reload v1. Shader hot-reload es suficiente.
10. NO decimal engine dissolution — acabar v1 antes de re-scope.

---

## Cierre

La research está **completa**. 35 archivos + 5 síntesis + este master = ~14,000 líneas de análisis. Ningún agente adicional agregará valor marginal significativo — las decisiones pendientes son ahora **humanas + implementación**, no investigación.

Próximo paso natural: **ejecutar v1 M1 (RHI + ECS foundations)** con el stack arriba definido. Si eso ronda bien en 8-12 semanas, M2 (physics) sigue. No pivotear.

La lección maestra de 35 agentes + 14,000 líneas: **la diferencia entre los engines que sobreviven (FromSoft, id Tech, Santa Monica) y los que mueren (Luminous, Slipspace) no es la tecnología — es la disciplina sobre scope, iteración y cadencia**. ALZE tiene más probabilidad de shippear si adopta esa disciplina que si adopta la última técnica rendering.
