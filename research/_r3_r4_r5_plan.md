# alze_engine — Plan de profundización rounds 3/4/5

**Fecha:** 2026-04-22
**Motivación:** el usuario pidió "más research y más profundidad" sobre alze_engine después de R1 (paisaje + libs + physics) y R2 (7 engines AAA historia).

**Gaps identificados al cierre de R1+R2:**
1. Técnicas concretas de rendering 2024-2026 (Nanite, Lumen, mesh shaders, work graphs, path tracing, neural rendering) quedaron como nombres, no como algoritmos.
2. Engines AAA que NO se cubrieron: Decima (Guerrilla/Kojima), Naughty Dog engine (TLOU/UC), Insomniac (Spider-Man/Ratchet), Santa Monica (GoW), FromSoftware (Souls/ER), id Tech 7/8 (Doom Eternal/Indy), Anvil/AnvilNext (AC).
3. Sistemas cross-cutting no-gráficos apenas mencionados: audio, networking, animación, job systems, memoria, editor, DCC pipelines.

## Round 3 — Rendering / GPU SOTA 2024-2026 (7 agentes)
Target: `/root/lab_journal/research/alze_engine/r3/`

1. `nanite.md` — virtualized geometry: cluster hierarchy, streaming, rasterizer deep dive
2. `lumen.md` — software + hardware Lumen, world-space radiance cache, VSM
3. `mesh_shaders_work_graphs.md` — task/mesh shaders, amplification, DX12 work graphs
4. `ray_tracing_2024_2026.md` — BVH build, RTXDI/ReSTIR, path tracing, shader exec reordering
5. `neural_rendering.md` — DLSS4, FSR3/4, XeSS, NRC, neural texture comp, 3DGS en games
6. `frame_graph_bindless.md` — Frostbite FrameGraph + UE5 RDG + bindless + transient aliasing
7. `virtual_textures_streaming.md` — Megatexture → Decima → UE5 VT, sparse textures, SVT

## Round 4 — AAA engines que saltamos (7 agentes)
Target: `/root/lab_journal/research/alze_engine/r4/`

1. `decima.md` — Guerrilla Games / Horizon ZD+FW / Death Stranding 1-2
2. `naughty_dog.md` — ND engine (TLOU2, UC4, Intergalactic), fiber jobs, cinematic pipeline
3. `insomniac.md` — Insomniac engine (Spider-Man 1-2, Ratchet Rift Apart) — fast loading
4. `santa_monica.md` — Santa Monica engine (GoW 2018/Ragnarok), one-take camera, Norse mythology pipeline
5. `fromsoftware.md` — FS engine (Souls/Bloodborne/Sekiro/Elden Ring) — archive format, level design
6. `id_tech_7_8.md` — Doom Eternal (id Tech 7), Indiana Jones (id Tech 8), async compute + mesh shaders
7. `anvil_ubisoft.md` — AnvilNext 2.0 / Scimitar → Anvil Pipeline (AC Shadows, Valhalla)

## Round 5 — Cross-cutting engine systems (7 agentes)
Target: `/root/lab_journal/research/alze_engine/r5/`

1. `audio.md` — Wwise / FMOD / Steam Audio / Atmos / ray-traced audio / miniaudio
2. `networking.md` — rollback (GGPO), replication (UE RG), Source networking, deterministic sim
3. `animation.md` — motion matching, learned MM, physics-based, IK2, cloth/hair (Groom, HairWorks)
4. `job_systems.md` — Naughty Dog fibers, TBB, Unity Jobs+Burst, Frostbite task graph, std::execution
5. `memory_allocators.md` — arenas, handles, streaming allocators, virtual memory tricks, large pages
6. `editor_architecture.md` — undo/redo, serialization, hot reload, PIE, Inspector
7. `dcc_asset_pipeline.md` — USD, glTF, Houdini Engine, Blender bridge, MaterialX, DDC

## Política
- Cada agente produce 200-400 L md con URLs primarias (autor año venue GDC/SIGGRAPH/blog + paper PDF si existe).
- Foco en algoritmos + data structures + números concretos (MB, ms, throughput), no solo overview.
- Mencionar aplicabilidad a ALZE: "¿copiable en C++17 + GL 3.3 hoy, en Vulkan v2, o aspiracional v3?"
- Un `_sintesis.md` por round al cierre con tabla comparativa + top ideas + anti-patterns.

**Total estimado**: 21 agentes + 3 síntesis = ~6000-8000 líneas de research.
