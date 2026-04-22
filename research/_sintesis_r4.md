# Síntesis round 4 — 7 engines AAA que saltamos en R2

**Fecha:** 2026-04-22
**Input:** 7 agentes paralelos, ~3,328 líneas. 0 fallos bloqueantes. Corrección histórica encontrada: Decima's Nubis clouds (Shadow Fall 2013) **precede** a Fox Engine clouds (MGS V 2015), no al revés como R2 sugería — corregido en R4/decima.md.

Complementa [`_sintesis_round2.md`](_sintesis_round2.md). R2 cubrió 7 engines con estado-de-salvamento (migrando a UE5 o muertos); R4 cubre **7 engines que están prosperando o tienen DNA técnico único**:

- [`r4/decima.md`](r4/decima.md) — Guerrilla Games / Horizon / Death Stranding (475 L)
- [`r4/naughty_dog.md`](r4/naughty_dog.md) — ND engine (TLOU2, UC4, Intergalactic 2026) (396 L)
- [`r4/insomniac.md`](r4/insomniac.md) — Spider-Man 1+2, Miles Morales, R&C Rift Apart, Wolverine (446 L)
- [`r4/santa_monica.md`](r4/santa_monica.md) — GoW 2018 + Ragnarok + Valhalla (397 L)
- [`r4/fromsoftware.md`](r4/fromsoftware.md) — Dantelion engine (Souls, Bloodborne, Sekiro, ER, AC6) (577 L)
- [`r4/id_tech_7_8.md`](r4/id_tech_7_8.md) — id Tech 7 Doom Eternal + id Tech 8 Indy + DTDA (641 L)
- [`r4/anvil_ubisoft.md`](r4/anvil_ubisoft.md) — Scimitar → AnvilNext 2.0 → Anvil Pipeline (396 L)

---

## Tabla cross-R4 (engine × moat único × stealable-for-solo-dev)

| Engine | Team size | Cadencia ship | Moat técnico principal | Stealable por solo-dev | No-stealable |
|---|---|---|---|---|---|
| **Decima** | 130→400+ | 2-4 años | Volumetric clouds (Nubis) + velocity streaming + GPU-driven 5-stage pipeline | Nubis (~4-6 sem) + cone streaming (~2 sem) + checkerboard TAAU (~3-4 sem) | Continent-scale streaming 70+km², cross-studio tooling KP↔GG |
| **Naughty Dog** | 100+ sr engineers | 3-5 años | Fiber job system (ND-classic), animation layering TLOU2, cinematic pipeline | Principles of wait-counter + mutex-free patterns; 2-skeleton character-to-character contact IK | 160-fiber pool real size (no 160k myth), 25 años de accumulated tech, mocap budget |
| **Insomniac** | 300+ | 1-2 años | PS5 SSD exploitation (Rift Apart portals), web-swing physics hero-state FSM | Tile LRU streaming; hero-state FSM pattern; RT reflections with SSR fallback | PS5 hardware I/O (not portable) |
| **Santa Monica** | 200+ | 4-5 años | One-take camera + narrow-genre polish | Spring-arm rig; camera-aware nav modifier; hair cards shader; dither-fade occluders (~2-3k LOC total) | GoW's narrative ambition requires studio |
| **FromSoftware** | 300-400 | 2-3 años | **Engine reuse across 15+ años** — iterative evolution w/ no rewrite | **Pattern**: evolve one engine iterativamente, no rewrite. c0000.hkx player rig reused Demon's→ER | Community reverse-engineered archives (tool ecosystem), FromSoft's magic |
| **id Tech 7/8** | 70-100 engineers | 4-6 años | Vulkan-only 60-1000+ fps, async compute discipline, mesh shader baseline Indy | Frame budget culture, ubershader discipline, async compute patterns (not mesh shaders themselves) | Console port pipeline (Panic Button for Switch), Microsoft ownership |
| **Anvil Ubisoft** | 2000+ cross-studio | 1-2 años | Crowd sim + parkour + **LaForge motion matching research papers** | Learned Motion Matching papers (Holden 2020, Bergamin 2019) are the STEALABLE gift | Engine itself is 2000-dev trap; 4-season world bakes require team budget |

---

## Lecciones meta R4

### 1. FromSoftware es EL modelo para solo-dev
De los 14 engines estudiados (R2+R4 = 14), FromSoftware es el único que:
- Un team <400 devs
- Ship cadence 2-3 años constante
- **15+ años evolving one engine, cero rewrites**
- Reusa animation rigs, archive format, level streaming entre títulos
- Cuality por dollar más alta de la industria

