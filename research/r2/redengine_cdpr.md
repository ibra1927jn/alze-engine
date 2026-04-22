# REDengine (CD Projekt Red) — case study

Research date: 2026-04-21. Purpose: inform ALZE Engine's proprietary-vs-licensed decision by studying the clearest recent case of a mid-sized studio abandoning a bespoke engine.

## Overview

CD Projekt started as a distributor, pivoted to development with **The Witcher (2007)** built on a heavily modified BioWare Aurora Engine. When that engine could not scale to the studio's ambitions, CDPR built **REDengine** in-house and shipped four major titles on its successive versions:

- **REDengine 1** — *The Witcher 2: Assassins of Kings* (2011). Windows, Xbox 360. First 64-bit work later.
- **REDengine 2** — refined version of 1; sometimes bundled as a re-tag rather than a full rewrite.
- **REDengine 3** — *The Witcher 3: Wild Hunt* (2015). 64-bit only, open-world-first, cross-gen (PS4/Xbox One, later Switch/PS5/Series).
- **REDengine 4** — *Cyberpunk 2077* (2020) + *Phantom Liberty* (2023). Ray tracing, first-person, dense urban streaming.

Team sizes grew roughly in parallel: ~150 devs on Witcher 2, ~240 on Witcher 3 core team, 500+ on Cyberpunk 2077 at peak (plus outsourcers). CDPR's total headcount passed 1,200 by 2024.

**Licensing model**: never licensed externally. REDengine was a competitive-advantage asset for CDPR alone — no third party ever shipped on it. This is critical to the economics argument below: zero external revenue ever amortised engine R&D.

## REDengine architecture

Public info is fragmented across GDC talks, the 80.lv RED 3 interview, and community reverse-engineering. What's known:

- **Physics**: Havok on REDengine 1/2. REDengine 3 added NVIDIA PhysX + APEX for cloth and destruction on PC (console builds used vanilla/CPU fallback). REDengine 4 continued PhysX-flavoured integration.
- **Animation**: in-house blend-tree + state-machine system. For W3 dialogue, CDPR built a procedural cinematic system — about 2,400 base dialogue animations composed algorithmically to generate camera, facial, body, and look-at tracks. This let two writers/designers ship ~35h of spoken quest content without per-scene cinematic hand-staging.
- **Scene representation**: prefab-based hierarchy (confirmed in GDC 2023 "Building Night City" talk). Worlds assembled from nested layer/entity templates; Witcher 3 also used Umbra 3 for visibility/occlusion culling with custom streaming hooks.
- **UI**: Scaleform GFx (Flash-based).
- **Audio**: FMOD on earlier titles, later Wwise-style in-house mix graph for CP77.
- **Scripting**: **REDscript** — CDPR's proprietary statically-typed OO language, compiled to bytecode. Community reverse-engineered it: `jac3km4/redscript` on GitHub provides compiler, decompiler, language server (redscript-ide), and debug adapter (redscript-dap). Cyberpunk's entire modding ecosystem (Cyber Engine Tweaks, Redscript mods, ArchiveXL) was built on this reversal, because CDPR never shipped official scripting tools for CP77.

The **REDkit** editor (refurbished subset of the internal toolchain) was only released for Witcher 3 in 2024 — nearly a decade after the game. This is characteristic: CDPR never productised the engine for outside use.

## Witcher 3 achievements

- **Open world scale**: Velen + No Man's Land + Skellige + Novigrad, all seamlessly streamed at 64-bit, no loading between regions. Umbra 3's streaming was co-developed with CDPR specifically for W3's density (manually-placed occluders were infeasible at that scale).
- **Quest density**: ~200 hand-authored quests with flag-tracked branches. Gwent embedded as a full sub-game with tournament brackets; quest completion interacts with card availability and NPC dialogue.
- **Dialogue variation**: the procedural cinematic system let writers iterate on lines without rescheduling mocap, a huge productivity lever. Most non-critical-path dialogue camera/body work is algorithmic.
- **Vegetation**: SpeedTree integration for forests + custom wind/shader work.
- **Weather & atmospherics**: volumetric clouds/fog/mist in a forward+ hybrid renderer, unusual for 2015.
- **Hair**: NVIDIA HairWorks for Geralt, horses, and ~36 monsters. Used DX11 hardware tessellation — only viable on Maxwell-era NVIDIA GPUs, caused an AMD optimisation controversy, and was scaled down / effectively disabled on consoles (PS4/Xbox One used vanilla hair). A clear example of proprietary-engine tech that **didn't cross-port cleanly**.

## Cyberpunk 2077 RT implementation

