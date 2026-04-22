# FromSoftware Engine ("Dantelion") — 15 Years of Incremental Evolution

> Round 4 research 2026-04-22 para ALZE Engine. FromSoftware es el caso mas singular de este round: no publica GDC talks sobre su engine, no comparte documentacion, no licencia fuera. **Casi todo lo que sabemos viene de reverse-engineering community** (SoulsFormats, DSMapStudio, Zullie the Witch, Lance McDonald, BinderTool). Cuando algo es especulacion vs verificado, lo marco [SPEC]. Fuentes: Souls Modding Wiki, GitHub SoulsFormats, Tim Leonard's DS3 netcode reverse-engineering blog, Digital Foundry, PC Gamer / Kotaku / 80.lv reporting, Wikipedia, Capcom IR-style investor material reinterpretado por prensa. No hay CEDEC/GDC abierto de FromSoft sobre esto — el engine es literalmente silencioso.

---

## Overview

FromSoftware's engine es conocido en la comunidad como **"Dantelion"** (en particular **Dantelion2**, visible en strings del executable desde Armored Core 4 en adelante) aunque no hay un nombre oficial publicado — Miyazaki nunca lo menciona por nombre en entrevistas. Es un motor **C++ propietario** que nace con Armored Core 4 (2006, Xbox 360/PS3, primera generacion HD de FromSoft) y se ha ido evolucionando **sin rewrite full** durante ~20 anos a traves de Demon's Souls (2009) -> Dark Souls 1/2/3 (2011-2016) -> Bloodborne (2015) -> Sekiro (2019) -> Elden Ring (2022) -> AC6 (2023) -> ER:SotE (2024) -> Elden Ring Nightreign (2025) -> Duskbloods + FMC (2026).

**Peculiaridades del pattern FromSoft**:

1. **Una sola arquitectura evolucionada 20 anos.** No hay rewrite: hay layer upon layer. Todavia hay flags de debug viejos y reliquias visibles en el executable que los modders atribuyen a AC4 2006 [SPEC, Souls Modding Wiki].
2. **Nunca licenciado externamente.** A diferencia de RE Engine (que Capcom considero licenciar, pero no lo hizo), Dantelion es 100% interno. Nadie fuera de FromSoft programa contra el.
3. **Nunca presentado en GDC/CEDEC.** Miyazaki habla de filosofia de diseno; nunca de shaders, job systems, streaming. Los programadores senior (Yui Tanimura, Jun Ito) tampoco.
4. **Team size sorprendentemente pequeno.** Elden Ring + Armored Core 6 se hicieron con **~300 devs en total en la company, con peaks de 200-230 por proyecto** (PC Gamer 2023, entrevista con productor Yasunori Ogura). FromSoft total era ~400 empleados en 2023 y **456 en May 2025**. Para contrastar: Ubisoft AC Shadows empleo >3000 en 10 estudios. FromSoft ships mas quality-per-dev que cualquier otro estudio AAA occidental conocido.
5. **Cadencia 2-3 anos por titulo, con compartir masivo de assets entre titulos.** Elden Ring reusa animations de DS3; AC6 reusa particles de ER; Sekiro reusa skeletons base de DS3.

**Table: Dantelion tech stack per title (mejor estimate publico)**

| Title | Year | Engine version | Renderer | Graphics API | Network | Level streaming | Animation |
|---|---|---|---|---|---|---|---|
| Armored Core 4 | 2006 | Dantelion 1 | Forward [SPEC] | DX9 (X360) / libgcm (PS3) | P2P + servers | Arena-based, no stream | Custom + proto-Havok |
| Demon's Souls | 2009 | Dantelion 2 | Semi-forward | libgcm (PS3) | P2P + SCE matchmaker | Hub-world, per-zone load | Havok HKX |
| Dark Souls 1 | 2011 | Dantelion 2+ | Forward-ish | DX9/libgcm | P2P + Bamco server | Open-but-gated, contiguous | Havok HKX + TAE |
| Dark Souls 2 | 2014 | **"Katana"** branch | Forward + partial deferred | DX9/DX11 | P2P + servers | Warp-only (no contiguous) | Modified Havok |
| Bloodborne | 2015 | Dantelion 2 next-gen | Semi-forward, baked GI heavy | GNM (PS4) | P2P + Sony | Per-zone with loading screens | Havok HKX + TAE |
| Dark Souls 3 | 2016 | Dantelion 2 next-gen | **Deferred (first)** | DX11 | P2P + Bamco | Per-zone with elevators | Havok HKX + TAE |
| Sekiro | 2019 | Dantelion + PhyreEngine bits | Deferred PBR | DX11 | Offline | Per-zone, larger zones | Havok HKX + TAE (reused from DS3) |
| Elden Ring | 2022 | Dantelion 3 | Deferred PBR, open-world refit | **DX12 (first!)** | P2P + Bamco + EAC | **Open-world tile streaming + legacy dungeon per-zone** | Havok HKX + TAE |
| Armored Core 6 | 2023 | Dantelion 3 | Deferred PBR | DX12 | P2P | Arena-based | Havok HKX + TAE |
| ER: SotE | 2024 | Dantelion 3.1 | +RT shadows, denser LOD | DX12 | P2P | Stream onto existing tiles | Same |
| Nightreign | 2025 | Dantelion 3.2 | Same as ER | DX12 | **Full netplay-first** | Randomized tiles | Same |

**Quick note**: "Katana" (DS2) is the weird sibling — modding community says file formats are 90% compatible pero behavior around character control, saves, scripting, y map layouts es "fundamentally different," suggesting heavy rewrite en el branch that shipped DS2 (B Team, Shibuya/Tomohiro Shibuya as director with Tanimura). DS3 volvio al main Dantelion branch.

---

## 1. History: King's Field -> Dantelion

Pre-Dantelion era (1994-2005, PS1/PS2/XBOX):

- **King's Field** (1994, PS1) — fully 3D first-person RPG, released just 3 months after Sony's PS1 launch in Japan. The engine was custom (Naotoshi Zin, FromSoft founder, directed). This is the **genetic ancestor** of every FromSoft action-RPG: tank-like controls, stamina, no hand-holding, slow deliberate combat, esoteric lore, hostile atmosphere. Miyazaki explicitly cites King's Field as what he played as a kid and what Demon's Souls is a "spiritual successor" to.
- **King's Field II, III, IV** (1995-2001, PS1/PS2) — same engine lineage, iterative.
- **Shadow Tower** (1998, PS1) + **Shadow Tower Abyss** (2003, PS2) — evolution of King's Field.
- **Armored Core 1 thru Last Raven** (1997-2006, PS1/PS2) — parallel engine for mech combat. Lineage from Chromehounds (2006) eventually becomes Dantelion when Otogi/Ninja Blade 360 hardware transition forces HD-era rewrite.
- **Tenchu** (FromSoft bought IP 2004) — first game developed by Acquire. Later Tenchu titles by K2 or FromSoft. Not same engine lineage [SPEC] — Tenchu Z (2006) and Tenchu Shadow Assassins (2009) likely used early-Dantelion derivative.
- **Otogi 1 & 2** (2002-2003, Xbox) — important because they were Naotoshi Zin's last directed games; HD-era tech that informs Ninja Blade.
- **Ninja Blade** (2009, X360) — internally "Otogi 3" [SPEC]. Likely used a direct Dantelion 1 branch, given timing and Zin's involvement.
- **Chromehounds** (2006, X360) — Sega-published FromSoft mech game. The X360 tech is widely assumed to be the prototype Dantelion 1 that also ran AC4 [SPEC, Souls Modding Wiki].
- **3D Dot Game Heroes** (2010, PS3) — FromSoft **published** in Japan, developed by Silicon Studio — NOT their engine.