**Para ALZE**: plan 10-15 años de evolución incremental de UN engine. No rewrite at year 5. Esta es la única estrategia viable solo-dev. El pattern opuesto (Luminous, Slipspace, REDengine) todos muertos.

### 2. Ubisoft LaForge research papers = regalo gratis
LaForge publica en SIGGRAPH/TOG con implementaciones clean + math clara:
- Holden, Komura, Saito — "Phase-Functioned Neural Networks" SIGGRAPH 2017
- Holden, Kanoun, Perepichka, Popa — "Learned Motion Matching" SIGGRAPH 2020
- Bergamin — "DReCon: Data-driven Responsive Control of Physics-Based Characters" SIGGRAPH 2019
- Kovar, Gleicher — "Motion Graphs" SIGGRAPH 2002 (foundational)

**Copy the research, not the engine**. Ubisoft investiga para cross-studio, tú implementas solo lo que te aplica.

### 3. Naughty Dog fiber mito corregido
El famoso "160,000 fibers" es misquote. Gyrling GDC 2015 real number = **~160 fibras totales × 64 KB stack cada una = ~10 MB total**. Pool chico, no uno por task. **Para ALZE**: std::thread pool + C++20 coroutines para I/O cubre 95% de casos. Fibras solo si profiling muestra wait-from-worker waste.

### 4. id Tech frame budget discipline es extractable sin copiar API
id ship 60 fps locked en Switch + 1000+ fps RTX 3080. **La disciplina**: cada feature tiene un ms budget, se rechaza si lo excede. ALZE no copia Vulkan async compute patterns concretos; copia la cultura de budget. ~50 ubershader variants max. Zero stutter policy.

### 5. Insomniac + Decima ambos resuelven streaming con distinto trick
- **Insomniac**: SSD velocity directa (PS5 hardware), 22 GB/s peak. No portable fuera PS5.
- **Decima**: predictive cone sobre cualquier storage. Portable across platforms.

**Para ALZE** (PC/cross-platform): Decima approach (cone streaming + async LRU tiles) es el correcto. No depender de NVMe direct.

### 6. Santa Monica es el engine más narrow-scope del grupo
Único genre (action-adventure narrative), único art direction (Norse myth). Por eso es ~100k LOC budget estimado, no 500k+ como UE5. **Lo más parecido al scope realista de ALZE**. Los tricks de GoW (one-take camera, spring-arm rig, dither-fade occluders) son directamente portable.

### 7. Crowd sim (Anvil) tiene LOD tier model útil
AC series usa 4-tier crowd:
1. **Full sim** (<20m): full skeleton + AI brain
2. **Pose LOD** (20-50m): simplified skeleton + lookup anim
3. **Impostor LOD** (50-100m): billboard + sprite anim
4. **Positional marker** (>100m): just a point, culled

Recyclable pool (~200-500 active agents backs "5000 on screen" illusion). Solo-dev: bajar a 2-3 tiers pero **pattern es correcto**.

---

## Top 10 ideas nuevas R4 para ALZE

Complementan R1 (15) + R2 (10) + R3 (12):

### 1. Iterative engine evolution — no rewrite (FromSoft)
Cada título ALZE agrega features al engine, no reemplaza módulos. Archivo format estable, animation rigs reutilizables, level format compatible hacia atrás. **Decision arquitectónica más importante que cualquier técnica gráfica.**

### 2. Spring-arm rig para cámara (Santa Monica)
One-take camera + predictive streaming alineado al camera boom. 2-bone chain camera-to-character. ~500 LOC. Funciona para cualquier cámara third-person.

### 3. Forced-LOD volumes (Santa Monica)
Volúmenes en world que force un LOD level independientemente de distancia. Para cinematic moments + occluded rooms. Evita pop in momentos narrativos. ~200 LOC.

### 4. Camera-aware pathfinding modifier (Santa Monica companion AI)
Atreus stays in camera view, modifies pathfind cost by "distance from camera frustum". Portable pattern. ~300 LOC sobre A*.

### 5. 2-skeleton contact IK (Naughty Dog TLOU2)
Hand-on-waist, ladder, weapon grips — segundo skeleton arrastrado a constraint point del primero. ~1k LOC full-body IK library.

### 6. Hero-state FSM pattern (Insomniac web swing)
No rigid-body simulation. Estado = {web_anchor, state, timer}. Procedural anim layer blends based on velocity + input. Aplicable a cualquier traversal mechanic (rope, grapple, zipline). ~2k LOC.

### 7. Bark system (Spider-Man citizens, RDR2 dialogue)
Event → filtered candidate lines → cooldown per speaker → play. Sin branching dialogue trees para barks. ~500 LOC + data-driven.

