# Síntesis round 5 — Sistemas cross-cutting (audio, net, anim, jobs, memoria, editor, DCC)

**Fecha:** 2026-04-22
**Input:** 7 agentes paralelos, ~4,470 líneas. 0 fallos bloqueantes. Varias correcciones en atribuciones de papers (RTN a Zhou 2018/2019 no Holden, Frostbite 2009 → Andersson 2010 "Parallel Futures").

Complementa R1 (libs+physics), R2 (engines históricos), R3 (rendering SOTA), R4 (engines missed). **R5 cubre todo lo que no es rendering**: los 7 subsistemas que juntos completan un engine real.

- [`r5/audio.md`](r5/audio.md) — Wwise/FMOD/miniaudio/OpenAL Soft, HRTF, Steam Audio, Atmos, ray-traced audio (601 L)
- [`r5/networking.md`](r5/networking.md) — rollback (GGPO), UE ReplGraph, Source netcode, GNS, determinismo (646 L)
- [`r5/animation.md`](r5/animation.md) — skeletal + motion matching + learned MM + IK + cloth/hair + facial (635 L)
- [`r5/job_systems.md`](r5/job_systems.md) — threads/fibers/coroutines/ECS sched/SIMD/std::execution (548 L)
- [`r5/memory_allocators.md`](r5/memory_allocators.md) — arenas/pools/slab/handles/streaming/large pages (675 L)
- [`r5/editor_architecture.md`](r5/editor_architecture.md) — PIE vs export, serialization, undo/redo, hot reload, Inspector (572 L)
- [`r5/dcc_asset_pipeline.md`](r5/dcc_asset_pipeline.md) — FBX/glTF/USD, Blender/Maya/Houdini/Substance/Megascans, MaterialX, DDC (793 L)

---

## Decisión-stack consolidado R5 para ALZE v1 (solo-dev, hoy)

Cada subsistema con recomendación concreta + LOC estimado + alternativas descartadas + razón:

| Subsistema | v1 recomendado | LOC | Descartado v1 | Por qué |
|---|---|---|---|---|
| **Audio** | miniaudio + custom DSP graph ~500 LOC + event layer ~500 LOC | ~3k total | Wwise/FMOD | Vendor dependency + licensing friction. Swap posible en 1 sem si interfaces clean |
| **Audio spatial** | Posponer a v2. Panning 2D only en v1. | — | OpenAL Soft HRTF, Steam Audio | No gameplay-critical para ALZE scope actual |
| **Networking** | **Nada**. Single-player first. Arquitectura: fixed timestep + input-as-commands + serializable state. | 0 | Rollback, CS, lockstep | No shippear multiplayer v1. Solo diseñar APIs para no cerrar puerta. |
| **Animation** | Ozz-animation lib + 2-bone IK + spring chain cloth | ~4.2k | Motion matching, XPBD full cloth, learned MM | MM requiere motion database (~500 MB + tagging pipeline ~60 días asset work). v2. |
| **Job system** | N=min(cores,8) std::thread pool + task DAG explicit + xsimd 3-5 hot loops | ~1.5k | Fibers (ND pattern), UE TaskGraph clone | Fibers over-engineered <100k LOC. C++20 coroutines para I/O, no compute. |
| **Memory** | Per-frame arena 32MB + per-type pools + 64-bit Handle<T,Gen> system + zero malloc hot paths | ~2k | Virtual memory tricks, large pages, streaming allocator | Simple + disciplined vence clever. |
| **Editor** | Dear ImGui inside game exe + ImGuizmo + JSON scene + hand-written inspectors + shader hot-reload | ~5k | UE Slate-like, reflection-driven inspector, C++ hot reload (Live++) | Editor = más LOC que renderer si se over-engineers. ImGui + JSON + convenciones cubre 90%. |
| **DCC/Assets** | Blender + glTF 2.0 + cgltf + KTX2 + Substance Painter hero assets | 0 extra (ya integrados) | FBX fighting, USD, custom archive | glTF cubre 95%. USD requiere ~700k LOC dep. |

**TOTAL v1 budget solo-dev ALZE**: ~15-18k LOC across 7 systems. Audio + job + memory + editor = core. Animation + DCC = content pipeline. Networking = 0 (diseñar APIs forward-compatible, no implementar).

---

## Top 12 ideas concretas R5 para ALZE

### 1. Handle-based allocator pattern (Frykholm / Bitsquid) — v1 foundational
`Handle<T,Gen>` = 32-bit index + 32-bit generation counter. Safe dangling detection. Patrón EN TODAS las partes del engine (ECS entities, material refs, texture refs, audio voices, UI widgets). ~500 LOC. **Decision que afecta 10 años.** r5/memory_allocators.md §5.

