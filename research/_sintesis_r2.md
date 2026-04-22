# Síntesis round 2 — motores gráficos propietarios de AAA

**Fecha:** 2026-04-22
**Input:** 7 agentes paralelos, 1606 líneas, 0 fallos bloqueantes (3 WebFetch 403/socket reemplazados por mirrors).

Complementa [`_sintesis.md`](_sintesis.md) (round 1, con UE5/Unity/Godot/Bevy/AAA histórico/rendering libs/physics). Este file cubre **engines propietarios de AAA actuales** que no cabían en round 1.

- [`rage_rockstar.md`](rage_rockstar.md) — GTA V/VI, RDR2, Euphoria (243 L)
- [`redengine_cdpr.md`](redengine_cdpr.md) — Witcher/Cyberpunk + case study de migración a UE5 (166 L)
- [`re_engine_capcom.md`](re_engine_capcom.md) — RE remakes, DMC5, DD2, MHW, SF6 (200 L)
- [`snowdrop_ubisoft.md`](snowdrop_ubisoft.md) — The Division, Avatar, Star Wars Outlaws (191 L)
- [`fox_engine_kojima.md`](fox_engine_kojima.md) — MGSV, PES, el engine que murió con su equipo (185 L)
- [`iw_slipspace_fps.md`](iw_slipspace_fps.md) — Call of Duty + Halo Infinite (300 L)
- [`glacier_dunia_misc.md`](glacier_dunia_misc.md) — Hitman + Far Cry + Starfield + FFXV Luminous (321 L)

---

## Tabla resumida — 14 engines estudiados en total