Dantelion era (2006-present):

- **2006 Armored Core 4** — Dantelion 1 HD-era premiere. X360/PS3. First game in which modders see the modern BND/TAE/FLVER archive lineage [SPEC].
- **2008 Armored Core: For Answer** — Dantelion 1 refined.
- **2009 Demon's Souls** — Dantelion 2 officially seen in executable strings. PS3 exclusive (Sony Japan Studio co-production). First on-ramp to the Souls DNA. Online invasions. Stamina-based combat. Modern iteration.
- **2011 Dark Souls** — Dantelion 2+. The cultural breakthrough. Interconnected world. Multi-platform (PS3/X360/PC).
- **2012-2013** — Armored Core V / Verdict Day on Dantelion (less polished branch).
- **2014 Dark Souls 2** — "Katana" fork. DS2 is widely considered architecturally divergent within Dantelion — same archives, different runtime. Directed by Tanimura + Shibuya (B Team) without Miyazaki. PS3/X360/PS4/XBO/PC.
- **2015 Bloodborne** — PS4 exclusive, Sony funded. Dantelion 2 next-gen on GNM. Shift from DS's "plate/sword" medieval to Gothic/Lovecraftian hybrid. 30 FPS with infamous frame-pacing issues (25-33 ms oscillation) that Lance McDonald later showed were deliberate vsync quirks.
- **2016 Dark Souls 3** — Dantelion 2 next-gen. First fully-deferred renderer. 60 FPS PS4/X1/PC. Miyazaki returns to direct.
- **2019 Sekiro: Shadows Die Twice** — PS4/X1/PC. PhyreEngine influences [see note below]. Offline-only (no invasions). Removes Souls mechanics (no stamina, grappling hook, parry-centric).
- **2022 Elden Ring** — Dantelion 3. First open world. First DX12. **The commercial peak — 25M+ copies by 2024, 30M+ as of 2026 with SotE**. PS4/PS5/X1/XSX/PC.
- **2023 Armored Core VI: Fires of Rubicon** — Dantelion 3 for mech. Proves engine can do vertical flight combat at 60 FPS.
- **2024 Elden Ring: Shadow of the Erdtree** — Biggest DLC ever ($40 standalone, 15-20 GB). Adds Realm of Shadow ~15 km² extra [SPEC].
- **2025 Elden Ring Nightreign** — Co-op roguelike spinoff. Reuses ER assets on Dantelion 3.2.
- **2026** — Duskbloods (Switch 2 exclusive, announced Apr 2025) + **codename "FMC"** (unannounced, slated 2026 per insider reports) — may be a DS3 remaster or new IP.

Note on PhyreEngine: rumor is that Sekiro partially used **Sony PhyreEngine** libraries for rendering. The Steam/Sekiro community debate has never been resolved; most credible take is that FromSoft has used PhyreEngine modules (collision, lighting utilities) as middleware bolt-ons since Demon's Souls, not as full engine [SPEC]. PhyreEngine is Sony's free cross-platform engine originally for PS3, made available to Sony's first/second-party partners.

---

## 2. Archive Formats + Modding Community (How We Know Anything)

This is the **crown jewel of FromSoft knowledge**: a community has reverse-engineered everything because FromSoft publishes nothing.

### Archive hierarchy

```
GAME.exe + dvdbnd0.bhd + dvdbnd0.bdt   (top level — the "disc")
  |
  |-- BHD4 header (encrypted, file index + offsets)
  +-- BDT4 body (actual data blob)
       |
       +--- chr_c1000.bnd.dcx           (per-character archive, compressed)
             |
             +-- c1000.flver            (mesh — vertices, bones, mat refs)
             +-- c1000.hkx              (Havok skeleton + anims)
             +-- c1000.tae              (Time-based Animation Events — hit frames, sfx cues)
             +-- c1000.anibnd           (ANIBND = HKX animations bundled)
             +-- c1000.hks              (Havok Script = Lua dialect for AI)
             +-- *.tpf                  (textures)
             +-- *.mtd / *.matbin       (material description)
```

**Key formats** (via SoulsFormats FORMATS.md by JKAnderson):

- **BHD + BDT** (Binder Header + Binder Data): split-archive pattern. Header has file list + offsets (encrypted in BHD4 with RSA); data in BDT. Analogous to **a `.zip` split into table-of-contents and content**. DS1 uses BHD3/BDT3; DS3/BB/ER use BHD4/BDT4. Elden Ring's DVDBND is ~50 GB, the one archive to rule all.
- **BND** (Binder): flat archive bundling related files. BND3 / BND4 versioning. Equivalent to tar.
- **DCX** (compression wrapper): wraps any file — zlib, DEFLATE, or **Oodle** (since DS3). Elden Ring switched fully to Oodle which stopped older tools from working until they licensed Oodle.
- **FLVER** (FromSoftware Lightweight Virtual Environment Render... guess? No one really knows acronym) — **mesh format**. Contains vertices, normals, tangents, bone weights, LODs, material refs, "dummy points" (attachment sockets for fx/weapons). FLVER0 (legacy), FLVER2 (current).
- **HKX** — standard Havok format for skeletons, animations, collision meshes. **FromSoft uses Havok Physics & Animation SDK** (licensed from Microsoft since MS bought Havok in 2007... then sold to Intel... now part of Havok as subsidiary). Using Havok is how they get mature skeletal animation + ragdoll for free.
- **TAE** (Time-based Animation Events): per-animation metadata describing **when** things happen during the animation clip — hit-active frames, sfx triggers, camera shake, input-cancel windows, iframe start/end. This is **the secret sauce of souls combat feel**: every weapon has a TAE that encodes 13-iframe roll, weapon hitbox-active windows, recovery frames. The `.tae` authoring tool is Meowmaritus's **DSAnimStudio**.
- **MSB** (Map Studio Binary) — level layout file. Spawn points, collision volumes, trigger zones, navmesh regions, enemy placements. Opens in DSMapStudio.
- **PARAM** files — hundreds of tuning tables (weapons, armor, spells, ai behaviors, animation blends). Analogous to Unity's ScriptableObject mass. PARAMs are the **design diff** between DS1 and DS3: same engine, massively different param tables.
- **ESD** (Event Script Data) — state machines for NPC dialog / shop / boss phases.
- **EMEVD** — global event scripts (boss room enter, cutscene trigger, world flag toggle). Lua-adjacent bytecode.
- **FMG** (FromSoft Message) — localized strings per region.
- **HKS** — Havok Script = sandboxed Lua 5.0 dialect used for **AI** per-character decision logic.

