# AAA In-House Engines — Research Notes for ALZE Engine

> Target: ALZE Engine (`/root/repos/alze-engine`, C++17, no RTTI/exceptions, ~25-30K LOC, small team).
> Goal: extract AAA ideas that a small team can ship without a 200-engineer tooling group.

## Overview

Studios build engines in-house when the licensing math flips against Unreal/Unity. The usual drivers:

1. **Control over the hot path.** An engine tuned to one game style (corridor shooter, open-world RPG, driving sim) outperforms a generalist engine by a wide margin. Both id Software's 60 Hz Doom guarantee and DICE's 64-player Battlefield netcode come from aggressive specialization.
2. **Optimization for exact content shape.** Megatextures made sense for Rage's hand-painted terrain; SVOGI made sense for CryEngine's dynamic time-of-day. Neither makes sense for every game.
3. **IP and competitive moat.** Valve's Source 2 + Rubikon physics, Remedy's Northlight path tracing, Guerrilla's Nubis cloudscapes — these are marketing differentiators as much as rendering features.
4. **Licensing economics.** Unreal's 5% royalty on a 30M-unit blockbuster is real money. In-house breaks even somewhere around the second or third title on the engine.
5. **Tooling ownership.** Artists and designers iterate at the speed the editor allows. Owning Hammer 2 (Valve) or the Creation Kit (Bethesda) is owning the community pipeline.

**When it is worth it:** a studio shipping >=3 titles on similar tech, with ~30+ engine engineers and a willingness to eat a 2-3 year tooling bring-up cost. Below that bar, UE5/Unity wins.

**When it is not:** first-time IPs with uncertain scope, single-title studios, or teams under ~15 engineers total. The cost of re-inventing an editor, asset pipeline, profiler stack, and platform certification layer crushes novel gameplay development. The ALZE situation (C++17, small team, ~30K LOC) sits firmly in the "borrow ideas, do not rebuild the world" bucket — the goal is to cherry-pick architectural patterns, not compete on feature surface.

## id Tech 6/7

id Tech under Tiago Sousa (rendering director since 2016) has been a Vulkan showcase. Key traits across Doom (2016), Doom Eternal (2020), and Doom: The Dark Ages (2025, id Tech 8):