### 2. Per-frame arena + per-type pools (Frostbite/ND pattern) — v1
32 MB scratch arena per frame, reset at frame end. Per-component-type pools for archetype ECS. Zero malloc in hot paths. Target: <50 allocs/frame (shipping CI rule). ~800 LOC. r5/memory_allocators.md §§2-3.

### 3. Ozz-animation lib port + 2-bone IK + spring chain (Blanc) — v1
~2.5k LOC Ozz + ~800 LOC IK + ~500 LOC spring chain = 4.2k LOC total anim subsystem. Cubre 90% de necesidades. Motion matching v2. r5/animation.md §§1, 6, 9.

### 4. JSON scene serialization + stable IDs + ImGui editor inside game exe — v1
One executable, toggle between "play mode" and "edit mode" via ImGui. JSON scenes (git-friendly). Stable entity IDs across saves. ~5k LOC editor module. r5/editor_architecture.md §§1, 2, 10.

### 5. ImGuizmo for viewport manipulation — v1
Cedric Guillemet's lib integrates cleanly. Translate/rotate/scale gizmos + camera matrix. Zero custom LOC (just link + config). r5/editor_architecture.md §6.

### 6. miniaudio + OpenAL Soft fallback — v1 audio
David Reid's miniaudio single-header MIT. OpenAL Soft for HRTF when spatial becomes gameplay relevant (v2). ~1.5k LOC custom DSP graph + ~1k LOC event layer. r5/audio.md.

### 7. xsimd for 3-5 hot loop SIMD — v1
xsimd abstracts AVX2/NEON/SSE2. Pick 3-5 proven hot loops (culling, particle sim, physics solve, animation blend). Measure first. ~200 LOC per loop. r5/job_systems.md §9.

### 8. Deterministic timestep + input-as-commands — v1 (forward-compat net)
Game logic at 60 Hz fixed; render interpolates. Input = discrete commands in buffer (not continuous sampling). Enables future rollback/CS without architectural rewrite. ~500 LOC discipline. r5/networking.md closing note.

### 9. Blender as canonical DCC — v1
Zero license cost. glTF 2.0 native. 4.x features (Geometry Nodes, EEVEE Next). Adobe Substance Painter only for hero-tier ($240/yr). Megascans free con Epic account. r5/dcc_asset_pipeline.md §§4, 11.

### 10. C++20 coroutines for async I/O (NOT compute) — v1
Asset loading, texture streaming, network messages. std::coroutine + cppcoro or libunifex. ~300 LOC I/O glue. No fibers, no TBB. r5/job_systems.md §5.

### 11. Learned Motion Matching research (Ubisoft LaForge 2020) — v2
Holden, Kanoun, Perepichka, Popa 2020. ~3k LOC runtime + motion database asset pack. **The single biggest animation quality leap available**. v2 because requires mocap database + tagging pipeline ~60 días asset work. r5/animation.md §4.

### 12. GGPO-style rollback (Tony Cannon) — v2/v3 if multiplayer matters
Canonical rollback library. Input buffer 8-9 frames typical, frame advantage limits, sync test mode. **Only if ALZE goes multiplayer**. Architecture enables (determinism, fixed timestep) but implementation is v2+. r5/networking.md §3.

---

## Anti-patterns R5 (nuevos)

33. **Wwise en v1 solo-dev** — $250k free threshold no aplica a gameplay, aplica a revenue. Licensing friction + vendor dep por 0 ganancia vs miniaudio.
34. **Networking v1** — arquitectura forward-compat sí (timestep + input-as-command); implementación NO. Ship single-player first.
35. **Motion matching sin motion database** — algoritmo es ~3k LOC, base de datos + tagging es ~60 días de asset work. Si no tienes budget asset, no adoptes MM. Ozz + blend trees basta.
36. **Fibers <100k LOC engine** — Naughty Dog fiber system asume 100+ engineers. std::thread + coroutines primero. Fibras solo si profiling muestra wait-from-worker waste concreto.
37. **RTTI + exceptions en allocator paths** — ALZE ya es -fno-rtti -fno-exceptions. Mantener disciplina. alloca OK (no unwind).
38. **C++ hot reload (Live++ / RCC++) en v1** — compilación background + DLL hot-swap = ~2k LOC integration + platform-specific pain. Shader hot-reload suficiente v1.
39. **Custom archive format en v1** — glTF + KTX2 + JSON cubren 95%. Custom format es v2+ decision si alguna razón concreta (compression, anti-tamper).
40. **USD en v1** — ~700k LOC dependency, docs complejas. Solo adoptar v3 si ALZE se vuelve DCC-heavy.
41. **FBX como fuente** — binary format Autodesk legacy, pipeline poco claro. Blender → glTF es el camino. FBX solo para compatibility in-feed.
42. **CPU skinning en v1** — compute skinning es default desde 2015. Vertex shader skinning back-up. CPU skinning solo para physics-blended ragdoll.
43. **Global singletons para subsystems** — audio/render/anim como `Audio::Instance()` es trampa testability + threading. Pasar contextos explícitos.
44. **Serialize via reflection en v1** — RTTR (~30k LOC dep) o refl-cpp o UHT-style codegen = complejidad prematura. Hand-write serializers por componente. ~20 LOC por component, acumula bien.

