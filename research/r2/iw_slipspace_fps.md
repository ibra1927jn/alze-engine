# IW Engine & Slipspace — FPS-specialized engine research

Research date: 2026-04-21. Context: ALZE Engine scouting. Two shooter tech
stacks (CoD's IW Engine, Halo Infinite's Slipspace) — lessons for multiplayer
FPS, where tick rate, hit-registration and netcode dominate far more than
streaming or fidelity.

## Overview

CoD is the biggest annual shipping franchise in gaming: ~3,000 developers
across ~15 studios rotate to ship a title every year. Halo is Microsoft's
flagship shooter IP, but its custodian (343 Industries, rebranded **Halo
Studios** Oct 2024) has struggled publicly since the Bungie split. Halo
Infinite shipped December 2021 after a visible delay, and in late 2024
Microsoft confirmed all future Halo titles on Unreal Engine 5 — effectively
retiring Slipspace.

Both engines are "FPS first": tick rate, hit registration, server-
authoritative simulation and lag compensation are primary design axes.
Streaming, GI, vegetation — subordinate to "does gunplay feel accurate under
80 ms ping?". That inversion of priorities is the single most important
lesson here.

## IW Engine lineage

- **id Tech 3 (1999)**: Infinity Ward licensed Quake III Arena's engine for
  CoD1 (2003), specifically for its QuakeWorld-lineage netcode (client
  prediction + server reconciliation + snapshot interpolation).
- **IW 2.0 — CoD 2 (2005)**: fork point. Renderer + memory reworked enough
  to count as distinct branch.
- **IW 3.0–5.0 — MW (2007), WaW (2008), MW2 (2009), MW3 (2011)**: incremental.
- **Treyarch fork** (~IW 3.0): Black Ops 1 (2010) → BO4 (2018), Cold War
  (2020), iterated independently.
- **Sledgehammer fork** (IW 6.x/7.x): Advanced Warfare (2014), WWII (2017),
  Vanguard (2021). Exosuit movement was an engine-level retrofit.
- **IW 8.0 — MW (2019)**: marketed as ground-up rewrite (nuance next section).
- **IW 9.0 — MW II (2022)** unifies IW, Treyarch, Sledgehammer onto one
  build. MW III (2023) + Black Ops 6 (2024) ship on IW 9.0 — BO6 is the
  first Treyarch title on the shared engine after ~12 years on the BO2-era
  branch.

15+ years of accretion means Quake-3-era code paths almost certainly still
exist in corners of the codebase.

## IW Engine key tech

- **Tick rate**: 60 Hz core MP; Warzone ~20–24 Hz at 150 players (community-
  measured, persistent criticism). Ranked modes experiment higher.
- **Hybrid hit-scan + projectile** per-weapon tuned for feel vs counterplay.
- **Crowd replication**: 150-player Warzone lobbies need aggressive relevancy
  filtering + delta compression per client.
- **Asset streaming**: tight MP maps, fast LRU, distant fidelity traded for
  player/VFX headroom.
- **Audio** (Dolby Atmos, positional, per-weapon voicing, footstep occlusion)
  treated as gameplay-critical — competitive edge.
- **Matchmaking / SBMM** + region routing with engine-integrated hooks.
- **Ricochet Anti-Cheat (2021+)**: kernel Windows driver + server-side
  behavioural detection + hardware attestation (TPM 2.0 / Secure Boot
  announced for Black Ops 7).

## MW2019's engine rewrite

Branded "built from the ground up"; per Game Informer's tech piece and dev
interviews, reality:

- **Renderer rewritten**: new PBR pipeline, DX12, hybrid tile-based
  streaming, photogrammetry. Genuinely new.
- **Ray tracing** (PC only at launch): RT shadows, later extended.
- **Audio + animation** systems overhauled.
- **Gameplay code preserved**: recoil curves, TTK, hit-reg timing deliberately
  kept. "Rewrite the pipes, keep the gunplay."
- **5-year dev** with Infinity Ward Poland leading engine work parallel to
  California.

Useful framing: **renderer + tooling + audio** rewrite on a preserved
simulation core. Good pattern to modernise presentation without breaking feel.

## Warzone tech

- BR build of IW Engine. **150 concurrent players** default (experimented up
  to 200; casual variants mix bots — e.g. 28 humans + 120 bots).
- **Large-map streaming**: Verdansk-scale needed work the 6v6 build never
  did — long sight lines, bigger asset budget, distance relevancy.