| Engine | Familia | Estado 2026 | Games signature | Moat técnico |
|---|---|---|---|---|
| UE5 (round 1) | Epic licenciado | Dominante | Fortnite, Wukong, Stalker 2 | Nanite + Lumen |
| Unity (r1) | Unity licenciado | Grande pero herido | Hollow Knight, Cuphead | DOTS/Burst + ecosistema |
| Godot (r1) | MIT OSS | Creciendo | Dome Keeper, Halls of Torment | Servers + SceneTree |
| Bevy (r1) | MIT Rust | Research-grade | Tiny demos, Tunnet | Archetype ECS + wgpu |
| id Tech (r1) | idSoftware/Bethesda | Activo | Doom Dark Ages | Vulkan pionero |
| Frostbite (r1) | DICE/EA | Activo | Battlefield 2042, FIFA | Frame Graph (O'Donnell) |
| **RAGE** | Rockstar | Activo | GTA V/VI, RDR2 | Streaming continental + Euphoria |
| **REDengine** | CDPR | **Retirándose** | Witcher 3, CP77 | RT Overdrive path tracing |
| **RE Engine** | Capcom | **Activo (100%)** | RE2-4 remake, DMC5, DD2, MHW | Dual-path scripting VM + photogrametry |
| **Snowdrop** | Ubisoft Massive | Activo | The Division, Avatar, Outlaws | Graph-first tooling |
| **Fox Engine** | Konami | **Muerto** | MGSV | Volumetric clouds pionero (inspired Decima) |
| **IW Engine** | Activision | Activo | Call of Duty anual | 60-120 Hz netcode + 150p Warzone |
| **Slipspace** | 343→Halo Studios | **Retirado (2024)** | Halo Infinite (único) | Blam! legacy |
| **Glacier 2** | IO Interactive | Activo | Hitman trilogy, 007 First Light | 1200 NPC crowd sim |
| **Dunia** | Ubisoft Montreal | **Migrando** | Far Cry series | Propagación fuego/fauna grid-based |
| **Creation 2** | Bethesda | Activo (frágil) | Starfield, TES VI | Mod ecosystem (ESM/ESP) |
| **Luminous** | Square Enix | **Muerto (2023)** | FFXV, Forspoken | — (dissolved) |

**Tendencia dominante 2022-2025**: proprietary engines rindiéndose a UE5:
- **REDengine → UE5** (Witcher 4 "Polaris" + Cyberpunk sequel "Orion") — 15 años de partnership con Epic
- **Slipspace → UE5** "Project Foundry" (rebrand a Halo Studios, Oct 2024)
- **Fox Engine → UE5** (eFootball full UE5 target 2027, Metal Gear Delta remake en UE5)
- **Luminous → dissolved** (Square Enix, 28-Feb-2023)
- **Dunia → Snowdrop** (Far Cry 7 "Blackbird" leak, intra-Ubisoft)

Survivors 2025: RAGE (Rockstar), RE Engine (Capcom), Snowdrop (Ubisoft Massive), IW Engine (Activision), Creation 2 (Bethesda, frágil), Glacier 2 (IOI). **Casi todos con >15 años de historia y 5+ títulos shipped antes de la salud actual.**

---

## Top 10 ideas nuevas para ALZE (round 2)

Complementan las del round 1. Ordenadas por leverage real en un small team.

### 1. Dual-path scripting VM — C# 8.0 / .NET con interpreter dev + AOT-to-C++ release (RE Engine REVM)

El patrón más genuinamente innovador de toda la research. Capcom ejecuta C#/.NET sobre una VM propia ("REVM"):
- **Dev build**: interpreter → hot reload + breakpoints + edit-and-continue
- **Release build**: AOT compile el mismo C# a C++ estático, link nativo

Evita la dicotomía UE5 Blueprint-VM-slow-vs-C++-recompile. Ofrece la ergonomía de scripting con zero-cost al ship.

**Para ALZE**: si el scope permite, adoptar **AngelScript + AOT-to-cpp** o **Lua JIT + static-compile-as-option** o incluso **WASM en dev, transpile a C++ en release**. El pattern importa más que el lenguaje concreto.

### 2. Predictive streaming cone (RAGE) — camera velocity como load hint

Rockstar pre-carga assets en un **cono orientado al vector de velocidad de la cámara**. Cuanto más rápido, más lejos pre-carga (RDR2 train = muy lejos; walk = cerca). Combinado con LOD grid jerárquico.

```
hint_frustum(position, direction, velocity):
    distance = clamp(velocity * k_predictive, min_d, max_d)
    cone_half_angle = base_angle + velocity * k_angle
    pre_load_chunks_in(position, direction, distance, cone_half_angle)
```

Small-team implementable. RDR2 demostró que escala hasta 75 km² en 8GB de RAM.

### 3. Graph-to-code compilation (Snowdrop) — no interpreter en release

Snowdrop's big-tech secret: **graphs de material/VFX/animation/AI se compilan a C++ en build time**, NO hay interpreter en shipping binary. Artistas usan node editors, runtime paga zero abstraction.

**Para ALZE**: definir un único **Graph IR** (JSON o binary) con domain node libraries (materials, VFX, AI). Build-time transpiler a C++. El editor vive separado del runtime.

### 4. Grid-based propagation simulation (Dunia) — fire / disease / ecology

Far Cry's fire engine (Clark Tucker's postmortem, Levesque):
- Grid de celdas con `flammability`, `moisture`, `wind_exposure` por celda
- Cada tick: `damage_to_neighbor = source_heat × flammability_neighbor × (1 - moisture) × wind_dot`
- Acoplable a wildlife sim (animales huyen del vector de propagación)

Extiende trivialmente a: water level, disease spread, radiation, enemy AI pathfinding awareness.

### 5. Deterministic NPC schedules (Glacier 2) — 1200 NPCs, 500 on-screen, 30 fps

Mikkel Fauerby GDC 2012. Cada NPC tiene:
- Schedule YAML con 15-min windows + priority interrupts
- Routine base (walk route A → desk → coffee → desk)
- Interrupt table (player_enters_vision → switch to "watch"; alarm → "evacuate")
- Crowd LOD: distant NPCs = positional markers + animation LOD; close NPCs = full sim

**Para ALZE**: si alguna vez tienes open world con crowd density, Glacier es la referencia. Hitman 3's Dartmoor tiene 300+ NPCs en una mansión.

### 6. 4-cubemap weather probe stack (Fox Engine)

Cada zona del mapa tiene **4 cubemaps prebaked**: dry_day, dry_night, wet_day, wet_night. Runtime interpola entre los 4 según weather state + TOD. MGS V's sandstorm + Afghan TOD uso esta tabla perfectamente.

**Para ALZE**: caro en disco pero estable perf. Escala mejor que RT GI para hw mid-tier.

### 7. Content-addressable archive format (RAGE `.rpf`)

Format spec:
- Header + TOC en offset 2048B
- zlib compression sin deflate header (size penalty -2 bytes por archive)
- Mmap'd reads → zero-copy a game state
- Content hash as GUID (dedup implícito)