---

## Cross-system integration rules

Reglas que surgen al considerar R5 junto:

### Threading + memory
- Per-thread arenas (no global arena with mutex). TLS arena reset at job end.
- Handle system allocations from per-type pools, free-list thread-safe con CAS.
- Job system NEVER locks. Dependencies via DAG, not mutexes.

### Animation + physics
- Animation baked pose → physics ragdoll blend via "active ragdoll" (ND pattern). Drivers per joint with PD controllers.
- Foot IK runs AFTER animation pose, BEFORE render. 2-bone solve ~30us/character.
- Cloth simulation (XPBD spring chain) runs on job thread, reads animation matrices via double buffering.

### Audio + animation
- Animation events emit sound events (Wwise/FMOD pattern, o custom event queue).
- Footsteps: animation callback → query terrain material → play variant via AudioEventLayer.
- Facial blendshapes ↔ audio lip-sync: Audio2Face offline pipeline, blend weights baked per clip.

### Editor + serialization
- Stable IDs per entity + per asset. Never raw pointers in scene files. Always `Handle<T>`.
- JSON text files for scenes (git-friendly). Binary .pack for shipping (bake step).
- Undo/redo via Command pattern (memento too expensive for deep graphs).

### Net + determinism
- Game logic on fixed tick (60 Hz). Render interpolates between last 2 ticks.
- Float determinism: `-ffast-math` OFF. SIMD reduction order controlled. Transcendentals from deterministic table.
- Input = sequence of commands (one per tick). Can be replayed for rollback, for demo recording, for crash repro.

### DCC + editor + assets
- Source asset = .blend / .glb / .png in DCC. Baked asset = .gltf+.ktx2+.pack in engine DDC.
- Content-hash DDC: hash(source+importer_version+platform) → derived. Cross-machine + cross-platform reusable.
- Hot reload: editor watches source mtime, re-runs importer, swaps asset at runtime via Handle indirection.

---

## Ranking de densidad R5

1. **memory_allocators.md** — foundational para todo lo demás. Handle system + arenas + pools = infraestructura básica. Aplicable día 1.
2. **job_systems.md** — decisión arquitectónica temprana (fibers vs std::thread) afecta 10 años. Ya la R5 resuelve (std::thread + coroutines).
3. **editor_architecture.md** — editor es más LOC que renderer si se over-engineers. R5 da el camino corto (ImGui + JSON + convenciones).
4. **dcc_asset_pipeline.md** — Blender + glTF + Substance stack es el default moderno. Ahorra decisiones infinitas de format.
5. **animation.md** — Ozz + IK + spring chain es la stack mínima viable. Motion matching es v2 si hay budget asset.
6. **audio.md** — miniaudio + custom DSP es el path. Wwise/FMOD pueden sumarse later sin rewrite.
7. **networking.md** — no shippear v1 pero arquitectura correcta ahora evita rewrite futuro.

---

## Conclusión R5

R1-R4 fueron sobre **rendering tech**. R5 demuestra que **los 6 subsistemas no-rendering juntos son ~15-18k LOC para ALZE v1** — comparable al renderer en scope.

**Insight clave**: muchos engines AAA "son excelentes en rendering pero mediocres en otras áreas". Unity es fuerte en editor (Inspector reflection-driven, package manager) pero débil en memory discipline (GC allocations spam). Frostbite es fuerte en rendering + FrameGraph pero débil en editor (hard to author scenes). ALZE como solo-dev puede diseñar las 7 áreas con coherencia desde día 1 — eso es el ÚNICO ventaja real sobre AAA engines.

**Para máster síntesis**: combinar R1+R2 (landscape + engines) + R3 (rendering SOTA) + R4 (engines missed) + R5 (cross-cutting) en un roadmap v1/v2/v3 definitivo con LOC budgets y cronograma estimado.