- **MP map integration**: Plunder/Resurgence reuse MP geometry; asset pipeline
  extended so one mesh serves multiple modes.
- **Tick rate is the weak spot**: 20–24 Hz under load. Cost scales ~linearly
  with tick × player count — it's a budget problem, not a tech problem.
- **Cross-platform**: unified matchmaking, input-based segmentation handled
  above the engine.

## Slipspace Engine (343)

- Ancestor **Blam!** from Bungie, debut Halo: Combat Evolved (2001). Powered
  Halo 2, 3, ODST, Reach. Strengths: portal occlusion, radiosity, vehicle
  physics, mission scripting, Forge/Theater dev tools shipped in-game.
- Bungie forked Blam! into the **Tiger Engine** for Destiny after the 2007
  Microsoft/Bungie split. Halo IP stayed with MS; engine knowledge left with
  Bungie.
- 343 inherited Halo in 2011; Halo 4 (2012) + Halo 5 (2015) shipped on their
  evolution of the Blam! branch Bungie left behind.
- Post-Halo 5, 343 started **Slipspace**: re-architected engine. Bonnie Ross
  called it "the biggest investment ever made by 343". Teased E3 2018.
- Shipped on **Halo Infinite (Dec 2021)** after one-year delay + substantial
  rework post the 2020 gameplay reveal backlash. Bungie vet Joseph Staten
  brought in to lead campaign through final production.

## Halo Infinite tech

- **Open-structure campaign** on Zeta Halo: not fully open-world, large
  interconnected zones with dynamic day/night and real-time exterior
  lighting. Slipspace had to grow beyond Blam!'s corridor-plus-vista roots.
- **Multiplayer framerate**: 60+ FPS, 120 FPS on Xbox Series X, uncapped on
  PC.
- **Tick rate**: 4v4 arena at **60 Hz**; Big Team Battle (up to 24 players)
  at **30 Hz**; campaign co-op at 30 Hz. The 30 Hz BTB rate is a common
  complaint (desync, peeker's advantage).
- **Ray-traced sun shadows**: added Season 3 (March 2023) on PC, later on
  Xbox Series X|S. Multiplayer only, ~15 months late vs expectation.
- **Forge** (shipped beta Season 3): visual node-graph scripting,
  auto-generated nav meshes, AI toolkit (campaign enemies in custom maps),
  lighting probes, bot support. Genuinely innovative UGC system.

## Slipspace's troubles (publicly known)

- Multi-year delays: Infinite slipped from Xbox Series X launch (Nov 2020) to
  December 2021.
- Scope cut: Staten publicly admitted 343 "didn't have the time to go after
  the level of fidelity as much as we wanted". Campaign co-op cancelled;
  Forge delayed to a later season.
- Leadership churn: founder Bonnie Ross, creative head Staten, design lead
  Jerry Hook, MP lead Andrew Witts, art director Nicolas Bouvier all
  departed 2022–2023. Staten left Microsoft April 2023 for Netflix Games.
  Heavy 343 layoffs early 2023.
- Engine decision: Jan 2023 Unreal-evaluation reports; **October 2024**
  rebrand to Halo Studios + confirmed all future Halo titles on **Unreal
  Engine 5**. "Project Foundry" is the UE5 research effort. Slipspace now in
  maintenance for Infinite's live-service tail only.

Slipspace is the textbook cautionary tale of engine rewrite attempted
concurrently with a flagship shipping title. Each starved the other of
engineering attention; both suffered.

## Shooter-specific engine features

Orthogonal to graphics — what actually defines a competent FPS engine:

- **Fixed tick rate simulation** (60 Hz typical, 64/128 Hz tournament).
  Logic at fixed interval; render interpolates.
- **Server authority**: server simulates truth. Reduces wall-hack / teleport
  exploit surface.
- **Client-side prediction**: client runs identical simulation on local
  input without waiting. Responsive feel at 80 ms.
- **Server reconciliation / rollback**: on diff, rewind to server-ack'd
  snapshot and re-apply unacknowledged inputs.
- **Lag compensation**: server rewinds world state by shooter's latency
  (capped ~200 ms) to validate hits where targets *were*. Causes "shot
  around corners" artefacts but is necessary.
- **Delta-compressed state replication** vs last-ack'd baseline.
- **Snapshot interpolation** for remote entities, render 50–100 ms behind
  latest received snapshot.
- **Anti-cheat at engine level**: kernel drivers + hardware attestation
  below user-mode code.
- **Audio as gameplay**: footsteps and gunshot direction are competitive
  information, not ambience.

## En qué son buenos

**IW / CoD**: proven 60 Hz gunplay feel at scale; the gunfeel-audio-
animation-hitreg synchronisation is industry-leading (most copycats fail to
match it). Shipping infrastructure: cross-platform matchmaking, Ricochet,
BR-scale replication, live-ops. Audio pipeline specifically is a
competitive differentiator.

**Slipspace / Halo**: Blam! heritage gave strong vehicle physics (Warthog
feel), sandbox AI variety (Grunts/Elites behavioural mix), and one of the
best MP map dev-tool stacks in the industry. Halo Infinite's Forge (visual
scripting + auto nav mesh + AI toolkit + bot support) is genuinely
innovative and hasn't been equalled elsewhere.