- **Vulkan pioneer.** Doom 2016 was the highest-profile Vulkan launch title; id Tech 6/7 is effectively a Vulkan-native renderer with a D3D12 back-port.
- **Hybrid deferred + clustered forward.** id Tech 6 moved from the deferred renderer of Doom 2016 to a clustered-forward pipeline: the view frustum is subdivided into a 3D grid of cells, lights/decals/probes are voxelized into cells, and the primary forward pass does a lookup per pixel. id Tech 7 kept this model.
- **Virtual texturing evolution.** id Tech 5's MegaTexture (Rage) used a single ~128Kx128K static atlas streamed reactively — famous for pop-in. id Tech 6 replaced it with a tiled atlas (16Kx8K of 128x128 tiles) with GPU-driven caching. id Tech 7 largely abandoned static mega-textures in favor of runtime material composition.
- **Async compute everywhere.** GPU particle simulation, skinning (vertex compute → buffer, consumed by later passes so the geometry pass has fewer shader permutations), and "present from compute" all ride the async queue (Simon Coenen's graphics study of Doom Eternal is the canonical breakdown).
- **Aggressive bindless.** Doom Eternal draws the entire frame with a handful of drawcalls — a single descriptor table holds all textures, material indices live per-draw (Jean Geffroy et al., "Rendering the Hellscape of Doom Eternal," SIGGRAPH 2020).
- **Job-based multithreading.** Billy Khan (lead engine programmer): every subsystem — AI, physics, rendering, animation — is a fibre-dispatched job, which is the reason the engine can scale to 1000 FPS on a 7950X.
- **Data-driven, no big editor.** id Tech famously has no monolithic editor. Levels and gameplay logic are driven from `.decl` text files with hot reload. Level streaming is organized around named "zones." Iteration is very fast precisely because there is no editor database to invalidate.
- **Path tracing in six months.** Tiago Sousa's SIGGRAPH 2025 talk "Fast as Hell: idTech 8 Global Illumination" reports a production path-tracer shipped in Doom: The Dark Ages in roughly half a year — possible only because the renderer already had clean ray queries and a disciplined material system.
- **Shader module streaming.** id Tech streams shader modules at zone boundaries and compiles pipelines in parallel on worker threads; hitches on first-encounter materials are largely eliminated by pre-warming PSOs from telemetry traces of prior play sessions.
- **"Zone"-scoped level streaming.** Each zone is a self-describing text manifest: entities, lights, streaming volumes, gameplay triggers. Cross-zone transitions are resolved by hash-stable IDs, so designers can reorganise the world without breaking save games.

## Source 2

Valve's Source 2 is a textbook case of a legacy-plus-rewrite hybrid.

- **Legacy continuity.** Ships Dota 2, CS2, Deadlock, and Half-Life: Alyx. Significant chunks of Source 1 data formats and tooling lineage survive under the hood, but the renderer, physics (Rubikon replacing Havok), and entity system are modern rewrites.
- **Vulkan + D3D11.** CS2 and Half-Life: Alyx use a forward renderer; Dota 2 and Deadlock stayed on deferred. The engine chooses the renderer per-title rather than forcing one model.
- **Entity component system.** Source 2's entity pipeline is component-oriented rather than Source 1's monolithic CBaseEntity hierarchy.
- **Dynamic scene compilation.** Hammer 2 (sometimes called Hammer 5) dropped the old VBSP/VVIS/VRAD chain. Visibility is computed on-the-fly and lighting is deferred; the single Resource Compiler replaces the three-stage build. This is the biggest artist-facing change: what used to be a multi-minute bake is now seconds.
- **Panorama UI.** Valve's web-style (HTML5/XML/JS/CSS) UI framework, shared across CS2, Dota 2, and Deadlock. It is the one piece of Source 2 clearly designed to be reused across titles, not rebuilt per game.
- **Hammer 2 editor.** Qt-based; the workflow is block-out, not BSP-brush. VR level editing (for Alyx) was a first-class target, not an afterthought.
- **Resource Compiler.** A single tool subsumes what was previously VBSP + VVIS + VRAD + model/material compilers. One asset pipeline, one DAG of dependencies, deterministic builds. This is the most underrated quality-of-life improvement in Source 2.
- **Legacy scripting still VScript/Squirrel.** Source 2's entity scripts remain Squirrel-based; Valve chose not to break the Dota 2 modding community by replacing the scripting VM even though modern alternatives exist. Pragmatism over purity.

## Frostbite

DICE's engine, born inside Battlefield, forced across all of EA after 2013. Technically brilliant, institutionally painful.

- **Frame Graph (Yuriy O'Donnell, GDC 2017).** The most influential rendering-architecture talk of the last decade: render passes are nodes in a DAG, resources are edges, and the engine topologically sorts, inserts barriers, allocates transient memory by lifetime, and culls dead passes. Nearly every modern engine — Unreal's RDG, Anvil's frame graph, countless indies — traces back to this talk.
- **Bindless-by-default materials.** Single descriptor table, per-draw material index — the same pattern id Tech later went all-in on. Used to drive Dragon Age: Inquisition's huge draw counts.
- **Async compute is load-bearing.** Johan Andersson's Frostbite work on Mantle/DX12 established async compute + async DMA as a Frostbite philosophy, not an opt-in.
- **Streaming + 64-player netcode.** Battlefield-scale simulation is the engine's north star; the streaming archive format and networking stack are tuned for that workload.
- **Terrible to adapt outside Battlefield.** BioWare's Mass Effect: Andromeda and Anthem are the cautionary tales. Frostbite at the time lacked basic RPG primitives (party inventory, conversation systems, third-person camera rigs) and BioWare rebuilt them on top of a shooter-shaped foundation. One developer's famous quote: "Frostbite is a Formula 1. When it does something well, it does it extremely well. When it doesn't do something, it really doesn't do something."
- **Mercury / Fabric ECS.** Frostbite's gameplay-entity layer evolved from a classic component model toward a chunk-based ECS (referred to internally by various code names including "Mercury" in some talks). Per-archetype chunked arrays, job-parallel systems, strongly typed components — same family as Unity DOTS and id Tech's entity model.

## CryEngine

Crytek's engine: historically the rendering pace-setter, now coasting on a long tail.

- **Crysis 1 (2007) popularised real-time SSAO and deferred shading** at a point when most shipping titles were still forward-only.
- **Light Propagation Volumes** (Kaplanyan, 2009) — an early real-time GI technique that fed later approaches.
- **SVOGI (Sparse Voxel Octree GI).** Voxelized scene representation built incrementally on CPU, thousands of rays per frame on GPU to gather occlusion and single-bounce indirect light. Budget on Xbox One was 3-4 ms GPU, 2-3 ms on mid PC. Fully dynamic — no bake, which was the whole point given Crysis's dynamic time of day.
- **Decline.** Post-2014 financial trouble at Crytek gutted the engine team. CryEngine is still source-available under a royalty license, but the velocity has shifted elsewhere (Amazon Lumberyard/O3DE forked from it and went their own way).

## Anvil (Ubisoft)

Ubisoft's Anvil (née Scimitar, 2007; AnvilNext 2012-2020) is the open-world workhorse across Ubisoft Montreal, Quebec, and Bordeaux.

- **Open-world streaming.** Anvil's defining competence: tile-based world streaming at multi-km scale with seamless loads.
- **Procedural content.** AnvilNext 2.0 generates urban structures from design templates — the reason AC Origins could ship its Egypt and AC Shadows can ship feudal Japan without hand-modelling every building.
- **Crowd simulation.** AC Unity famously pushes ~10,000 on-screen NPCs via aggressive AI LOD: ~40 high-fidelity agents running the full behaviour tree, the rest on cheap "extras" logic.
- **GPU-driven rendering.** Compute-based cluster culling + indirect draws (same family of technique as Frostbite and Decima). Dragon Age, AC Unity, R6 Siege are the usual reference implementations.
- **Shared tech across the Ubisoft studio network.** Anvil is one of a handful of engines (also Snowdrop, Dunia) reused across Ubisoft. Integration is coordinated, not just code-dropped.
- **Central tech stewardship.** Ubisoft maintains a central "La Forge" / core tech team that owns Anvil upgrades and pushes them across studios; each studio has an embedded engine team that merges and extends. This two-tier ownership model is rare and expensive but is the only way to keep 12+ studios on a shared codebase without fragmentation.
- **Frame graph adoption.** Anvil moved to a frame-graph rendering architecture post-2017, explicitly modeled on O'Donnell's Frostbite talk.

## Decima (Guerrilla)

Guerrilla's engine — shared with Kojima Productions for Death Stranding 1/2.

- **GPU-driven rendering.** Heavy use of indirect draw and compute culling; most scene state lives GPU-side.
- **Nubis volumetric clouds.** Andrew Schneider's SIGGRAPH 2015 talk, "The Real-time Volumetric Cloudscapes of Horizon: Zero Dawn," is the reference for modern volumetric cloud rendering. Ray-march through a domain shaped by Perlin-Worley noises, rendered in under 2 ms on PS4. The 2017 follow-up ("Nubis: Authoring Real-Time Volumetric Cloudscapes") covers the production tooling that turned the tech demo into an art pipeline.
- **Vegetation and world streaming.** The Horizon series runs dense foliage with instanced, GPU-culled draws; Decima's world streaming is tuned for continuous traversal, not corridor transitions.
- **Custom UI framework (UIPainter).** Decima ships its own UI stack rather than bolt on a third-party one — consistent with their tight-integration philosophy.
- **Shared across studios.** Kojima Productions used Decima for Death Stranding 1 and 2 via a direct engineering partnership with Guerrilla — one of the few cases of an in-house engine being licensed to another first-party studio. The collaboration meant Decima gained non-open-world use cases (cinematic cutscene authoring, third-person rigged gameplay) that Horizon alone would not have forced.

## Northlight (Remedy)

Northlight's pitch: a small-studio engine that punches at the AAA visual ceiling.

- **Physically-based, RT-first.** Control (2019) was a ray-tracing showcase title. Alan Wake 2 (2023) added full path-traced indirect lighting with DLSS 3.5 Ray Reconstruction for denoising.
- **Mesh shaders + sub-pixel occlusion culling.** Mesh-shader pipeline means everything in the scene can act as an occluder, with meshlet-level culling, not just mesh-level.
- **Modern ECS.** Remedy rewrote Northlight's game-object framework during Alan Wake 2 development into a data-oriented ECS with compile-time validation of memory access — the explicit goal was memory-efficient, safe parallel execution on variable core counts.
- **Tight art-engineering collaboration.** Remedy is a ~300-person studio; Northlight's feature set tracks what Remedy's art direction needs, not a generic "render anything" mandate.
- **Volumetric lighting.** Control's signature "foggy hotel corridor" look is volumetric froxel fog driven by the same voxel grid that samples direct and indirect light. Relatively cheap and visually distinctive; Alan Wake 2 extends this with ray-traced light shafts.
- **Developer-facing tools.** Remedy's `.decl`-style text authoring plus Dear ImGui-style in-engine panels — again, a sub-300-person studio cannot afford a Hammer 2 or a Creation Kit and does not pretend to.

## Creation Engine 2

Bethesda's Starfield engine. A cautionary outlier.

- **Deep ancestry.** Lineage traces back through Gamebryo (Morrowind, Oblivion). Creation Engine 2 is described by Todd Howard as a "new tech base" but retains the ESM/ESP/BSA data model that has shipped since Morrowind (2002).
- **Added to CE2:** real-time GI, advanced volumetrics, better post, Havok animation, radiant AI improvements, procedural generation (including lip-sync).
- **Moddability is the point.** Creation Kit (2024 for Starfield) is shipped publicly; mod archives use the same format as the engine. No other AAA studio is shipping their full authoring environment to users.
- **Aging architecture.** Save-state bloat, cell-based world format, scripting performance — the issues are structural. Former Bethesda engineers have publicly argued the studio should move to UE5. Bethesda's counter-argument is essentially "the mod community is a $X-billion moat, do not break it."
- **The lesson.** Creation Engine 2 is what "never rewrite" looks like after 24 years. It is instructive as a warning: an in-house engine that serves one IP family forever, with every decision constrained by backward compatibility, becomes impossible to modernize without breaking the community that justifies its existence. For a small team starting fresh, the lesson is to keep file formats rev-able, version every serialized struct, and resist the temptation to ship "stable forever" schemas.

## En qué son buenos (cross-engine strengths)

1. **Tight feedback loop artists ↔ programmers.** When the engine team sits 20 feet from the art lead, the correct feature ships. Frostbite's frame graph was scoped by what Battlefield needed; Nubis was scoped by what Horizon's skyboxes needed.
2. **Specialisation beats generality.** id Tech's corridor-combat 60 Hz guarantee, Decima's vegetation LODs, Anvil's crowd LOD — none of these would ship as default settings in a general-purpose engine.
3. **IP control.** Path tracing in id Tech 8 ships the week id decides it is ready. No engine vendor roadmap dependency.
4. **Data layouts owned end-to-end.** Bindless material tables, GPU-driven scene buffers, ECS chunk layouts — fine-tuned to the CPU/GPU ratio the game actually hits.
5. **Debug and profiling depth.** In-house engines carry deep first-party profilers (RAD Telemetry integrations, custom GPU captures) tuned to the game's hot paths.

## En qué fallan (cross-engine weaknesses)

1. **Cross-game reuse is brutal.** Frostbite → BioWare is the archetype. Engines built around one game's shape resist being bent to another's. Transplants cost multiples of licensing UE5 and shipping.
2. **Tooling lags.** Unreal's editor has had 20+ years of polish across thousands of teams. Most in-house editors are "good enough for us" rather than best-in-class — Anvil and Creation Kit are exceptions.
3. **Onboarding cost.** A new hire is productive in UE5 in weeks, in Frostbite or Anvil in months. Small studios feel this acutely.
4. **Bus factor.** Key engine architects (Tiago Sousa, Billy Khan, Johan Andersson, Andrew Schneider) are single points of failure. Their departure is a crisis.
5. **Platform ports are the studio's problem.** When a new console launches, the engine team has to do the work the engine vendor would normally do.
6. **Community and asset marketplace is zero.** No Unity Asset Store, no UE Marketplace. Everything is built or licensed one-off.

## Qué podríamos copiar para ALZE Engine

Concrete, small-team-achievable ideas:

1. **Frame Graph pattern (Frostbite / O'Donnell GDC 2017).** A DAG of render passes with declared resource reads/writes. The engine topologically sorts, inserts pipeline barriers automatically, and allocates transient resources by lifetime. For a ~30K LOC engine this is on the order of 800-1500 LOC including transient memory aliasing and is the single highest-leverage rendering refactor available. Implement in three stages: (a) pass declaration API, (b) barrier inference, (c) transient resource aliasing.
2. **id Tech-style data-driven text authoring.** No custom editor. Levels, entities, materials in plain text (TOML/JSON/custom `.decl`), with filesystem-watcher hot reload. Dear ImGui handles any in-engine debug UI. This avoids the "5-year tooling rewrite" trap and is exactly how a small team ships.
3. **Decima-style GPU-driven rendering.** Scene state in SSBOs, compute-based cluster culling (two-pass: frustum + occlusion against HZB), indirect draws, virtual texturing for the largest texture working sets. Start with compute culling of instances; graduate to meshlet culling only if mesh shaders are a mandatory target.
4. **Frostbite-style bindless materials.** One large descriptor table of textures (bound once per frame), a material SSBO keyed by material index, per-draw constants hold only a `material_id` + transform index. Collapses drawcall CPU cost and simplifies shader permutation explosion — a small team cannot afford a combinatorial shader cache.
5. **Source 2-style dynamic scene compilation.** At load time, walk static meshes with identical material, pack them into batched/merged vertex+index streams so the runtime issues one draw per "batch," not one per source mesh. Preserves author-time granularity while paying runtime cost of big batches. Straightforward C++; zero runtime dependency growth.
6. **Async compute for the overlap set.** Post-FX (bloom, AO, DoF), particle simulation, compute AO — all run on a second compute queue overlapping with graphics. Async compute in Vulkan is notoriously error-prone with barriers; use the frame graph from point 1 to generate the cross-queue sync for free.
7. **Job system as first-class citizen (id Tech / Frostbite).** Every long-running task — asset decompression, animation, physics step, culling, even renderer command generation — is a job. Fibre-based or std::thread-pool-based, not OS threads. For a 30K LOC engine a ~1K LOC fibre scheduler (see Naughty Dog's "Parallelizing the Naughty Dog Engine Using Fibers," GDC 2015 for the canonical design) is sufficient and dramatically simplifies reasoning about parallel work.
8. **Compile-time component validation (Northlight).** Components with statically-declared read/write intent, enforced by concept / type-traits in C++17/20. Systems that violate the declared access fail to compile. Costs one afternoon of template work, buys decades of concurrency correctness.
9. **Hot-reload-first authoring (id Tech).** Assume any text or shader file edited on disk is live-reloadable within one frame. Structure serializers to support incremental reload without full subsystem teardown — this is a discipline more than a feature, and it is the single biggest multiplier on iteration speed.

## Qué NO copiar

1. **Multi-year tooling rewrite cycles.** A small team cannot afford a Hammer-2-scale editor. Dear ImGui + text files + hot reload is the right scope.
2. **In-house UI framework.** Panorama, UIPainter, Scaleform-replacements — all cost-prohibitive. Use Dear ImGui for dev/debug UI and SDL + a small bespoke retained-mode layer for game UI, or license RmlUi / Noesis if needed.
3. **Custom streaming archive format.** Tempting but a trap. Use glTF (or its binary form) for scene data, KTX2 + Basis Universal for textures, and plain ZIP/asset-bundle layout for packaging. If compression is the bottleneck, drop in LZ4 or Zstd; do not design a new container.
4. **Custom physics engine.** Rubikon, Havok-replacements, even Frostbite's internal physics are decade-long investments. Integrate Jolt Physics (BSD, modern C++, used by Horizon Forbidden West).
5. **SVOGI / voxel GI / path tracing at launch.** These are 6-12 month AAA efforts and require a clean BVH + ray query layer. Ship with shadow maps + SSAO + probe-based GI first.
6. **Megatextures / virtual textures at v1.** Virtual texturing is a great R&D follow-up, not a v1 feature. Standard sparse textures + streaming mips is enough.
7. **Frostbite-style ambition on a single codebase.** Every studio that tried to force one engine across disparate genres (Frostbite, Creation Engine) paid dearly. Keep ALZE scoped to one game-shape.

## Fuentes consultadas

- Yuriy O'Donnell, "FrameGraph: Extensible Rendering Architecture in Frostbite," GDC 2017 — https://www.gdcvault.com/play/1024612/FrameGraph-Extensible-Rendering-Architecture-in
- Jean Geffroy, Yixin Wang, Axel Gneiting (id Software), "Rendering the Hellscape of Doom Eternal," SIGGRAPH 2020 — https://advances.realtimerendering.com/s2020/RenderingDoomEternal.pdf
- Simon Coenen, "DOOM Eternal — Graphics Study" — https://simoncoenen.com/blog/programming/graphics/DoomEternalStudy
- Adrian Courrèges, "DOOM (2016) — Graphics Study" — https://www.adriancourreges.com/blog/2016/09/09/doom-2016-graphics-study/
- Andrew Schneider (Guerrilla), "The Real-time Volumetric Cloudscapes of Horizon: Zero Dawn," SIGGRAPH 2015 — https://advances.realtimerendering.com/s2015/The%20Real-time%20Volumetric%20Cloudscapes%20of%20Horizon%20-%20Zero%20Dawn%20-%20ARTR.pdf
- Andrew Schneider, "Nubis: Authoring Real-Time Volumetric Cloudscapes with the Decima Engine," SIGGRAPH 2017 — https://advances.realtimerendering.com/s2017/Nubis%20-%20Authoring%20Realtime%20Volumetric%20Cloudscapes%20with%20the%20Decima%20Engine%20-%20Final%20.pdf
- Tiago Sousa (id Software), "Fast as Hell: idTech 8 Global Illumination," SIGGRAPH 2025 — https://advances.realtimerendering.com/s2025/content/SOUSA_SIGGRAPH_2025_Final.pdf
- NVIDIA Developer Blog, "How id Software Used Neural Rendering and Path Tracing in DOOM: The Dark Ages" — https://developer.nvidia.com/blog/how-id-software-used-neural-rendering-and-path-tracing-in-doom-the-dark-ages/
- Remedy Entertainment, "How Northlight makes Alan Wake 2 shine" — https://www.remedygames.com/article/how-northlight-makes-alan-wake-2-shine
- Remedy Entertainment, "Northlight" product page — https://www.remedygames.com/northlight
- Anton Kaplanyan, "Light Propagation Volumes in CryEngine 3," SIGGRAPH 2009 — https://www.advances.realtimerendering.com/s2009/Light_Propagation_Volumes.pdf
- Crytek, "Voxel-Based Global Illumination (SVOGI)" documentation — https://docs.cryengine.com/pages/viewpage.action?pageId=25535599
- Johan Andersson (DICE), "Parallel Graphics in Frostbite — Current & Future," SIGGRAPH 2009 — https://www.slideshare.net/repii/parallel-graphics-in-frostbite-current-future-siggraph-2009-1860503
- Valve Developer Community, "Source 2" / "Hammer (Source 2)" — https://developer.valvesoftware.com/wiki/Source_2
- Ubisoft, "Technology & Innovation — How We Make Games" (Anvil) — https://www.ubisoft.com/en-us/company/how-we-make-games/technology
- Wikipedia, "Ubisoft Anvil" — https://en.wikipedia.org/wiki/Ubisoft_Anvil
- Wikipedia, "id Tech 6" / "id Tech 7" — https://en.wikipedia.org/wiki/Id_Tech_7
- Wikipedia, "Creation Engine" — https://en.wikipedia.org/wiki/Creation_Engine
- Wikipedia, "Frostbite (game engine)" — https://en.wikipedia.org/wiki/Frostbite_(game_engine)
- PCGamesN, "Mass Effect Andromeda — Frostbite problems" — https://www.pcgamesn.com/mass-effect-andromeda/mass-effect-andromeda-frostbite-problems
- 80.lv, "Using id Tech 7: Doom Eternal Analysis" — https://80.lv/articles/using-id-tech-7-doom-eternal-analysis
- 80.lv, "UIPainter: Custom UI Framework In Decima Engine" — https://80.lv/articles/learn-how-guerrilla-games-decima-engine-handles-ui-rendering
- Bethesda / Starfield DB, "Creation Engine 2 Features" — https://www.starfielddb.com/creation-engine-2/
- Vulkan Guide, "GPU Driven Rendering Overview" — https://vkguide.dev/docs/gpudriven/gpu_driven_engines/
- Alex Tardif, "Bindless Rendering" — https://alextardif.com/Bindless.html