### 8. Ubershader + ~50 variants max (id Tech policy)
Single forward+ shader, compile-time branches por feature (lit/unlit/alpha-blend/subsurface/hair). Máximo 50 permutations tracked. PSO precache on load. Zero shader stutter policy.

### 9. LaForge motion matching paper implementation
Holden 2020 "Learned Motion Matching". ~3k LOC runtime + motion database ~500MB (opt-in asset pack). Mejor walk/run/turn quality que cualquier state machine.

### 10. Community tool ecosystem via stable archive format (FromSoft)
Si ALZE quiere mod support eventualmente, publica archive format spec + version bump convention. Don't obfuscate — encourage community tools (equivalent a SoulsFormats / DSMapStudio). Moat = community no engine.

---

## Anti-patterns nuevos R4 (continúan numeración)

24. **Engine rewrite mid-ship** — textbook case Slipspace (R2 already). FromSoft counter-example: iterate never rewrite.
25. **PS5-specific SSD architecture in PC engine** — Insomniac Rift Apart portal tech doesn't port. Use Decima approach (async cone streaming) for PC.
26. **Continent-scale open world without 20-year team** — Rockstar (3000 devs, 20 años) is not replicable. Cap world size to single-biome ~10 km² max solo-dev.
27. **Copying Naughty Dog's fiber system** — 160k myth perpetuates over-engineering. std::thread + coroutines first, fibers only with profiling evidence.
28. **Mesh shaders as prerequisite** — Indy GC 2024 is first baseline-mesh-shader title. Fallback classic VS + indirect draw works 95% cases.
29. **Ignoring frame budget discipline** — id Tech's 60fps lock is cultural, not a toolchain feature. Budget tracking in CI from day 1.
30. **Photogrammetry pipeline for solo-dev without scan budget** — RE Engine 140-DSLR scanner, MetaHuman cloud. Alternative: Megascans free library (R5 dcc_asset_pipeline.md).
31. **One-take camera as policy vs capability** — SSM's GoW 2018 is single-camera because narrative demanded it, engine could cut. Don't hardcode no-cuts.
32. **Mod-support without stable ABI** — FromSoft's SoulsFormats ecosystem grew because archive format was stable across titles. Versioning discipline required.

---

## Ranking de densidad R4 (leverage para solo-dev)

1. **fromsoftware.md** — el modelo mental correcto para solo-dev. Lectura prioritaria.
2. **santa_monica.md** — narrow-scope engine, más portable tricks. 6 implementables concretas.
3. **decima.md** — mejor deep dive técnico del round. Nubis clouds + predictive streaming + checkerboard TAAU.
4. **anvil_ubisoft.md** — menos engine, más research papers. LaForge content es oro.
5. **naughty_dog.md** — lecciones principios + fiber myth debunk. 7 patrones destilados.
6. **insomniac.md** — SSD tech no portable, pero streaming patterns + bark + hero-state sí.
7. **id_tech_7_8.md** — disciplina de budget + async compute patterns. API-specific (Vulkan).

---

## Engines por lesson dominante (extendiendo tabla R2)

| Engine | Lección única |
|---|---|
| **Decima** | Predictive cone streaming es lo único que funciona cross-platform (no PS5-SSD). Nubis clouds = el mejor bang-per-buck atmosférico. |
| **Naughty Dog** | Principios de fiber > copiar fiber system. Animation layering es la bomba. |
| **Insomniac** | SSD exploit es moat PS5-only. Lo portable es LRU tile streaming + hero-state FSM. |
| **Santa Monica** | Narrow scope engine es un modelo válido. One-take camera es policy no capability. |
| **FromSoftware** | **Evolve one engine 15+ años, no rewrite**. ÚNICA estrategia solo-dev. |
| **id Tech 7/8** | Frame budget discipline + ubershader policy. Cultura > API. |
| **Anvil** | LaForge research papers > copying the engine. Read the research, implement what fits. |

---

## Conclusión R4

R2 cubrió engines con estado-de-salvamento (mayoría migrando/muertos). R4 cubre engines prosperando. **Dos lecciones meta**:

1. **Los engines que prosperan tienen cadence 2-3+ años y team <500 devs** (FromSoft, Santa Monica, id Tech small-ish team). Engines con cadence anual y team 2000+ (Anvil, IW) están en treadmill. Engines con rewrite history (REDengine, Slipspace, Luminous) están muertos.

2. **Para solo-dev el modelo realista es FromSoftware + Santa Monica + research papers Ubisoft LaForge**. No replicate UE5/Decima/Naughty Dog at engine level; extract patterns + copy individual research implementations.

R5 siguiente cubre sistemas cross-cutting (audio, net, anim, jobs, memoria, editor, DCC) que son tan importantes como el rendering para un engine real.