## En qué fallan

**IW**: 15+ years accreted technical debt; data-miners surface CoD2-era
assets in modern builds. Annual shipping treadmill prevents deep rewrites
(MW2019 is the only renderer rewrite, took 5 years parallel). Warzone tick
rate: 20 Hz for a 150-player shooter in 2024 is indefensible on merits;
persists because cost scales brutally. Install sizes >200 GB because the
asset pipeline wasn't designed for a decade of DLC accumulation.

**Slipspace**: canonical rewrite-while-shipping failure. Seven years from
Halo 5 to a shipping Slipspace title, feature-incomplete at launch,
campaign co-op cut, Forge delayed. Engine-team turnover that never let
knowledge stabilise. Ultimately abandoned for UE5 — a public concession the
rewrite never produced a sustainable long-term foundation.

## Qué podríamos copiar (mecanismo concreto) para ALZE Engine

If ALZE ever ships a multiplayer FPS (big if — see next section):

- **Fixed-timestep simulation decoupled from render**. 60 Hz min, 120 Hz for
  competitive. Gaffer-on-Games "Fix Your Timestep" accumulator pattern.
- **Client-side prediction with deterministic simulation**: no uncontrolled
  RNG, no wall-clock dependencies in the sim path.
- **Server reconciliation / rollback**: client keeps a ring buffer of
  unacknowledged inputs, replays on diff with server snapshot. Canonical
  QuakeWorld / Gabriel Gambetta pattern.
- **Lag compensation with capped window (~200 ms)**: server keeps last ~N
  ms of world state, rewinds on hit validation.
- **Delta-compressed snapshots** vs last client-ack'd baseline: changed
  fields only, quantise positions (16-bit fixed-point rather than f32),
  bitpack per-field.
- **Snapshot interpolation** for remote entities, render 50–100 ms behind
  latest snapshot.
- **Audio as first-class subsystem**: footsteps, gunshot directionality and
  occlusion resolved synchronously with tick, not as async postprocess.
- **Engine-level primitives for MP meta**: spawn protection timers, map
  rotation tickets, team-balance hooks. In the engine as reusable services,
  not re-scripted per game mode.

## Qué NO copiar

- **IW's 15-year codebase inheritance** — start clean with modern netcode
  primitives instead of reverse-engineering legacy.
- **Slipspace's rewrite-while-shipping approach**: if you need to rewrite,
  stop shipping; if you need to ship, don't rewrite.
- **Shipping a multiplayer FPS as a small team**: CoD has ~3,000 devs, Halo
  Studios has 500+. Warzone live-ops alone is larger than most indie
  studios. Scale mismatch isn't a rounding error.
- **Annual release cadence**: forces architectural decisions that block
  long-term improvement. Activision spacing MW/BO titles is a tacit
  admission.
- **Tick-rate compromises for player-count bragging rights**: 150-player BR
  at 20 Hz is marketing-driven. Pick one — high player count OR high tick
  rate — and be honest.

## La lección más amplia

FPS engine design is a distinct discipline from open-world engine design.
Primary technical concerns, ranked:

1. Tick rate + simulation determinism (not streaming radius).
2. Hit registration under latency (not GI / volumetrics).
3. Anti-cheat surface (not photogrammetry pipelines).
4. Audio-to-gameplay fidelity (not cinematic sound design).
5. Matchmaking / session infrastructure (not save systems).

If ALZE's direction is not multiplayer FPS, almost nothing here applies —
tick rate is irrelevant for a single-player ARPG. If ALZE ever commits to
multiplayer shooter territory: build from QuakeWorld-lineage netcode
primitives, treat audio as load-bearing gameplay data, and don't try to
match CoD on content volume.