**Para ALZE**: adoptar **LZ4 (speed) o zstd (ratio)** en lugar de zlib. mmap + section-based access. Un archive format propio = content control + anti-tampering + fast patching (delta patches sobre hashes).

### 8. ESM/ESP mod format (Creation) — plugin ABI estable como feature

Bethesda's killer moat: **cualquiera puede escribir un plugin** que el engine carga al boot. Override de assets, scripts, world data. Load order explícito. Hash-verified.

**Para ALZE**: aunque no sea mod-target, la disciplina de **plugin ABI estable** paga dividendos. Define record types, FormID address space, load-order rules. Mods = extensible renderer techniques + debug overlays + community tools.

### 9. Fixed tick rate decoupled from render (IW + canonical netcode)

Sí, esto es la biblia FPS. Si ALZE alguna vez hace multiplayer:
- **60 Hz o 128 Hz game simulation** en thread propio
- **Render interpolates** entre últimos 2 ticks
- **Client prediction** + server reconciliation + lag compensation
- **Delta snapshots** con bitfield per-field change mask

No negociable para shooter. Gabriel Gambetta + Gaffer on Games = lectura obligada.

### 10. Photogrammetry pipeline as first-class (RE Engine + Fox Engine + MetaHuman)

Pattern cross-engine:
- Hero asset scan con ≥100 DSLRs (Capcom = 140; UE5 MetaHuman = cloud service)
- RAW → cleanup → blend shape library
- In-engine performance capture (mocap suit + facial camera → blend weights)
- Photogrammetry ya NO es AAA-only — Reality Capture + Metashape están al alcance

**Para ALZE**: diseñar el animation system pensando en **blend-shape input from scan**, no solo skeletal. Permite adoptar scan-actor pipeline incrementalmente sin refactor.

---

## Anti-patterns (nuevos del round 2)

Complementan los 15 del round 1:

1. **Engine propietario sin 3+ títulos shipped previos** (CDPR rewrite Witcher 4 en UE5 tras CP77; RE Engine shipped sobre 10 juegos antes de llegar a DD2)
2. **Engine rewrite while shipping** (Slipspace = texto del libro, vs MW2019 que fue rewrite acotado de renderer + tools sobre simulation preservada)
3. **Under-staffed engine team paralelo a game team** (CDPR post-Cyberpunk: el engine team se fue a UE5 porque mantener REDengine consumía presupuesto del game team)
4. **Cross-genre forcing** (Frostbite ruined Mass Effect Andromeda; Creation 2 struggles en Starfield open world tras Skyrim corridor heritage)
5. **Licensing Euphoria** — NaturalMotion comprada por Zynga 2014, closed commercial licensing 2017. Si necesitas procedural animation, hazlo tú con Machine Learning (Motion Matching) o ragdoll+IK blend
6. **Custom scripting sin dual-path** (Papyrus de Creation = slow en Starfield por eso; REVM = rápido por eso mismo)
7. **Continent-scale sim sin 100+ devs** (RAGE = 20 años, 3000 devs across studios; imposible a small team)
8. **P2P netcode para MP (Rockstar)** — Warzone 150p tick 20-24 Hz demuestra que scaling players baja tick; elegir UNA dimensión (players OR tick) a small-team scale
9. **Annual ship cadence** (CoD) — engine debt compounds invisible hasta que hay que pagarla de golpe. Cadencia 2-3 años = refactor windows
10. **Engine dissolution antes de estabilizar** (Luminous Productions, Square Enix Feb 2023) — don't dissolve engine team before engine ships 3 successful games

---

## Engines por lesson dominante (meta-pattern)

Si ALZE tuviera que aprender UNA sola lección por engine:

| Engine | Lección única |
|---|---|
| UE5 | Virtualized geometry + unified GI son la vanguardia, pero costosos |
| Unity | Cambios de licensing pueden matar la trust — no depender de vendor único |
| Godot | MIT + community tooling te da resilience independentemente de fondos |
| Bevy | ECS + data-oriented desde día 1 paga dividendos a los 5 años |
| RAGE | Predictive streaming + archive format maduros = open world scale factible |
| **REDengine** | Proprietary engine **ROI = 5+ shipped titles**. Si no, UE5. |
| **RE Engine** | Dual-path scripting VM es el killer feature del siglo. Copy inmediato. |
| **Snowdrop** | Graphs + build-time compile = tooling de artista sin perf penalty |
| **Fox Engine** | El engine muere con su equipo si no documenta. Succession planning obligatorio. |
| **IW Engine** | 15 años de codebase legacy son ventaja + deuda. Refactor windows obligatorios. |
| **Slipspace** | Rewrite-while-shipping es la pesadilla textbook. Hacer clean-room o no empezar. |
| **Glacier 2** | Deterministic NPC scheduling = replay-ability + multiplayer opcional gratis |
| **Dunia** | Grid-based propagation = gameplay-depth emergent gratis |
| **Creation 2** | Mod ecosystem = moat a 20 años vista (Skyrim sigue vendiendo 2026) |
| **Luminous** | No dissolvas el engine team antes de 3 successful ships. Cautionary. |

---

## Update al stack recomendado para ALZE

Actualizar [`_sintesis.md`](_sintesis.md) con estas capas nuevas:

### Scripting (NEW, from RE Engine)
- **Dev**: AngelScript / Lua / WASM sobre VM propia, hot reload + breakpoints
- **Release**: AOT compile a C++ estático, link nativo. Zero-cost abstractions.
- **Fallback**: si el scope no permite dual-path, al menos tener **AngelScript con JIT** (vs Lua interpretada puro) — AngelScript tiene JIT comunitario decent

### Streaming (NEW, from RAGE)
- **Predictive cone** basado en camera position + direction + velocity
- **LOD grid** jerárquico (chunks 64m + sub-chunks 16m + meshlets)
- **Async IO** desde day 1 — NUNCA blocking loads en game thread

### NPC / AI schedules (NEW, from Glacier + RDR2)
- **YAML-based schedules** con 15-min windows + interrupt priority table
- **Crowd LOD**: full sim (< 20m) → pose LOD (20-50m) → positional marker (50m+)
- **Barking system**: tagged dialogue lines + context filters + cooldown (RDR2 canonical)

### Propagation simulation (NEW, from Dunia)
- **Grid de celdas** con physical properties (flammability, moisture, velocity, etc.)
- **Tick-based propagation** con formula configurable
- **Bind to wildlife** para emergent gameplay

### Mod support (NEW, from Creation)
- **Stable plugin ABI**: record types + FormID address + load order + hash verify
- Prioritario si ALZE apunta a "engine that lasts 20 years"

### Archive format (NEW, from RAGE)
- **Content-addressable** (hash-as-GUID)
- **LZ4 / zstd** (speed / ratio pick por asset class)
- **Mmap reads** + section offsets en TOC
- Delta-patch support para DLC + hotfixes

---

## Conclusión del round 2

Los 7 engines del round 2 son, mayoritariamente, **historias de supervivencia o extinción**:
- 3 migrando a UE5 (REDengine, Slipspace, Fox)
- 1 dissolved (Luminous)
- 1 en decline frágil (Creation 2)
- 3 prosperando (RAGE, RE Engine, Snowdrop)
- 1 migrando intra-publisher (Dunia → Snowdrop)
- 1 annual treadmill (IW Engine)
- 1 genre-focused stable (Glacier 2)

**La lección meta**: el ROI de un engine propietario es brutalmente dependiente de volumen de títulos shipped + team size + genre focus. RE Engine es el outlier feliz (10+ AAA en 8 años por dedicación a genre Capcom-core). REDengine es el outlier infeliz (2 titles masivos pero scope creep).

**Para ALZE**: antes de invertir más en custom tech, la decisión de scope debe estar clara (¿qué 2-3 juegos va a shipping ALZE en los próximos 5 años? ¿del mismo género?). Sin esa claridad, las 10 ideas nuevas que surface este round son optimizations prematuras.

**Próximo research sugerido si hay apetito**:
- Motor audio (FMOD vs Wwise vs miniaudio vs Wwise-free-alternative Sonicle)
- Motor de networking / netcode (Steam Networking Sockets, Photon, Epic OnlineServices, ggPo para rollback)
- Editor / DCC integration (Blender/Maya/3DS Max pipeline, FBX vs glTF vs USD, shader hot reload)
- Tooling: visual debugging (RenderDoc, Tracy profiler), assertion / crash reporting (Sentry, Backtrace)