### Community tooling (essential references)

- **SoulsFormats** — JKAnderson + Meowmaritus. .NET library reading/writing all above formats. De-facto reference implementation. https://github.com/JKAnderson/SoulsFormats + https://github.com/Meowmaritus/SoulsFormats.
- **DSMapStudio** — katalash (now soulsmods org). Standalone Vulkan 1.3 editor for maps, params, text. https://github.com/soulsmods/DSMapStudio. Successor to DSTools (Unity-based).
- **Yabber** — JKAnderson. Unpack/repack tool for DCX/BND. https://github.com/JKAnderson/Yabber.
- **BinderTool** — Atvaark. Alternative unpacker. https://github.com/Atvaark/BinderTool.
- **DSAnimStudio** — Meowmaritus. Direct3D TAE editor — this is *the* tool for hit/hurt-box modding. https://github.com/Meowmaritus/DSAnimStudio.
- **FBX2FLVER** — Meowmaritus. Imports DCC meshes into DS format. https://github.com/Meowmaritus/FBX2FLVER.
- **FLVER_Editor** — asasasasasbc. Standalone mesh editor GUI. https://github.com/asasasasasbc/FLVER_Editor.
- **Soulstruct for Blender** — Grimrukh. Blender plugins for FLVER/MSB. https://github.com/Grimrukh/soulstruct-blender.
- **Dark Souls Souls Modding Wiki** — http://soulsmodding.wikidot.com/ + https://www.soulsmodding.com — community knowledge base for every format.
- **Mod Engine / Mod Engine 2** — soulsmods. Load-order + file-override system so mods don't touch base game files.
- **Seamless Co-op** — LukeYui / TechieW. Nexus mods #510 (DSR) + ER mod. Bypasses FromSoft matchmaking to allow persistent same-world coop. Reverse-engineered Bamco session protocol.

### Personalities who reverse-engineer the engine

- **Lance McDonald** (@manfightdragon, YouTube "Warp Chair"). Specializes in cut content and unreleased builds. Bloodborne 60 FPS patch (he reverse-engineered DS3 60-mode on PS4 and **ported it binary-level to Bloodborne**, fixing cloth physics sample rate, particle sim rate, enemy patrol pathing, motion blur samples, and elevator speed manually because they were all hardcoded to 30 Hz). Also recovered Bloodborne's cut "Chalice of Hexing" content, deleted NPCs, boss rush mode.
- **Zullie the Witch** (@ZullieTheWitch, YouTube). Model/animation-centric. Explores character meshes from impossible angles, finds cut content, explains animation sharing (e.g., Rennala in ER uses a boss animation package originally cut from DS3). Started with Cheat Engine in 2014 after DS2 launch.
- **Meowmaritus** — format pioneer, DSAnimStudio author.
- **Sekiro Dubi / B3LYP** — animation hackers.
- **Tim Leonard** (timleonard.uk) — reverse-engineered DS3 networking protocol from packet captures, explained it in a multi-part blog "Reverse Engineering Dark Souls 3 Networking" (2022). This is the single most authoritative technical reference for how FromSoft P2P works — not a FromSoft employee, just a passionate engineer.
- **illusorywall** (Tumblr) — Demon's Souls + BB research.
- **AinTunez**, **TKGP**, **katalash**, **LukeYui** — tool authors.
- **Grimrukh** — Soulstruct author.

---

## 3. Level Design + Map System

### Dark Souls 1: Interconnected Lordran

The DS1 world (Lordran) is famously **one contiguous 3D space**. You descend from High Wall of Lothric to Undead Parish to Undead Burg to Depths to Blighttown to Quelaag to Demon Ruins — then discover Blighttown -> Valley of Drakes -> New Londo -> Firelink shortcut ladder and realize you've been threading a topologically coherent world. Macro-scale loops include:

- Firelink Shrine <-> Undead Burg via Hollow ladder-church shortcut
- Firelink <-> Darkroot Garden via Havel elevator
- Firelink <-> Blighttown via New Londo waterway
- Sens Fortress one-way elevator to Anor Londo (the top-of-map moment)

These are **physical geometry loops, not skyboxes**. The map is assembled as spatial tiles with real collision hand-off, and shortcuts are designed so you unlock them once you've earned them. Miyazaki in the Design Works interview: "I like for people to discover the world themselves." The level designer team had to iterate collision/streaming coherence intensely — the design is **expensive labor**, which is why DS2/DS3/BB/Sekiro all moved back to zones with warp-only travel.

### Streaming model (per-zone)