Uncertainty: Warzone tick-rate numbers are community-measured, not official.
IW Engine version numbering is partially fan-reconstructed. Slipspace
internals are almost entirely undocumented; postmortem context here is
pieced from press interviews.

## Fuentes consultadas

IW / CoD:
- https://en.wikipedia.org/wiki/IW_(game_engine) · https://callofduty.fandom.com/wiki/IW_engine
- https://en.wikipedia.org/wiki/Id_Tech_3 · https://callofduty.fandom.com/wiki/Id_Tech_3
- https://gameinformer.com/2019/08/26/the-impressive-new-tech-behind-call-of-duty-modern-warfare
- https://en.wikipedia.org/wiki/Call_of_Duty:_Modern_Warfare_(2019_video_game)
- https://www.callofduty.com/blog/2022/12/call-of-duty-modern-warfare-II-next-gen-tech-advanced-game-performance
- https://en.wikipedia.org/wiki/Call_of_Duty:_Modern_Warfare_II_(2022_video_game)
- https://www.videogameschronicle.com/features/interviews/interview-how-treyarch-wants-black-ops-6s-new-movement-system-to-change-the-genre/
- https://en.wikipedia.org/wiki/Treyarch · https://en.wikipedia.org/wiki/Call_of_Duty:_Warzone
- https://www.gamespot.com/articles/how-black-ops-6-being-forced-to-wipe-the-slate-clean-led-to-its-biggest-new-feature/1100-6526217/
- https://www.gamespot.com/articles/call-of-duty-warzone-experimenting-with-150-player-matches/1100-6529768/
- https://www.callofduty.com/blog/2025/03/call-of-duty-warzone-verdansk-map-return-intel-drop
- https://diamondlobby.com/server-tick-rates/
- https://www.callofduty.com/ricochet · https://support.activision.com/articles/ricochet-overview
- https://www.callofduty.com/blog/2021/10/ricochet-anti-cheat-initiative-for-call-of-duty
- https://www.gamesradar.com/over-3000-people-are-working-on-call-of-duty-says-activision/
- https://kotaku.com/activision-call-of-duty-3000-cod-developers-modern-warf-1848888635

Halo / Slipspace / Blam!:
- https://www.halopedia.org/Slipspace_Engine · https://www.halopedia.org/Blam_engine
- https://bungie.fandom.com/wiki/Blam!_Engine · https://bungie.fandom.com/wiki/Tiger_Engine
- https://www.halowaypoint.com/en-us/news/our-journey-begins
- https://www.halowaypoint.com/news/closer-look-halo-infinite-online-experience
- https://www.theloadout.com/halo-infinite/tick-rate
- https://en.wikipedia.org/wiki/Halo_Infinite · https://en.wikipedia.org/wiki/Halo_Studios
- https://www.gamedeveloper.com/production/-i-halo-infinite-i-s-joseph-staten-discusses-the-creation-of-open-world-i-halo-i-
- https://www.pcgamer.com/halo-creative-lead-joseph-staten-leaves-microsoft-3-months-after-heavy-layoffs-at-343-industries/
- https://gameinformer.com/2023/01/31/343-industries-reportedly-switching-from-its-own-slipspace-engine-to-unreal-for-some
- https://www.halowaypoint.com/news/a-new-dawn
- https://www.gamespot.com/articles/new-halo-games-project-foundry-unreal-engine-5-when-to-expect-them/1100-6527083/
- https://support.halowaypoint.com/hc/en-us/articles/10581874119828-Halo-Infinite-Forge-Overview
- https://news.xbox.com/en-us/2023/10/13/the-forge-ai-toolkit-is-going-to-change-halo-infinite-forever/
- https://www.videogameschronicle.com/news/halo-infinites-season-3-is-out-now-adding-ray-tracing-on-pc-and-more/

Netcode technique references:
- https://en.wikipedia.org/wiki/Client-side_prediction · https://en.wikipedia.org/wiki/Netcode
- https://www.gabrielgambetta.com/client-side-prediction-server-reconciliation.html
- https://www.gabrielgambetta.com/lag-compensation.html
- https://developer.valvesoftware.com/wiki/Source_Multiplayer_Networking
- https://gafferongames.com/post/fix_your_timestep/
- https://gafferongames.com/post/snapshot_compression/ · https://gafferongames.com/post/snapshot_interpolation/
- https://snapnet.dev/blog/netcode-architectures-part-3-snapshot-interpolation/