REDengine 4 was one of the first shipping engines with a full hybrid RT path on launch day (Dec 2020):

- **RT reflections** — BVH-traced on opaque and transparent surfaces. The player's own body is excluded from BVH (visible as a missing self-reflection).
- **RT shadows** — directional sun + local light shadows in RT.
- **RT diffuse GI** — probe-based hybrid. Cyberpunk launched with a GI-probes system placed per-location at 1.5–4m spacing, combined with RT bounces. Not full path-traced indirect, but the best mass-market hybrid of its era.

**RT Overdrive (April 2023)** made REDengine 4 the industry reference for real-time path tracing:

- Full path tracing — primary rays and multiple indirect bounces.
- **ReSTIR GI** — Reservoir-based SpatioTemporal Importance Resampling, from NVIDIA Research (Bitterli, Wyman et al., 2020–2021 papers). Cyberpunk 2077 was the public flagship integration; CDPR + NVIDIA co-presented the work at GDC 2024 ("Pushing Path Tracing One Step Further", Pawel Kozlowski).
- **Shader Execution Reordering** — RTX 40-series HW feature, essential for viable perf.
- **DLSS 3 Frame Generation** — effectively mandatory for playability: native 4K RT Overdrive on RTX 4090 delivered ~16–18 fps (Digital Foundry, Tom's Hardware benchmarks); DLSS 3 Perf + FG pushed this to 90–110 fps.
- Patch 2.1 (Dec 2023) added improved ReSTIR GI, reducing darkening artifacts from denoising.

Digital Foundry (Alex Battaglia) has repeatedly called this the most technically ambitious RT workload shipping on consumer hardware through 2024–2025.

**Shader permutation explosion** contributed to CP77's early stutter. The hybrid RT + rasterised path combined with many material variants produced a large shader cache; Phantom Liberty's 2.0 update was separately reported to invalidate cached shaders and force recompilation, causing a second stutter wave in Sep 2023.

## Technical postmortems published

- **GDC 2023** — "Building Night City: The Technology of Cyberpunk 2077" (Charles Tremblay). Covers prefab hierarchy, streaming redesign, memory/CPU budgeting. Slides public via GDC Vault / s3.
- **GDC 2024** — "RT: Overdrive in Cyberpunk 2077 Ultimate Edition: Pushing Path Tracing One Step Further" (Pawel Kozlowski, NVIDIA). ReSTIR integration deep dive. Companion PDF on `intro-to-restir.cwyman.org`.
- **Digital Dragons (Krakow)** — yearly Polish conf with multiple CDPR talks across 2015–2024 on engine, AI, and cinematics.
- **Digital Foundry** — Battaglia's hybrid-RT and path-tracing analyses are the best public-facing deep technical reads.
- **Post-launch interviews** — CEO Adam Kiciński and CFO Piotr Nielubowicz (not CTO — the CTO is Paweł Zawodny) handled the post-launch apology tour (VGC Q&A, Dec 2020 call transcripts on cdprojekt.com). They acknowledged "ignored signals about old-gen console readiness".
- **80.lv** — W3 RED 3 tech interview from 2015 remains a primary source on the procedural dialogue system.

## The UE5 migration decision (2022)

On **March 21, 2022**, CDPR announced the "New Witcher Saga" on Unreal Engine 5 as part of a **15-year strategic partnership with Epic Games** (with prolongation option). The partnership covers licensing plus joint technical development of UE5 and future Unreal versions.

**Officially stated reasons**:

- Access to "cutting-edge game development tools" with predictability.
- Open-world support in UE5 evolving fast; better to build on top of it than alongside.
- Let writers/designers focus on content instead of engine-chasing.

**The unstated/ implicit reasons, pieced from CTO Paweł Zawodny's interviews and post-launch retrospectives**:

- **Engine-game dev debt**: every CDPR release required what staff described as "basically rewriting REDengine from scratch." Major refactors between Witcher 2→3 and again W3→CP77. RED 4→5 was judged too expensive.
- **Cyberpunk's launch** exposed the cost of parallel engine + game development under crunch: bugs from half-finished engine features bled into game QA.
- **Hiring**: REDengine-specific engineers are scarce and expensive; UE/Unity talent pool is vast. CDPR's Boston (Orion) and Vancouver expansions needed hireable talent fast.
- **Tooling**: UE5's editor, Blueprints, Nanite, Lumen, and MetaHumans mean new hires are productive on day one. REDkit was never productised internally; onboarding took months.

Phantom Liberty ended up being **the only CP77 expansion** precisely because the engine team was migrating. CDPR publicly blamed the lack of additional CP77 content on the UE5 pivot.

## What CDPR is taking to UE5 (the Unreal-REDengine collaboration)

Announced joint work — available to all UE5 licensees (not locked to CDPR):

- **FastGeo Streaming** — fast environment load/stream for open worlds, built with Epic.
- **Nanite Foliage** — dense forests/fields at Nanite detail.
- **Unreal Animation Framework** improvements — character movement at crowd scale.
- **Mass AI** — large dynamic crowds (CDPR publicly targets "most realistic crowd system in any game" for Cyberpunk sequel).
- **ML Deformer** — subtle realistic character deformations.
- **MetaHuman** integration at scale.

The Witcher 4 (codename **Polaris**) tech demo at **State of Unreal 2025** ran on PS5 at 60 fps showcasing these — region of Kovir, Ciri as protagonist, monster contract vertical slice. Team size as of 2025: ~450 devs on Polaris, ~200 on Cyberpunk sequel (Project Orion at CDPR Boston, led by Gabe Amatangelo).

## En qué fue buena REDengine

- **Visual fidelity ceiling**: RT Overdrive in CP77 remains the industry RT reference as of 2025. Path-traced multi-bounce local lighting with emissive shadows on neon surfaces was unheard-of before.
- **Dialogue animation density**: W3's procedural cinematic system shipped ~35h of fully directed dialogue with a small animation team. This is a proprietary-engine dividend — the system was bespoke to CDPR's content pipeline.
- **Hand-authored open-world density**: Novigrad + Velen + Skellige was the gold standard for narrative open-world design in 2015. CP77's Night City pushed vertical density (street level, skybridges, interiors) beyond what was being shipped elsewhere.
- **Quest-state aware systems**: deep branching with persistent flag/fact tracking across ~200 quests.

## En qué falló

- **Launch stability on old-gen**: CP77 on PS4/Xbox One was the worst major launch of the generation. Sony pulled the game from PSN (unprecedented for a AAA), offered refunds; CDPR's stock dropped ~20%, $1.8B market cap lost.
- **Shader compilation stutter**: launch + again on Phantom Liberty 2.0 (cache invalidation forced recompile).
- **Cross-platform quality gap**: HairWorks in W3, RT fidelity in CP77 — proprietary features often optimal on PC/NVIDIA, degraded on consoles/AMD.
- **Tooling complexity + onboarding**: no productised editor until REDkit in 2024 (for W3 only, nearly a decade late).
- **Budget contention**: engine-dev and game-dev drawing from the same pool of engineers meant neither was ever fully staffed.
- **Knowledge silo**: REDscript had no official docs; the modding community had to reverse-engineer it.

## Lección del case study para ALZE Engine

The honest thesis for a small/mid team:

- **Proprietary engine economics break even around 5+ shipped titles** on the same base, or when your game's shape is so unique that no licensed engine fits. CDPR shipped 4 titles on REDengine (W2, W3, CP77, Phantom Liberty counts as 0.5) and still found the economics unfavourable going forward.
- **Hiring**: UE/Unity devs are abundant and productive on day one. Proprietary-engine devs require 3–6 months onboarding on undocumented tooling. This compounds as the team grows.
- **Tech debt compounds across engine generations**. RED 2→3 was a major rewrite, RED 3→4 another, RED 4→5 judged too expensive vs switching to UE5. Every rewrite is a ~2-year tax that does not ship player-facing content.
- **Engine + game from the same budget** is the silent killer. CDPR crunched partly because engine features were landing mid-production. A licensed engine decouples: Epic ships UE5.x on a schedule; you ship content on yours.
- **For ALZE specifically**: if the ambition is "ship 3–5 games over 10 years exploring a consistent mechanic/world", proprietary is defensible. If the ambition is "ship 1–2 games exploring a genre/idea", a modded UE5, or a bgfx/Sokol-level mid-level library with hand-written game code, is economically saner and removes the engine-dev tax.

## Qué podríamos copiar (mecanismo concreto)

- **Quest/dialogue branching with fact-tracking**: durable world-state flags that dialogue, NPC routines, and other quests all read. W3's model (observable from REDkit) — a graph of nodes with preconditions/effects on a global fact DB — is portable, engine-agnostic, and implementable in a weekend as a data layer over any engine.
- **Procedural cinematic dialogue**: W3-style composition of camera/facial/body/lookat tracks from a library of base animations. This is *not* an engine feature — it's a content pipeline convention. Adopt it on top of UE5's Sequencer.
- **Open-world streaming with quest-state-aware LOD**: Novigrad's pattern — load interior detail tied to quest progression, not just distance. This is authorial discipline, not engine tech.
- **Facial animation blending for dialogue performance**: dedicated mimic layer on top of body animation, driven from phoneme + emotion tags. Cheaper than performance capture for every line.

## Qué NO copiar

- **Building a proprietary engine without 3+ titles' worth of runway**. CDPR shipped The Witcher 1 on Aurora and Witcher 2 on REDengine 1 as the *warm-up* before RED 3's ambition. Without that shipped base, CP77's scope on RED 4 would have been unthinkable. A small team should not start by building an engine; ship the game first.
- **Under-staffing engine team relative to game team**. CDPR's engine team never grew proportionally to game team; every release required cross-pulling.
- **Shipping engine features parallel to game features** on the same critical path. Either freeze the engine at a known-good revision per game, or don't.
- **Never productising the editor**. If you're going to build proprietary tooling, eat the productisation cost — your own team is the biggest licensee.
- **Proprietary scripting language without docs**. REDscript required community reverse-engineering; this wastes your own onboarding time too.

## Fuentes consultadas

- CD Projekt press: https://www.cdprojekt.com/en/media/news/new-witcher-saga-announced-cd-projekt-red-begins-development-on-unreal-engine-5-as-part-of-a-strategic-partnership-with-epic-games/
- CD Projekt Red blog — Witcher 4 UE5 tech demo: https://www.cdprojektred.com/en/blog/149/working-with-epic-to-debut-the-witcher-4-unreal-engine-5-tech-demo-at-unreal-fest
- Wikipedia REDengine: https://en.wikipedia.org/wiki/REDengine
- GDC Vault — Building Night City: https://www.gdcvault.com/play/1028734/Building-Night-City-The-Technology
- GDC 2023 slides PDF: https://media.gdcvault.com/gdc2023/Slides/Buildingnightcity_Tremblay_Charles.pdf
- GDC 2024 — RT Overdrive ReSTIR: https://www.nvidia.com/en-us/on-demand/session/gdc24-gdc1002/
- ReSTIR Cyberpunk integration (Kozlowski, NVIDIA): https://intro-to-restir.cwyman.org/presentations/2023ReSTIR_Course_Cyberpunk_2077_Integration.pdf
- NVIDIA Research ReSTIR GI paper: https://research.nvidia.com/publication/2021-06_restir-gi-path-resampling-real-time-path-tracing
- NVIDIA GeForce — RT Overdrive interview: https://www.nvidia.com/en-us/geforce/news/cyberpunk-2077-ray-tracing-overdrive-mode-interview/
- 80.lv — RED 3 tech interview: https://80.lv/articles/the-witcher-3-interview-a-deeper-look-into-redengine-3/
- Tom's Hardware — RT Overdrive path tracing analysis: https://www.tomshardware.com/features/cyberpunk-2077-rt-overdrive-path-tracing-full-path-tracing-fully-unnecessary
- PC Gamer — W3 procedural dialogue: https://www.pcgamer.com/most-of-the-witcher-3s-dialogue-scenes-was-animated-by-an-algorithm/
- Wccftech — UE5 switch reasons (Zawodny): https://wccftech.com/cd-projekt-red-switched-to-unreal-engine-5-the-witcher/
- VGC post-launch Q&A: https://www.videogameschronicle.com/features/interviews/qa-cd-projekt-on-how-its-fixing-cyberpunk-2077/
- Kotaku — Sony pulls CP77: https://kotaku.com/sony-pulls-cyberpunk-2077-from-playstation-store-says-1845908592
- GameRant — Witcher 4 dev count: https://gamerant.com/cd-projekt-red-witcher-4-260-devs/
- GamingBolt — W4 450 devs, Orion scaling: https://gamingbolt.com/the-witcher-4-team-nears-450-developers-cd-projekt-red-outlines-plans-to-scale-cyberpunk-2-studios
- jac3km4/redscript (community decompiler): https://github.com/jac3km4/redscript
- REDkit on Steam: https://store.steampowered.com/app/2684660/The_Witcher_3_REDkit/
- Umbra/GDC W3 streaming: https://www.gdcvault.com/play/1020231/Solving-Visibility-and-Streaming-in
- Cyberpunk 2.0 / Phantom Liberty patch notes: https://www.cyberpunk.net/en/news/49060/update-2-0
- CreativeBloq — UE5 switch (CDPR reveals real reason): https://www.creativebloq.com/3d/video-game-design/cyberpunk-2077-dev-reveals-why-it-really-moved-to-unreal-engine-5
- Project Orion pre-production (CDPR Boston): https://en.wikipedia.org/wiki/CD_Projekt_Red_Boston