Dantelion uses **per-zone MSB files**: one MSB per "map area" (Undead Parish, Blighttown, Sen's Fortress, etc). When a player crosses a trigger volume, the engine:

1. Load neighboring MSB async.
2. Swap collision meshes (HKX) in Havok world.
3. Hot-load character archives (BNDs) for enemies in that zone.
4. Apply fog gate (a literal visible fog wall door) if transition requires unload of previous zone.

Fog gates are both a **diegetic gameplay device** (boss rooms, zone commits) and a **pragmatic streaming trick**: they hide the swap by making the previous zone go black/fogged behind you. DS1's cleverness is that it avoided fog gates where possible by engineering the physical loops.

### DS2 regression

DS2 explicitly dropped contiguous streaming — you warp between zones always. Famous bug: you could take an elevator up in Earthen Peak and arrive at Iron Keep **which is sky-located but geographically should be below**. Because the engine doesn't care about world-space coherence post-loading-screen, they got away with the gaffe.

### Bloodborne, DS3, Sekiro

Bloodborne (Yharnam / Hunter's Dream / Nightmare Frontier / etc) partially re-embraces DS1-style interconnection at Yharnam + Old Yharnam + Cathedral Ward, with loading screens only for dream worlds. DS3 is smaller zones with elevators. Sekiro is bigger zones than DS3 with grappling hook traversal.

### Elden Ring: open world + legacy dungeons

Elden Ring is Dantelion 3's masterstroke: **open-world tile streaming for overworld + per-zone MSB for legacy dungeons**. Overworld map is partitioned into a grid (likely 1 km x 1 km tiles [SPEC, no confirmed number]). As player moves, tiles load/unload based on distance + camera-heading hint. Legacy dungeons (Stormveil, Raya Lucaria, Volcano Manor, Leyndell, Farum Azula, etc) are self-contained MSBs that you enter through a transition — effectively "old-school Souls zone" accessible from open world.

**Map size numbers**:

- Full map bounding box in the disc: **~79 km²** (this is the widely-cited reddit calculation by user Lusty-Batch using the length of their horse and a reference bridge — contested but order-of-magnitude accurate).
- Playable navigable area: **~13.5 km²** per more rigorous calculation by YouTuber Addypalooza. The rest is water + impassable terrain.
- With SotE (Realm of Shadow): combined playable ~20.5 km².

Compare:

- Skyrim: ~37 km² map box / ~14 km² playable.
- Witcher 3 base + Blood & Wine: ~136 km² box.
- Horizon Zero Dawn: ~67 km² box.
- RDR2: ~75 km² box / ~50% larger than ER playable.
- BotW: ~80 km² map box.

ER sits in the middle of the AAA open-world pack by *playable* area but is substantially **denser than BotW** with more hand-authored encounters.

---

## 4. Physics + Hit Detection

### Havok as foundation

FromSoft uses **Havok Physics & Animation** since at least Demon's Souls. This gives them:

- Skeletal animation blending, ragdoll, inverse kinematics (partially — FromSoft does simple foot IK for stairs).
- Collision meshes authored as `.hkx` — per-zone meshes with material indices driving sound fx (stone vs wood vs metal footsteps) and "killplane" markers.
- Ragdoll death physics (bodies fly down stairs, dragons flopping when they land).

Havok since 2015 has been free to use (Microsoft bought it 2007 then released SDK; then Intel acquired Havok 2016 and kept it free-for-games tier). This is why so many AAA Japanese studios use it — FromSoft, Capcom, Konami (MGSV), Square Enix.

### Weapon hit detection

The core model for Souls combat:

- **Weapon has no collision volume in neutral.**
- When attack animation begins, TAE file marks **active frame range** — during these frames, the engine performs a **swept box/capsule intersection** from the weapon's "dummy points" (sockets at grip, mid-blade, tip) against enemy hurtboxes.
- Character hurtboxes are simple capsules / OBBs attached to bones. Head capsule is separate for headshot bow/crossbow multiplier.
- Once active frames end, recovery frames → no hit registration.

**iframes per game (at native FPS)**:

| Game | FPS | Normal roll iframes | Fast roll iframes |
|---|---|---|---|
| DS1 (Prepare to Die) | 30 | 11 | 13 (Dark Wood Grain Ring, 2 extra via instability-frame removal) |
| DS2 | 30 | 13 | 15-16 w/ Agility ADP |
| DS3 | 30 (at launch, 60 on PS4 Pro) | 13 | 15 at high ADP equivalents |
| Bloodborne | 30 | varies by dodge type | Quickstep ~11 |
| Sekiro | 60 | Deflect ~2-5, no roll | - |
| Elden Ring | 30-60 | 13 base | +2 at max light load |

Interesting detail: **iframes are measured in frames at author time** but then upscaled or downscaled depending on render Hz. When Lance McDonald 60-FPS'd Bloodborne he had to realize cloth/particle/elevators were driven by fixed-timestep loops tied to 30 — patching to 60 broke them. This is a classic **legacy code baked-in-framerate** pattern. DS2 was similarly notorious for 60-FPS mode breaking physics (ladders, bonfire animations, weapon durability all tied to FPS).

### Poise system

Each character has a **poise stat** (int). Each weapon attack has a **poise damage** value. On successful hit:

```
defender.poise_current -= attacker.attack.poise_damage * (1 + defender_state_multiplier)
if defender.poise_current < 0:
    defender.state = STAGGER
    defender.poise_current = defender.poise_max
```

Poise regenerates to max after a timeout (typically ~2 sec of not being hit). Heavy armor + great-shields = high poise = hyper-armor through light attacks. Light armor = easy stagger.

Poise mechanics have varied massively per game — DS1 poise was very generous, DS3 almost removed it except for hyper-armor during weapon attacks, ER brought it back with clear stats. This is entirely a tuning-table (PARAM) problem, not an engine change — the engine subsystem is the same; balance iterates.

### Ladders, ropes, dragons

Custom gameplay physics on top of Havok:

- **Ladders** — tunneled trigger volume + scripted animation on mount/dismount. Character enters an animation state machine (per-frame animation with input gates). DS2's famous "instant ladder drop" is a state-machine quirk.
- **Ropes (Sekiro grapple)** — custom SOP [SPEC] for raycast-from-player to grapple-point markers. When target is valid, transitions to a scripted animation flying the player on a spline to the point. No rope dynamics; a kinematic point-to-point pull.
- **Elden Ring horseback (Torrent)** — custom mount state with **double-jump** that's famously handled by having the second jump play a specific particle + reset a jump counter in AI state, not a Havok-level physics trick.
- **Dragons landing** — bespoke scripted cinematic that triggers upon zone trigger volume + plays HKX ragdoll settle on landing (visible in Stormveil dragon, Fortissax, Placidusax).

---

## 5. Multiplayer / Invasions / Asynchronous Features

### P2P architecture

**All multiplayer in FromSoft games is pure peer-to-peer**. The Bandai Namco / Sony-hosted servers only handle:

1. **Matchmaking** (soul level, weapon matchmaking, password matchmaking, region hints).
2. **Message sync** — player-placed messages and bloodstains distribute via the server with server-side rate limits and rating counters.
3. **Session initiation** — once the server matches players, it hands them each other's NAT-pinhole info and they connect P2P.

Tim Leonard's reverse-engineering (2022 blog series) found:

- Protocol: **reliable UDP on top of SteamNetworkingSockets** (since DS3 Steam release; on consoles it's PSN/Xbox Live sockets).
- DS3: matchmaker server over HTTPS w/ Amazon-hosted endpoints (at Bandai Namco's infra). **Some endpoints are plaintext RSA-signed** which allowed third-party "Dark Souls Archive" servers to keep DS1/DS2 multiplayer alive after Bamco shut them down [SPEC — widely reported in modding channels].
- P2P message format: opcode + payload, delivered in-order via action-buffering.

**The ~15-23 sec latency of summon signs** noted in community guides is because the sign metadata hops through the P2P mesh (player A writes, server replicates, player B polls, player B connects to A) and each step has rate-limiting + exponential backoff.

### Asynchronous "multiplayer" (the defining feature)

Souls-series famously has three async features that are the signature innovation:

1. **Orange/white soapstone messages** — players write hint messages ("try jumping"; "praise the sun"). Messages are uploaded to Bamco backend, rated by other players. Other players download a curated set per zone. Each message has geohash-like coordinates in world-space + message text + rating count.
2. **Bloodstains** — when a player dies, their last 5 seconds of animation is compressed and uploaded. Other players see a bloodstain marker at that location; touching it plays the replay (client-side animation replay from the compressed state stream).
3. **Phantoms** — as player A plays, they occasionally see the ghost of player B doing whatever they're doing in a parallel session. Server streams low-frequency position+animation snapshots from random nearby sessions.

Consistency model: **eventually-consistent, tolerant of mutual desync**. There is **no lockstep, no rollback, no authoritative state**. Each peer runs its own simulation; P2P messages like "I attacked you at X, Y, Z with weapon ID W" arrive, the other peer applies them best-effort. Hit validation is **client-side for attacker** (your game decides if you hit them). This is why PvP has infamous "backstab fishing" and "latency armor" — if your animation says you attacked, the server-of-your-peer trusts you. Cheaters can exploit this.

Sekiro was **offline-only**, intentionally. Miyazaki said the deliberate single-player design let them focus on deflect/parry timing without having to network it.

### ER Nightreign (2025) — new tack

Nightreign (2025) was the first FromSoft co-op-forward title where multiplayer is the primary mode. Digital Foundry's review was critical — they noted FromSoft's netcode is still structurally unchanged from DS3, and in a 3-player roguelike you feel desync far more than in a 1v1 invasion. Nightreign added **session-aware matchmaking** (longer vetting of peer connection quality before session starts) but kept P2P [SPEC].

---

## 6. Animation Pipeline

### The TAE format as gameplay soul

The **TAE (Time-based Animation Events)** file is where game-feel lives. For each animation (swing, roll, block-break, iframe window, backstab, parry window), the animator — working with a gameplay designer — places event markers on the timeline:

```
frame 3:  trigger sfx "sword_whoosh"
frame 4:  active_hit_start dummy_point=blade_mid
frame 8:  active_hit_end
frame 9:  iframe_end (if this is a roll)
frame 14: cancel_window_start (input buffer accepts next input)
frame 20: recovery_end
```

**DSAnimStudio** by Meowmaritus is the de-facto tool — basically Maya-meets-hit-detection-visualization, letting modders see hitbox frames overlaid on animation previewing.

### Reuse across titles

FromSoft is notorious for **reusing animations across games**:

- Gwyn (DS1) -> Lord Soul phantoms in DS2 -> Lothric knights (DS3) -> Crucible Knight base skeleton (ER)
- Dragon landing animation: near-identical in DS1 Hellkite, DS3 Dragonslayer Armour phase 2, ER Placidusax.
- Rolling iframe keyframes identical DS1 -> ER.
- NPC stagger and get-up same across DS3/BB/Sekiro/ER.

Zullie the Witch has an entire sub-genre of videos showing **cut animations** hidden in the PARAM/TAE files of later games — the engine includes animations that were never triggered because the designer cut the encounter. E.g., Rennala's boss fight in ER appears to reuse a DS3-era animation package for an unused witch boss.

This is **the single biggest productivity secret of FromSoft**: they don't re-mocap or re-rig between games. Skeleton is shared, animations are shared, the same human body plan (`c0000.hkx`, "man", the base player rig) has existed since DS1. Each new game **extends** the animation library rather than replacing it.

### Ashes of War / Weapon Arts (ER / Sekiro / DS3)

Weapon Arts / Ashes of War are animation-slots attached to weapons that are **modular**: the weapon has a socket, the art asset is a self-contained animation+particle+input-handler, and runtime binds them. This lets ER have 100+ ashes of war attachable to 300+ weapons = combinatorial variety without combinatorial animation authoring. The actual sharing is at the animation-event-script level (TAE) — same stab animation drives 20 different spears because the weapon just provides a different mesh, hitbox dummy-points, and damage params.

---

## 7. PBR Adoption Timeline

**Demon's Souls (2009)** — pre-PBR. Forward lighting with cubemap reflections, baked AO on statics. The "fog, darkness, lantern" aesthetic largely hides the lack of sophisticated specular modeling.

**Dark Souls 1 (2011)** — same as DaS. Forward + baked ambient. Famous for muted palette + heavy fog; fans will say this **is** the art direction, but pragmatically it was also what the X360 DX9 pipeline could afford. Blighttown's ~15 FPS on X360 is legendary.

**Dark Souls 2 (2014, SotFS 2015)** — B team Katana branch. Semi-forward. The pre-release trailers showed pre-PBR **dynamic lighting** (torch carrying illuminated rooms). Retail release famously downgraded the lighting; comparison between E3 2013 demo and retail is infamous in resurrection threads. The **DS2 Lighting Engine Mod** (DS2LightingMod on Nexus) retrofits PBR-ish look onto SotFS.

**Bloodborne (2015)** — semi-forward with heavy **baked global illumination** + dynamic lights. The "oil painting" aesthetic is art direction plus:
- Heavy use of **baked lightmaps** on static environment (like CryEngine/Source era).
- **Filmic tonemapping** with teal-crushed shadows and amber highlights.
- **Sharp but low-res** textures (PS4 streaming budget tight) looked painted when held at 30 FPS and 1080p.
- Heavy **TAA + per-object motion blur** unified the image.

Bloodborne is often described as "not PBR in engine but PBR-looking in art direction." Modern remaster efforts (like the Bloodborne PC Remaster mod v0.99) have been removing baked lighting to allow dynamic PBR — and the community is split, because the baked look is also what makes BB feel BB.

**Dark Souls 3 (2016)** — **full deferred renderer**, G-buffer with normal + albedo + roughness + metallic. This is the PBR baseline. Still tonemapped toward teal/amber to match DS1 nostalgia.

**Sekiro (2019)** — deferred PBR refined. Cleaner palette (Sengoku Japan greenery + blue sky allowed naturalistic daytime lighting, a first for FromSoft since Chromehounds). Snow scenes in Ashina Depths showed SSS-ish snow and real volumetric fog.

**Elden Ring (2022)** — **DX12, modern deferred PBR** with:
- Per-object motion vectors for TAA.
- Volumetric fog / god rays.
- Screen-space reflections (no SSR for Leyndell's golden city — the shimmering was faked with specular cubemaps).
- **Optional RT shadows** (added via patch, criticized by Digital Foundry as underwhelming for the cost — John Linneman: "better for screenshots than gameplay").
- Shader compile stutter issues at launch — Elden Ring was the **first DX12 FromSoft title**, and they didn't pre-compile PSO shader cache on first boot, so players encountered 50-500 ms hitches when new enemies/effects streamed in. Partially fixed with patch 1.08 which does deferred warm-up during bonfire rest.

**ER: SotE (2024)** — iterative on ER renderer. Some dense foliage LOD work. Digital Foundry ran the same analysis: "FromSoftware has gotten substantial money and acclaim from these games but has not meaningfully improved their technical abilities since Bloodborne released on PS4 in 2015." That's a harsh but not entirely unfair read — **FromSoft spends their dev budget on content/combat/world, not on the renderer's silicon ceiling**.

**AC6 (2023)** — different art direction (sci-fi mech, neon/metallic). Same Dantelion 3 deferred PBR. Showed engine can do **60 FPS with heavy particles** in arenas because the level size is arena-bound, not open-world.

---

## 8. Open World in Elden Ring — Technical Reality

### The world as tiles

Best inference from modding (DSMapStudio editor views) is that ER's overworld is a grid of MSB tiles. Each tile contains:

- Static geometry references (reused asset meshes from a shared asset pool — tree variations, rock variations, ruin modules).
- Trigger volumes (encounter spawns, cutscene entries, boss-arena fog gates).
- Navmesh (built per-tile).
- Baked GI data.
- Sound region data.

The shared asset pool is the **budget trick**. ER has thousands of instances of ~100 tree prototypes, ~50 rock prototypes, ~30 ruin prototypes. Each with 3-4 LODs. When you look at Limgrave and Altus Plateau you see the same ruin arches with different decoration / rotation. This is how a 300-dev team fills 13 km² playable + hundreds of points of interest.

### LOD and streaming

- **Hard pop-in** is noticeable in ER — trees and grass appear mid-distance as you ride Torrent. FromSoft explicitly chose aggressive LOD because the PS4 base was a target platform (2013 hardware, 5400 RPM HDD). The 30 FPS PS4 mode streams from HDD and has been observed dropping into 20s in Caelid swamps.
- **Foliage** is instanced point-cloud with wind animation in vertex shader. Grass density varies per tile.
- **Weather** is mostly fog density + directional light angle + skybox swap. No full Gaia-level atmospheric scattering. ER's rain is a billboard particle overlay.

### Comparison to BotW / Horizon / TotK

- **BotW** (2017, Wii U / Switch) — 80 km² with physics-everything (cooking, fire propagation, chemistry). Nintendo's bespoke engine. BotW achievement is sim-density; ER achievement is encounter-density. Different games.
- **Horizon Zero Dawn** (2017, Decima) — 67 km² box. Decima's foliage + volumetric-cloud tech more technically advanced than FromSoft's. HZD has fewer hand-authored encounters per km² but denser terrain detail.
- **Horizon Forbidden West** (2022) — contemporary with ER. Strictly better from a graphics-tech standpoint (SSGI, RT, virtualized geo hints via Decima). But ER outsold it ~5x because of design.

The open-world tech tradeoff FromSoft made: **less graphically ambitious, more hand-authored encounters, more reuse of art, ship on time**. They ship a world that is less beautiful than HFW or Avatar: Frontiers of Pandora, but with more memorable discrete gameplay moments (catacombs, mini-bosses, hero encounters) per square km. From a business standpoint it worked spectacularly.

---

## 9. Tools + Licensing

### Never licensed out

Dantelion is **not available to license**. FromSoft has never sold or co-developed it with another studio. The only "close" collaboration is **Sony Japan Studio** on Demon's Souls (2009, but Japan Studio is dissolved since 2021) and **Bluepoint** on the Demon's Souls Remake (2020, PS5 launch title — but Bluepoint **rebuilt the game entirely in their own engine**, not in Dantelion; they used original FromSoft assets as reference).

### Internal tool maturity

FromSoft's internal tools are **not talked about publicly**. What we can infer from modding:

- Level editor = authors MSB files. DSMapStudio's UI probably mirrors internal tool conceptually because MSB format = FromSoft tool output.
- Animation tool = authors TAE + HKX. Meowmaritus's DSAnimStudio again mirrors internal conceptually.
- PARAM editor = tuning table editor. Very spreadsheet-y.
- Script editor = EMEVD / ESD authoring. Community tools are primitive; FromSoft internal is probably node-graph or proprietary script.

There is **no FromSoft equivalent to Unreal Editor or RE Engine's slick toolset**. Their internal tools are believed to be utilitarian and programmer-authored [SPEC]. The reason the games feel polished is not tool polish; it's iteration time spent on content, not on making the tooling beautiful.

### Did Tenchu / Chromehounds / 3DDotGameHeroes / Ninja Blade use Dantelion?

- **Chromehounds (2006)** — yes, likely the prototype Dantelion 1 [SPEC — Souls Modding Wiki inferred from shared file formats visible in dumps].
- **Tenchu Z (2006) / Tenchu Shadow Assassins (2009)** — possibly Dantelion 1 derivative but less certain; K2 did the later Tenchu work externally.
- **3D Dot Game Heroes (2010)** — NO. Developed by Silicon Studio with their own tech. FromSoft published only.
- **Ninja Blade (2009)** — likely Dantelion 1-era internal engine [SPEC], given Naotoshi Zin's involvement and Otogi-3 internal name.

### Havok and Bink

FromSoft's external dependencies:

- **Havok Physics + Animation** — visible in the `.hkx` file extension. Used for skeletons, ragdoll, collision.
- **Bink Video** (RAD Game Tools / Epic) — video playback (cutscenes, menus). The `.bik` files are in the archive.
- **Oodle** (RAD Game Tools / Epic) — data compression since DS3. Superior to zlib for game data.
- **Wwise** (Audiokinetic) — audio middleware since Demon's Souls [SPEC — visible in asset file naming patterns].
- **EAC (Easy Anti-Cheat)** — PC Elden Ring. Controversial because it blocks legitimate mods (seamless co-op, etc).
- **Scaleform (Autodesk, since retired)** — UI for older titles. DS3 / BB era. Modern games likely moved to custom UI.
- **DirectX / OpenGL / GNM / NVN** — per-platform.

The **consistent pattern**: FromSoft uses middleware for well-solved problems (physics, audio, compression, video) and keeps renderer + gameplay + netcode proprietary.

---

## 10. Quality from a Small-ish Team

The most important lesson of this document for ALZE:

**FromSoftware ships more quality-per-dollar than any other AAA studio.**

Numbers (per PC Gamer 2023 interview with producer Yasunori Ogura + ResetEra 2025 updates):

- Total company size: **~300 devs / ~400 employees (2023), 456 employees (May 2025)**.
- Peak team on ER: **~200-230 devs**.
- Peak team on AC6: **~200-230 devs** (overlapping with ER Shadow of the Erdtree).
- Dev cadence: 2-3 years per major title, with peak overlap between projects.

Compare:

- **Ubisoft AC Shadows (2025)** — 10 internal studios, ~3000+ devs credited.
- **CD Projekt Cyberpunk 2077** — ~500 devs internal + many external.
- **Rockstar GTA VI** — 2000+ internal.
- **Naughty Dog TLOU Part II** — ~300 devs (this is the only Western studio at FromSoft scale).
- **Sony Japan Studio when it existed** — ~200.

FromSoft is in the upper tens of percentiles of headcount efficiency. **Elden Ring gross revenue ~$1.4 B / ~300 devs = ~$4.7M per dev.** Ubisoft AC Valhalla grossed ~$1 B / ~3000 devs = $333 K per dev — FromSoft is ~14x more revenue-efficient per dev.

**How do they do it?**

1. **Engine reuse over 15+ years.** Dantelion ships 10+ titles with iterative improvement. No full rewrite. New features land as additions, never as "we're retiring this subsystem and rewriting." The same `c0000.hkx` player rig has been around since Demon's Souls.
2. **Animation asset reuse.** A single rolling animation has shipped in DS1/DS2/DS3/BB/Sekiro/ER (with minor tweaks). That's ~15 years of payoff from one animator-month of work.
3. **Small dev teams force architectural discipline.** When the company has 300 people, there's no room for 15-person "engine platform team" + 100-person "level art pipeline team" separately. Tools are rough-and-ready; designers iterate PARAM tables rather than tool polish.
4. **Ship on 2-3 year cadence with predictable outputs.** Miyazaki keeps making souls-like games. That's a feature not a bug — the audience knows what they're getting. Marketing doesn't need to reset each cycle.
5. **Outsource solved problems** (Havok, Wwise, Bink, Oodle). Don't rebuild physics.
6. **Say no to scope.** DS3 is intentionally narrower than DS1 because DS1's interconnected world was expensive. Bloodborne cut the shield. Sekiro cut invasions. ER cut equip-weight tradeoffs for the stealth parts. Each title scopes features down to what the team can finish.
7. **Accept the renderer is not state-of-the-art.** Digital Foundry can and does criticize FromSoft for not doing Nanite, not doing Lumen, not doing proper RT, shader-stutter on DX12 rollout, frame-pacing. FromSoft **doesn't care**. The engine is good enough; the fans don't buy Elden Ring for its renderer.

---

## ALZE Applicability Table

| FromSoft practice | Inspiring for ALZE? | Why / why not |
|---|---|---|
| One engine evolving 15+ years with no rewrite | **YES** | This is the most important single lesson. ALZE should plan Dantelion-style evolution, not "v2 rewrite" religion. |
| Custom archive format (BND/BHD/DCX) | **NO** | ALZE uses LZ4/zstd on standard archive (tar/zip-like). Custom binary formats are a modding-barrier for adopters; keep it interoperable. |
| TAE (per-animation event track) | **YES** | Animation-driven gameplay events (hitbox-active-frame, sfx cue, cancel-window) is a cleanly separable subsystem. ALZE should author this in the animation asset, not scatter hit-timing in code. |
| FLVER mesh format | **NO** | Use glTF 2.0 for interop + binary internal cache. FLVER's advantage is compactness for disc, irrelevant on modern SSD budgets. |
| Havok for physics + skeletons | **CONDITIONAL** | Havok is free for games tier; ALZE could use Havok. BUT: Jolt Physics (Horizon FW) is MIT-licensed modern C++. Jolt is better fit for ALZE's open-source ethos. |
| Reusing animations across titles | **YES (pattern)** | Even for a single-project engine like ALZE, design the animation pipeline so assets are library-level not project-level. Portable HKX-like format with named events. |
| P2P networking with server-assisted matchmaking | **CONDITIONAL** | For small-scale multiplayer (1-8 players) yes — cheaper to operate, fault-tolerant. For competitive PvP or large-scale, dedicated servers dominate. |
| Async multiplayer (messages, bloodstains) | **YES (innovation)** | Cheap async social features dramatically increase engagement with minimal server cost. Consider for ALZE-games. |
| iframe / poise tuning via data tables | **YES** | Every gameplay-feel tunable should be a PARAM-like data table, hot-reloadable in dev. |
| Interconnected world design a la DS1 | **ASPIRATIONAL** | Requires enormous iteration labor. Small-team inspiration: design a single interconnected map before attempting open world. DS1 shows small-team can nail this; open-world ER-scale requires 300-dev budget. |
| DX12 adoption with shader stutter issues | **NO (cautionary)** | ER's launch stutter showed even big studios screw up PSO pre-compilation. ALZE should pre-compile PSOs at first run with a cache-warmup phase (AAA best practice since 2023). |
| Tools are rough-and-ready internally | **CONDITIONAL** | For a very small team, yes — don't overinvest in tool polish. But tools that are too painful kill iteration speed; find middle ground. |
| Opacity re: tech (no GDC talks) | **NO** | ALZE as an open source-adjacent project should talk publicly about its tech. FromSoft's opacity works because of brand mystique; ALZE won't have that for a decade. |
| Art direction > raw graphics tech | **YES** | FromSoft has bad TAA and aggressive LOD pop, and nobody cares because the art hits. ALZE should bias art-direction investment over chasing Nanite. |
| Team size ~300 for AAA-scale shipping | **INSPIRING** | 300 is huge for a hobbyist ALZE but it's proof that AAA is achievable without Ubisoft-scale orgs. Aim for team that scales to tens not thousands. |

---

## Honest note — the quiet engine that ships

FromSoftware is the most **quietly successful** engine studio of our time. No GDC talks. No licensing business. No public tech roadmap. No Nanite equivalent. Shader compile stutters. Aggressive LOD pop. TAA that Digital Foundry shames quarterly. And yet:

- ~30M copies of Elden Ring + SotE.
- Game of the Year 2022.
- Cultural phenomenon spawning the "soulslike" genre now implemented by studios around the world.
- A ~300-dev team doing what 3000-dev Ubisoft teams cannot.

Their formula is **evolve one engine incrementally across 15+ years of titles, never rewrite**. Pile features on rather than refactor. Keep Havok + Wwise + Bink + Oodle external. Reuse animations to the point of Zullie-the-Witch memes. Ship every 2-3 years. Say no to scope creep. Let art direction carry what the renderer can't.

**This is the most realistic model for a small-dev engine like ALZE.** Not id Tech (which requires John Carmack), not RE Engine (which requires a 200-person R&D org), not Unreal 5 (which is Epic Games' war chest), not Fox Engine (which died with Kojima's exit). Dantelion shows that:

1. You can evolve a mid-tier renderer over 15 years and still be commercially dominant.
2. You can stay on last-gen graphics tech and let gameplay/design carry you.
3. A C++ engine with clean layered subsystems lasts longer than any "clean rewrite."
4. The community will reverse-engineer your formats given enough mystique — and that's a distribution win, not a leak.

**ALZE should plan for 10-15 year evolution, not a 2-year sprint.** The goal is not to build *the* engine. The goal is to build *an* engine that ships its first title, then its second, then its third, with each title improving the underlying tech. When the team reaches game 5 and Dantelion-like layered reuse starts paying off, the leverage becomes enormous. Until then — ship content, iterate PARAMs, keep the renderer good-enough, and don't rewrite.

---

## References

**Primary community reverse-engineering work**:
- [SoulsFormats by JKAnderson + Meowmaritus (.NET library for FromSoft formats)](https://github.com/JKAnderson/SoulsFormats)
- [SoulsFormats FORMATS.md reference](https://github.com/JKAnderson/SoulsFormats/blob/master/FORMATS.md)
- [DSMapStudio by katalash / soulsmods](https://github.com/soulsmods/DSMapStudio)
- [DSAnimStudio by Meowmaritus (TAE editor)](https://github.com/Meowmaritus/DSAnimStudio)
- [FBX2FLVER by Meowmaritus](https://github.com/Meowmaritus/FBX2FLVER)
- [BinderTool by Atvaark](https://github.com/Atvaark/BinderTool)
- [FLVER_Editor by asasasasasbc](https://github.com/asasasasasbc/FLVER_Editor)
- [Soulstruct for Blender by Grimrukh](https://github.com/Grimrukh/soulstruct-blender)
- [Souls Modding Wiki — Engines](http://soulsmodding.wikidot.com/topic:engines)
- [Souls Modding Wiki — File Formats](http://soulsmodding.wikidot.com/game-engine-file-formats)

**Networking reverse-engineering**:
- [Tim Leonard, "Reverse Engineering Dark Souls 3 Networking" (2022)](https://timleonard.uk/2022/05/29/reverse-engineering-dark-souls-3-networking)
- [Hacker News discussion](https://news.ycombinator.com/item?id=31982898)
- [DS3 Connection Info (tremwil, P2P connection viewer)](https://github.com/tremwil/DS3ConnectionInfo)
- [@illusorywall, "Demon's Souls used P2P to host multiplayer events" (Tumblr)](https://www.tumblr.com/illusorywall/113766030199/demons-souls-it-used-p2p-to-host-multiplayer)

**Technical journalism**:
- [Digital Foundry — Elden Ring PC performance analysis](https://www.eurogamer.net/digitalfoundry-2022-elden-rings-pc-performance-simply-isnt-good-enough)
- [Digital Foundry — Shadow of the Erdtree analysis](https://www.neogaf.com/threads/digital-foundry-elden-ring-shadow-of-the-erdtree-ps5-xbox-series-x-s-pc-is-performance-fixed.1672311/)
- [Digital Foundry — Armored Core 6 tech review (the Elden Ring engine returns on PS5/XSX)](https://www.resetera.com/threads/digital-foundry-armored-core-6-df-tech-review-the-elden-ring-engine-returns-on-ps5-xbox-series-x-s.761559/)
- [Digital Foundry via 80.lv — RT in Elden Ring for screenshots not gameplay](https://80.lv/articles/rt-in-elden-ring-better-for-screenshots-than-gameplay-says-digital-foundry)
- [Digital Foundry — Nightreign analysis (Windows Central)](https://www.windowscentral.com/gaming/the-elden-ring-nightreign-digital-foundry-tech-review-brings-bad-news)

**Team size + business**:
- [PC Gamer — FromSoft made Elden Ring + AC6 with 300 devs](https://www.pcgamer.com/fromsoftware-made-elden-ring-and-armored-core-6-with-a-staff-of-just-300-developers/)
- [80.lv — 200 devs at peak for ER / AC6](https://80.lv/articles/it-took-about-200-developers-at-peak-times-to-make-armored-core-6-and-elden-ring)
- [ResetEra — FromSoft 456 employees as of May 2025](https://www.resetera.com/threads/fromsoftware-has-now-over-450-employees.1215009/)

**History + company**:
- [FromSoftware (Wikipedia)](https://en.wikipedia.org/wiki/FromSoftware)
- [Hidetaka Miyazaki (Wikipedia)](https://en.wikipedia.org/wiki/Hidetaka_Miyazaki)
- [King's Field (Wikipedia)](https://en.wikipedia.org/wiki/King%27s_Field)
- [TechSpot — King's Field retrospective, roots of Elden Ring & Dark Souls](https://www.techspot.com/article/2248-the-roots-of-elden-ring-and-dark-souls/)

**World design + game design**:
- [TheGamer — Dark Souls 1 interconnected level design magnum opus](https://www.thegamer.com/dark-souls-1-fromsoftwares-magnum-opus-of-interconnected-level-design/)
- [Medium — World Design lessons from FromSoftware by James Roha](https://medium.com/@Jamesroha/world-design-lessons-from-fromsoftware-78cadc8982df)
- [Dark Souls Design Works Interview (Fextralife wiki)](https://darksouls.wiki.fextralife.com/Dark+Souls+1+-+Design+Works+Interview)
- [Miyazaki on boss design philosophy (Game Developer magazine)](https://www.gamedeveloper.com/design/-i-dark-souls-i-director-miyazaki-offers-his-philosophy-on-boss-design)

**Map size**:
- [PC Gamer — Geographer calculates Lands Between size](https://www.pcgamer.com/games/rpg/elden-ring-geographer-tests-rigorous-calculation-against-weed-fueled-horse-math-to-determine-the-exact-size-of-the-lands-between/)
- [GGRecon — How Big Are The Lands Between](https://www.ggrecon.com/guides/elden-ring-map-size-comparison/)
- [ScreenRant — Skyrim vs Lands Between map comparison](https://screenrant.com/skyrim-map-elden-ring-lands-between-big-size/)

**Cut content + modding personalities**:
- [Lance McDonald YouTube channel (Warp Chair)](https://www.youtube.com/c/WarpChair/)
- [Kotaku — Lance McDonald Bloodborne 60 FPS mod](https://kotaku.com/how-a-modder-got-bloodborne-running-at-60-fps-1843165797)
- [Zullie the Witch YouTube channel](https://www.youtube.com/channel/UC4JL8XJ9dxqfvzTM8L5Sl3w)
- [PCGamesN — Zullie the Witch profile](https://www.pcgamesn.com/dark-souls-remastered/zullie-the-witch-dark-souls-youtube)
- [TheGamer — Zullie the Witch origin story](https://www.thegamer.com/elden-ring-dataminer-zullie-the-witch-origin-story/)

**Hitboxes + frame data**:
- [Rolling (Dark Souls Wiki / Fandom)](https://darksouls.fandom.com/wiki/Rolling)
- [Dark Souls 3 mechanics cheat sheet (gastevens GitHub)](https://github.com/gastevens/dark-souls-3-mechanics-cheat-sheet/blob/master/ds3mechanicscheatsheet.md)
- [Rolling - Dark Souls II Wiki](https://darksouls2.wiki.gg/wiki/Rolling)

**Bloodborne art + rendering**:
- [Bloodborne Wiki — Attention to Detail technical analysis](https://www.bloodborne-wiki.com/2017/02/bloodborne-attention-to-detail.html)
- [Polycount — Bloodborne Workshop Environment Study](https://polycount.com/discussion/156329/bloodborne-workshop-environment-study)
- [GameRant — FromSoftware and the power of good art direction](https://gamerant.com/fromsoftware-good-consistent-art-direction-elden-ring-bloodborne/)
- [Wccftech — Bloodborne PC Remaster Mod restores original art direction](https://wccftech.com/bloodborne-pc-remaster-art-direction/)

**Unannounced projects**:
- [MP1st — FromSoftware unannounced project FMC](https://mp1st.com/news/fromsoftware-i-development-unannounced-project-codenamed-fmc)
- [TweakTown — FMC slated for 2026](https://www.tweaktown.com/news/106484/fromsoftwares-unannounced-project-codenamed-fmc-is-slated-to-launch-in-2026/index.html)
- [GameRant — What to expect from FromSoftware in 2026](https://gamerant.com/elden-ring-fromsoftware-duskbloods-nightreign-secret-game-2026/)
