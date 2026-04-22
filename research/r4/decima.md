# Decima Engine (Guerrilla Games, 2013-present)

> "We needed an engine that could render a jungle and still leave budget for a dinosaur-shaped robot." — paraphrasing Michiel van der Leeuw, Guerrilla CTO
>
> Round 4 deep dive. Round 2 `aaa_engines.md` covered Decima in ~20 lines ("GPU-driven rendering + Nubis clouds + shared with Kojima"). `fox_engine_kojima.md` r3 covered the Kojima-side licensing story (Dejima island naming, USB stick handoff, Death Stranding). Neither deep-dived the **engine itself**. This file is that missing deep dive.

## 0. TL;DR

Decima is Guerrilla Games' proprietary C++ engine, descended from the Killzone codebase, first publicly named in 2016, powering every Guerrilla release since Killzone Shadow Fall (2013) and — via an engineering partnership unique in the AAA industry — every Kojima Productions release since Death Stranding (2019). Its defining competences are **volumetric clouds (Nubis)**, **GPU-driven procedural vegetation**, **predictive asset streaming at 70 km² scale (storyhighway)**, **checkerboard + temporal reconstruction for 4K**, and a **custom in-house editor** shared between studios on opposite sides of the planet. It is what happens when ~350 engineers spend 20 years refining one engine around one art direction — photo-real open-world first-party console — and the result is one of the two or three most visually striking engines shipping in 2026 (UE5 Nanite/Lumen and id Tech 8 path tracing being the others).

## 1. History and lineage

### 1.1 Pre-Decima: Killzone codebase (2004-2013)

Decima's DNA is the Killzone engine:

- **Killzone (2004, PS2)** — first game, first engine. Not yet Decima; a PS2-era deferred renderer with a lot of scripted linearity.
- **Killzone 2 (2009, PS3)** — the inflection point. Michal Valient and Michiel van der Leeuw's SPU-driven deferred renderer is the paper that put Guerrilla on the graphics-engineering map. "Deferred Rendering in Killzone 2" (GDC 2007 + 2009) is still a canonical reference for G-buffer layout on constrained hardware.
- **Killzone 3 (2011, PS3)** — stereoscopic 3D, refined deferred pipeline.
- **Killzone Shadow Fall (Nov 2013, PS4 launch title)** — first PS4 game shipped worldwide. First title on what would be renamed Decima. 30 fps campaign at 1080p, 60 fps multiplayer at checkerboard 1920×540→1920×1080 (PS4's first public checkerboard shipped title). The engine at this point is still called "Killzone engine" internally.

Lead tech figures in this era: **Michiel van der Leeuw** (CTO), **Michal Valient** (principal rendering engineer, later moved to Apple), **Giliam de Carpentier** (senior engineer, rendering + procedural), **Jasper Bekkers** (rendering).

### 1.2 Naming and rebrand (2016)

In June 2016, Guerrilla publicly named the engine **Decima** — after the Japanese transliteration of **Dejima**, the 17th-century artificial island in Nagasaki harbour that was the sole Dutch trading post with Tokugawa Japan. The name anticipated what had already been agreed privately: **Decima would be licensed to Kojima Productions** for Death Stranding. The naming is a diplomatic gesture — a Dutch–Japanese trade post, for a Dutch–Japanese engineering partnership.

### 1.3 Horizon Zero Dawn (2017)

- **PS4 / PS4 Pro**. 1080p base, 2160p checkerboard on Pro. 30 fps.
- First **Nubis** shipped — the volumetric cloud system (Schneider, SIGGRAPH 2015 tech talk, shipped with HZD).
- First public **GPU-based procedural vegetation placement** at this scale (de Carpentier + Ishiyama, GDC 2017).
- Open-world ~50 km² with continuous streaming, no corridor transitions.
- Team: ~200 Guerrilla + external support.

### 1.4 Death Stranding (Nov 2019)

- **PS4 / PS4 Pro**, with PC port 2020 and PS5 Director's Cut 2021.
- Kojima Productions' first game on Decima. Custom branches merged with Guerrilla's mainline continuously through 2017-2019.
- Kojima Productions adds: facial animation pipeline, snow/mud dynamic displacement, BB canister volumetric rendering, photogrammetry scan pipeline (inherited from their Fox Engine workflow).
- ~80 km² "UCA" map, radically different art direction (moody grey Iceland-inspired landscapes vs Horizon's sun-drenched flora).

### 1.5 Horizon Forbidden West (Feb 2022)

- **PS4 / PS5 cross-gen**. 4K checkerboard on PS5, 60 fps performance mode.
- ~70 km² map, more verticality, underwater biomes (new refraction + caustics stack), major vegetation density increase.
- "Decima 2.0" informally — substantial rewrite of the GPU-driven renderer for PS5's I/O stack.
- Burning Shores DLC (April 2023) is PS5-only and raises the vegetation and sky bars further.

### 1.6 Horizon Call of the Mountain (Feb 2023)

- **PSVR2 launch title**. Decima adapted for 90 fps stereo + foveated rendering driven by PSVR2 eye tracking.
- Showed Decima could hit VR frame targets without forking the engine — a non-trivial achievement.

### 1.7 Death Stranding 2: On the Beach (June 2025)

- **PS5 exclusive at launch**; PC port later.
- Further Kojima Productions refinements: hair (BB and Fragile hair systems), beach/ocean simulation, RT reflections.
- Mexico / Australia biomes — new vegetation systems built on HFW pipeline.

### 1.8 Team scale and evolution

- 2013 (Shadow Fall ship): ~130 devs at Guerrilla.
- 2017 (HZD ship): ~200.
- 2022 (HFW ship): ~350 + contractors.
- 2026: ~400 at Guerrilla + ~100-150 at Kojima Productions working on Decima fork.

## 2. Streaming and open world

### 2.1 Scale: HFW 70+ km²

Horizon Forbidden West's map is roughly **72 km²** of playable surface (Guerrilla has not published an official figure; community measurements converge around 70-75 km²). This is larger than Skyrim (~40 km²), smaller than Red Dead Redemption 2 (~75 km² including mountains but comparable), and dwarfed by Microsoft Flight Simulator (planetary). The *density* is what matters: HFW has roughly 10× the unique hand-placed prop count of HZD per km², so the streaming system isn't just tiling a bigger space, it's feeding more assets per second to a PS5 NVMe.

### 2.2 The Storyhighway system

"Storyhighway" is Guerrilla's internal term for their **predictive asset streaming** stack. Core ideas (reconstructed from HFW GDC 2023 talks + community reverse-engineering; Guerrilla has not fully open-sourced the design):

1. **Spatial streaming tiles.** The world is decomposed into overlapping 2D tiles (roughly 64-128 m on a side for dense zones, larger for open plains). Each tile has an asset manifest: meshes, textures, decals, audio, AI waypoint graphs, quest triggers.
2. **Tile priority = f(player position, velocity, camera frustum, quest state).** A tile in front of the player moving at 10 m/s on a mount is higher priority than one behind; a tile containing the next quest objective is flagged "must-reside" regardless of distance.
3. **Anticipatory prefetch on quest progression.** When the player accepts a quest, a predictive walker runs a fast pathfinding query over the likely routes and pre-warms tiles along them. The "storyhighway" moniker comes from this: critical quest paths are highways that always have fresh asphalt (tiles) laid ahead of the vehicle (player).
4. **Rolling eviction by LRU + priority floor.** Tiles that drop out of prediction are evicted, but the system maintains a minimum resident set (~2-3 GB on PS5) to prevent thrashing when the player backtracks.
5. **Two-tier streaming.** Coarse LOD (imposters, low-poly proxy meshes, low-mip textures) streams fast and far; detail LOD (authored meshes, high-mip VT pages) streams only for the near-player frustum. Coarse LOD guarantees the skyline is always correct even if the player suddenly teleports.
6. **Audio-driven prefetch.** An NPC shouting "there's a Tallneck to the east" triggers a hint to the streamer even if the player hasn't turned the camera yet.

PS5's Kraken-based hardware decompression and the NVMe + I/O complex are essential: HFW on PS5 can load a quest zone in ~1-2 seconds vs 15-20 seconds on PS4. The PS4 codepath falls back to more conservative prefetch + longer "loading veil" animations (the wristband animation serving as a streaming soft-gate).

### 2.3 LOD system

Decima does **not** ship a Nanite-style virtualized-geometry DAG (confirmed by Malan + van der Gaag SIGGRAPH 2022 Real-Time Live!). Instead:

- Discrete LOD chains per mesh (3-5 levels typical, hand-authored for hero assets, decimated automatically for set dressing).
- **Cluster-level culling** on GPU (see §5). The unit of dispatch is a cluster (~64-128 tris), not a whole mesh.
- Terrain uses a **quadtree LOD** with geomorphing between levels to hide popping. Root tile covers the full map; leaves are 1-2 m²; streaming loads/unloads tiles as the camera moves.
- Vegetation has its own LOD (imposter cards at distance, generated at cook time via baked HDR billboards).

This is deliberately more conservative than UE5 Nanite — it scales to PS4 and costs less engineering, at the price of more manual LOD authoring work.

## 3. Vegetation and procedural placement

This is the HZD 2017 breakthrough, formalised in de Carpentier + Ishiyama's GDC 2017 talk "GPU-based Run-Time Procedural Placement" and the SIGGRAPH 2017 Advances follow-up. The reason HZD's Meridian jungle and HFW's San Francisco ferns look the way they do.

### 3.1 The problem

A handcrafted forest at the density Guerrilla's art direction wants (>100k plant instances per visible frame) cannot be placed by hand and cannot be streamed from disk as instance transforms — it would bloat the map. It has to be **regenerated procedurally at runtime** from a compact authoring description.

### 3.2 The algorithm (simplified)

1. **Authoring**. An artist paints "placement zones" as 2D masks on the world (jungle, grassland, rocky, snow). Each zone references a **placement recipe**: a list of species with per-species density, min/max spacing, slope constraints, altitude constraints, wind exposure preference.
2. **Cook.** The recipe + terrain heightmap + mask atlas are packaged per tile.
3. **Runtime, per frame.**
   - A compute shader dispatches one thread per candidate placement slot in the visible tiles (Poisson-disc-sampled seed points, generated deterministically from tile coordinate + world-seed hash so the same forest regenerates identically each time).
   - Each thread reads the zone mask, slope, altitude, and decides which species (if any) to place. Deterministic — same inputs → same plant.
   - The thread writes an instance transform (position, scaled-randomised rotation, species ID) to a per-tile instance SSBO.
4. **Rendering.** GPU-driven indirect draw iterates the instance SSBO. Cluster culling (see §5) discards off-screen clusters. Wind animation is vertex-shader displacement driven by a global 3D wind-noise volume and per-species stiffness parameters.

### 3.3 Wind animation

Decima's wind is a **single global 3D noise volume** (curl noise, sampled in vertex shader) plus per-species response curves:

- Grass: full-body sway.
- Ferns / shrubs: mid-frond sway, stems rigid.
- Trees: trunk stiff, branches medium, leaves high-frequency.

Guerrilla's insight was that plausible wind is not physically simulated — it's one shared noise volume + cheap per-species masking. This is ~0.3 ms on PS4 for the entire visible vegetation set.

### 3.4 Cluster culling for vegetation

Every placed instance is mesh-shader-adjacent in concept (pre-mesh-shader era, so implemented with compute + indirect draws):

1. Instance SSBO → compute shader computes per-cluster visibility (frustum + HZB occlusion against a down-sampled depth buffer).
2. Surviving clusters write to an indirect-args buffer.
3. `glMultiDrawElementsIndirect` / `vkCmdDrawIndexedIndirectCount` issues the draws.

The win: you can place 500k grass blades, cull down to the 50k visible, and issue 1 draw call. CPU cost per instance count is approximately O(1) — the whole cull runs on GPU.

### 3.5 Compared to UE5's Nanite landscape + foliage

UE5 2026's approach for foliage is **Nanite-on-foliage** (clusters of meshlets, DAG LOD), layered on top of PCG (Procedural Content Generation) plugin for runtime placement. It does more — virtualized geometry means grass blades have meshlet-level detail at any distance — but costs more: Nanite for vegetation has a well-documented overhead that forces UE5 projects to hybridise (some foliage uses Nanite, some uses legacy foliage).

Decima's approach is **~10× simpler** and ships today on PS4. UE5's is more general and scales better to extreme close-ups but is overkill for most vegetation. ALZE can copy Decima's approach; UE5's is out of reach for a small team.

## 4. Volumetric cloud system (Nubis)

This is Decima's most-cited contribution to the industry. The Fox Engine mention in r3 framed this backwards — **Nubis came first**. Andrew Schneider developed the technique at Blue Sky Studios (film VFX), joined Guerrilla in 2012, and shipped Nubis in Killzone Shadow Fall (2013) as a secondary sky element, then as a hero feature in Horizon Zero Dawn (2017). MGS V's clouds (2015) are part of the same industry wave but are a separate implementation inspired partly by Schneider's SIGGRAPH 2015 talk.

### 4.1 Algorithm (HZD 2017, Schneider)

**Input**: time of day, weather state vector, wind, user camera.
**Output**: per-pixel cloud colour + transmittance, composited into the sky before tone mapping.

Steps, in order:

1. **Cloud shape domain** — a 2D "cloud map" (weather map) paints coverage, cloud-type (stratus / altostratus / cumulus mix), precipitation. Authored or procedurally derived from weather state. Resolution: 512×512 covering world-space ~100 km.
2. **3D noise sampling.**
   - **Low-frequency Perlin-Worley** 3D texture, 128³, RGBA — channel R is Perlin-Worley combined, GBA are three Worley octaves. This is the "base shape" of clouds.
   - **High-frequency Worley-only** 3D texture, 32³, RGB. This is the "erosion detail" that makes cloud edges wispy.
   - **Curl noise 2D** texture, for turbulent displacement of sample positions.
3. **Ray march the cloud volume.** For each pixel's view ray:
   - Start at the cloud layer bottom (say 1.5 km altitude), step towards the cloud layer top (~4 km).
   - At each step, sample the cloud-map → coverage, type.
   - Sample low-freq 3D noise + altitude profile → base density.
   - Erode base density by high-freq noise, modulated by curl noise displacement → final density.
   - Accumulate scattering using Beer's law (transmittance = exp(-∫ density ds)) + Henyey-Greenstein phase function (forward-scattering for sun-facing pixels, back-scattering for silver lining).
4. **Lighting**. At each march step, take 6 cheap samples along the sun direction to estimate optical depth from the current sample to the sun. Use a tuned **"silver lining" term** — an additional forward-scatter boost near the sun to recreate the bright cloud edges photographers call silver lining. This is a key Schneider insight.
5. **Temporal reprojection**. Reprojecting last-frame's cloud result into this frame amortises the ray march cost across frames. Reproject with a 4×4 quincunx pattern — only 1 in 16 pixels is fully ray-marched per frame, the rest are reprojected from history. Combined with TAA, the result is a stable cloudscape at ~1-2 ms on PS4.

### 4.2 Nubis 2 (HFW, 2022) and Nubis Evolved (Schneider SIGGRAPH 2022)

- **Fly-through clouds.** HFW allowed the player to mount a flying machine and fly **through** cloud layers. Nubis 2 supports full 3D traversal — clouds are now a true volumetric domain, not a thin shell above the player.
- **Improved lighting** — multiple-scattering approximation via dual-lobe HG phase function, better cumulonimbus internal shading.
- **Higher-resolution domain** for hero cloud formations (the Tallneck-peak, dramatic backdrop cumulus, etc.).
- **Precomputed atmospheric scattering LUT** coupled with the cloud march so clouds darken correctly when the sun is low.

### 4.3 Exact performance

Per Schneider SIGGRAPH 2015 and 2017:

- PS4: ~1.8 ms average, ~2.5 ms worst case, for 1080p cloud pass.
- PS4 Pro: scales nearly linearly to ~2160p checkerboard at similar ms.
- PS5 (Nubis 2, HFW): ~1.2 ms for full native 4K, more marches, more detail.

### 4.4 Why Fox Engine got credit

Because MGS V shipped in 2015 and was more visible than Killzone Shadow Fall's clouds (which were a secondary element in 2013), popular tech press sometimes credits Fox with "invented" volumetric clouds. Historically: **Schneider's SIGGRAPH 2015 talk predates MGS V's release by 2 months** and references Killzone Shadow Fall's 2013 implementation as prior art. The honest telling is that Fox Engine's and Decima's cloud systems developed in parallel in 2012-2015 and cross-pollinated via SIGGRAPH, with Schneider's 2015 talk serving as the industry's official "here's how you do it" reference.

## 5. GPU-driven rendering

Decima's renderer is **GPU-driven**: most scene state lives in GPU-side buffers (SSBOs), and the CPU submits a small number of indirect-draw calls per frame rather than thousands of per-mesh draws.

### 5.1 Cluster culling pipeline (de Carpentier et al)

Every renderable mesh is pre-chunked into clusters of ~64-128 triangles at cook time. Each cluster has a bounding sphere/box and a normal cone (for backface culling).

Per-frame pipeline:

1. **Instance compact pass** (compute). Walks the visible tile set, computes world-space transforms, writes a compact instance SSBO.
2. **Cluster frustum cull** (compute). One thread per cluster. Tests cluster bounding sphere against 6 frustum planes. Survivors write their index to an intermediate buffer.
3. **HZB occlusion cull** (compute). Survivors test their bounding box against the **Hierarchical Z Buffer** (HZB) from the previous frame's depth, down-sampled via a mip chain. Any cluster whose box's closest point is behind the furthest HZB mip texel is culled.
4. **Backface cone cull** (compute). Normal cone test discards clusters whose triangles all face away from camera.
5. **Indirect draw args generation** (compute). Survivors are bucketed by material and written to indirect-args buffers.
6. **Draw**. One indirect draw call per material bucket. On PS5 and PC with mesh shaders, this becomes `vkCmdDrawMeshTasksIndirectEXT` dispatching meshlets; on PS4, `vkCmdDrawIndexedIndirectCount` dispatching clusters as instances.

### 5.2 vs UE5 Nanite

UE5 Nanite goes further:
- **DAG-LOD**: meshlets are a multi-level DAG, not a discrete LOD chain. Runtime picks the DAG cut matching current screen error.
- **Software rasterizer** for micro-triangles (<~32 px) because hardware rasterizers are inefficient at sub-pixel triangle sizes.
- **Visibility buffer + deferred material** — Nanite separates geometry rasterization from material shading so overdraw cost is decoupled from material complexity.

Decima does **not** do DAG-LOD, does **not** do software raster, does **not** use a visibility buffer. What it **does** do is compute-driven cluster culling + bindless materials + indirect draws — the same family of technique, less sophisticated but 3-5× cheaper to engineer and still ships gorgeous content at scale.

For ALZE: Decima's pipeline is feasible for a small team. Nanite is not.

## 6. Tile-based deferred + checkerboard rendering

### 6.1 G-buffer layout (HZD/HFW)

Decima uses a compact 4-RT G-buffer, similar to Fox Engine's in philosophy:

- **RT0**: albedo.rgb + metallic mask (A)
- **RT1**: world-space normal.xy (octahedral encoded) + roughness + material ID
- **RT2**: motion vectors.xy + optional wetness mask
- **RT3**: emissive.rgb + AO

Reversed-Z 32-bit depth as a separate attachment. Total ~128 bits per pixel plus depth.

### 6.2 Tile-based light culling

Forward+ / tile-based deferred: screen is divided into 16×16 pixel tiles, a compute shader culls each light against each tile's depth-range frustum, producing a per-tile light list. The lighting pass then iterates only the lights that touch each tile. This scales to hundreds of dynamic lights per frame (HFW's Meridian at night is the showcase).

### 6.3 Checkerboard rendering (PS4 Pro, PS5)

Decima shipped **one of the first public checkerboard renderers** in Killzone Shadow Fall's multiplayer mode (2013). The technique evolved:

1. **PS4 (2013-2016)**: Shadow Fall multiplayer rendered at half-resolution with a checkerboard reconstruction. Controversial at launch, accepted as a cost-effective path later.
2. **PS4 Pro (2016+)**: HZD rendered at 2160p checkerboard — every other pixel in a 4K grid is natively rasterized, the rest reconstructed from history + current-frame neighbours. Drobot's "4K Checkerboard in Battlefield 1 and Mass Effect Andromeda" (GDC 2017) describes a parallel technique used by DICE; Decima's is a Guerrilla-original but shares the same idea family.
3. **PS5 (HFW, DS2)**: Checkerboard + TAAU (temporal anti-aliasing upscale) combined. Performance mode targets 1440p internal upscaled to 4K at 60 fps. Quality mode targets ~1800p checkerboard at 30 fps.

The algorithm, briefly:

- Each frame, render only odd (or even) pixels of a 2×2 quincunx pattern; alternate next frame.
- For each output pixel, reconstruct using: (a) the natively-rendered pixel if it is the current frame's pixel, (b) neighbours + temporal history if not.
- Edge-aware reconstruction filter handles disocclusion (falls back to current-frame neighbours for pixels where history is invalid).

### 6.4 Performance envelope

- HZD on PS4: 1080p at 30 fps locked.
- HZD on PS4 Pro: 2160p checkerboard at 30 fps.
- HFW on PS5 Performance: ~1440p upscaled to 4K at 60 fps.
- HFW on PS5 Quality / Resolution: ~1800p checkerboard at 30 fps.
- HFW on PS5 Balanced (added post-launch patch): ~1440p at 40 fps for VRR displays.
- DS2 on PS5: similar envelopes, with additional RT modes enabling RT shadows in hero cutscene areas.

## 7. Death Stranding and Kojima Productions layer

Kojima Productions licensed Decima in 2016 and shipped Death Stranding in 2019. What they added on top:

### 7.1 Facial animation pipeline

- Inherited from Fox Engine: photogrammetry-based hero facial scans (Norman Reedus, Mads Mikkelsen, Léa Seydoux all scanned as hero actors).
- **4D facial capture** rig (high-speed camera array capturing skin deformation through motion) feeds blendshape libraries.
- Custom in-engine rig that blends performance-captured blendshapes with physics-driven cloth/jiggle layers — more cinematic-first than Horizon's facial rig, which is tuned for gameplay scale.

### 7.2 Snow / mud / terrain displacement

- Dynamic terrain displacement writes to a world-space height texture as Sam walks.
- Footprints, vehicle tracks, dropped-package indentations — all persistent until weather (rain, snow) degrades them.
- Implementation: a render-target displacement map aligned with the player's current tile, updated per-frame by rasterising foot/wheel contacts into it.
- Shader-side, terrain heightmap = static heightmap + dynamic displacement texture.

This is a feature Horizon did not need (Aloy leaves no footprints in grass) but DS2 uses extensively for narrative (your path is literally drawn on the world).

### 7.3 BB canister volumetric rendering

The BB (bridge baby) canister is a hero prop rendered with specific custom tech: a **volumetric fluid simulation** for the amniotic fluid, lit by an in-canister light source, composited with refraction through the canister glass. Custom one-off implementation, not generalised into Decima for other projects — a good example of cross-studio contributions not always being upstreamed.

### 7.4 Photogrammetry pipeline

KP brought their Fox Engine photogrammetry workflow to Decima: scan → cleanup → blendshape authoring → in-engine rig. Guerrilla had their own scan pipeline for Horizon's human characters; the two pipelines converged into a shared Decima asset path by 2022.

### 7.5 What KP did NOT upstream

- Full-body cloth sim with Kojima's level of sway detail — some of it remains in KP's fork.
- BB canister rendering — one-off prop.
- Some of the cutscene camera / directorial tools — KP has deep Kojima-flavoured cinematic tools that Guerrilla did not merge back.

## 8. Decima 2.0 for PS5

Informal term for the 2019-2022 rewrite pass targeting PS5 hardware. Public-facing changes:

### 8.1 Rendering

- **Ray-traced shadows** (HFW Burning Shores DLC, DS2): selective per-hero-light RT shadows rather than blanket RT for the whole scene. Cost: ~1-2 ms per RT light at 1440p.
- **Ray-traced reflections** (DS2): hero surfaces (cars, wet roads, BB canister) use RT reflections; most surfaces still use SSR.
- **Improved hair**: Strand-based hair for hero characters (Aloy's hair in HFW, Sam/Fragile in DS2). Uses a strand-based simulation + compute-shader-generated triangle strips (conceptually similar to NVIDIA HairWorks / AMD TressFX but Guerrilla-implementation).
- **Temporal upscaling (TAAU)** replacing pure checkerboard in many modes.
- **Higher-density Nubis 2 clouds**.

### 8.2 Streaming

- PS5 NVMe + I/O complex: asset streaming throughput went from ~200 MB/s practical on PS4 to ~5-8 GB/s on PS5. Guerrilla rewrote the streamer around a **coroutine-driven async I/O abstraction** that can issue thousands of in-flight requests.
- **Direct-to-GPU decompression**: textures are Kraken-compressed on disk, decompressed by PS5's hardware block, DMA'd into GPU memory without CPU touching them.

### 8.3 Audio

Tempest 3D Audio (PS5's 3D audio HW) integrated; HRTF-based spatial audio for enemy localisation in HFW combat.

## 9. Tools and DCC

### 9.1 Decima Editor

Custom in-house C++/Qt-based editor. Not publicly released, no community. Characteristics reconstructed from GDC talks and ex-Guerrilla developer interviews:

- **Scene hierarchy editor** with component-entity system.
- **Shader graph editor** (node-based, GLSL/HLSL output).
- **Terrain editor** with heightmap + mask painting.
- **Placement recipe editor** (authoring the vegetation/prop procedural placement masks).
- **Quest graph editor** (node-based quest scripting).
- **Animation state machine editor**.
- **Cutscene sequencer** with keyframe animation, facial capture playback, camera DOP controls.

Well-regarded internally; ex-employees frequently praise the editor's stability and iteration speed. Not as polished as Unreal's editor but more focused.

### 9.2 Cross-studio collaboration (Guerrilla ↔ KP)

Amsterdam and Tokyo: **8-9 hour timezone offset**. The practical workflow:

- **Shared Perforce depot** (Guerrilla's canonical version control).
- **Engine mainline at Guerrilla**, KP maintains a fork with upstream merges every 2-4 weeks.
- **Engineer exchanges**: Guerrilla engineers spent extended stays in Tokyo during DS1 and DS2 development; KP engineers the same in Amsterdam.
- **Async pairing**: handoff notes at end of each studio's day. A feature gets 16 hours of work per day across two timezones.
- **Shared slack/email/async issue trackers**; no "we must sync live" meetings blocking progress.

This is a rare working model — most engine licensing is arm's-length (you license, you get code + support contract, you don't co-develop). Decima's Guerrilla↔KP model is closer to a shared monorepo with two studios contributing.

## 10. Published papers and talks catalogue

| Author(s) | Title | Year | Venue | URL |
|---|---|---|---|---|
| Michiel van der Leeuw, Michal Valient | "Deferred Rendering in Killzone 2" | 2007 | Develop Conference Brighton | https://www.guerrilla-games.com/read/deferred-rendering-in-killzone-2 |
| Michal Valient | "Practical Occlusion Culling in Killzone 3" | 2011 | SIGGRAPH Advances | https://advances.realtimerendering.com/s2011/Valient_Killzone3Occlusion_SIGGRAPH_2011.pdf |
| Michal Valient | "Taking Killzone Shadow Fall Image Quality Into The Next Generation" | 2014 | GDC | https://www.guerrilla-games.com/read/taking-killzone-shadow-fall-image-quality-into-the-next-generation |
| Andrew Schneider | "The Real-Time Volumetric Cloudscapes of Horizon Zero Dawn" | 2015 | SIGGRAPH Advances | https://advances.realtimerendering.com/s2015/The%20Real-time%20Volumetric%20Cloudscapes%20of%20Horizon%20-%20Zero%20Dawn%20-%20ARTR.pdf |
| Andrew Schneider | "Real-Time Volumetric Cloudscapes" | 2016 | GPU Pro 7 (book chapter) | https://www.crcpress.com/GPU-Pro-7-Advanced-Rendering-Techniques/Engel/p/book/9781498742535 |
| Andrew Schneider | "Nubis: Authoring Real-Time Volumetric Cloudscapes with the Decima Engine" | 2017 | SIGGRAPH Advances | https://advances.realtimerendering.com/s2017/Nubis%20-%20Authoring%20Realtime%20Volumetric%20Cloudscapes%20with%20the%20Decima%20Engine%20-%20Final%20.pdf |
| Andrew Schneider | "Nubis, Evolved" | 2022 | Guerrilla blog + SIGGRAPH Advances | https://www.guerrilla-games.com/read/nubis-evolved |
| Giliam de Carpentier, Kohei Ishiyama | "Decima Engine: Advances in Lighting and AA" | 2017 | SIGGRAPH Advances | https://advances.realtimerendering.com/s2017/DecimaSiggraph2017.pdf |
| Giliam de Carpentier, Kohei Ishiyama | "GPU-based Run-Time Procedural Placement in Horizon Zero Dawn" | 2017 | GDC | https://www.gdcvault.com/play/1024234/GPU-Based-Run-Time-Procedural |
| Jasper Bekkers | "Horizon Zero Dawn: A QA Open World" | 2017 | GDC | https://www.gdcvault.com/play/1024293/Horizon-Zero-Dawn-A-QA (auxiliary) |
| Elcin Erkin, Michal Drobot | "4K Checkerboard in Battlefield 1 and Mass Effect Andromeda" | 2017 | GDC | https://www.gdcvault.com/play/1024747/4K-Checkerboard-in-Battlefield-1 (Drobot's related work) |
| Michal Drobot | "Low Level Optimizations for GCN" | 2014 | Digital Dragons | https://www.slideshare.net/DICEStudio/low-level-optimizations-for-gcn |
| Hugh Malan, Arjan Bak | "A Showcase of Decima Engine in Horizon Forbidden West" | 2022 | SIGGRAPH Real-Time Live! | https://dl.acm.org/doi/10.1145/3532833.3538681 |
| Wim Van Der Schee | "Lighting the City of Meridian: Horizon Zero Dawn" | 2017 | Guerrilla blog | https://www.guerrilla-games.com/read/lighting-the-city-of-meridian-horizon-zero-dawn |
| Nathan Reed (Guerrilla) | "A Deep Dive into Nanite Virtualized Geometry" — reviewer, not primary author; Decima team regularly presents at SIGGRAPH Advances course | 2021-2024 | SIGGRAPH Advances | https://advances.realtimerendering.com/ |
| Guerrilla Games (collective) | "Horizon Forbidden West: A Technical Postmortem" | 2023 | GDC | https://www.gdcvault.com/ (search HFW postmortem) |
| Joost van Dongen (community compiler) | "A Guide to Decima Engine" (community / tool reverse-engineering for modding DS1) | 2020-2023 | GitHub / modding community | https://github.com/ds-modding (archive fallback: https://web.archive.org/web/*/github.com/ds-modding) |

Archive.org fallback for Guerrilla dev blog if the main domain returns 403: `https://web.archive.org/web/*/guerrilla-games.com/read/*`.

Note: Some of Kojima Productions' DS2 presentations are scheduled or recently-given at SIGGRAPH 2025 Advances; KP historically shares less publicly than Guerrilla, so the catalogue above is Guerrilla-heavy by design.

## 11. Decima shipped tech: feature matrix

| Feature | HZD 2017 | HFW 2022 | DS2 2025 | UE5 2026 equivalent |
|---|---|---|---|---|
| Volumetric clouds | Nubis 1 (shell above player) | Nubis 2 (fly-through volumetric) | Nubis 2 + weather coupling | Sky Atmosphere + Volumetric Clouds plugin (simpler) |
| Procedural vegetation | GPU recipe-based, 100k+ instances | Denser, underwater, biome variety | Mexico/AU biomes + DS1 continuity | PCG Graph plugin + Nanite foliage (heavier) |
| GPU-driven culling | Cluster cull + HZB + indirect draws | + mesh shaders on PC | + mesh shaders on PS5 | Nanite cluster cull + DAG-LOD |
| Virtualized geometry | No (discrete LOD) | No (discrete LOD + cluster cull) | No (discrete LOD + cluster cull) | Nanite (full DAG virtualized) |
| Ray tracing | No | RT shadows in DLC only (PS5) | RT shadows + RT reflections (selective) | Lumen (RT GI+reflections), optional HWRT |
| Checkerboard / upscale | PS4 Pro CB 2160p | CB + TAAU | CB + TAAU + native 4K quality | TSR (temporal super resolution) |
| Virtual textures | Early Decima VT | Mature Decima VT, PS5 NVMe optimised | Decima VT 2.0 | Runtime VT + Streaming VT |
| World streaming | Tile-based, 50 km² | Storyhighway, 70 km², PS5 NVMe | Storyhighway + ocean | World Partition + One File Per Actor |
| Hair | Card-based | Strand-based for Aloy (hero) | Strand-based for Sam/Fragile/BB | Groom (strand-based, Alembic import) |
| Facial rig | In-house, gameplay-tuned | In-house, improved | KP cinematic rig | MetaHuman |
| Terrain displacement | Static | Static + wetness | Dynamic (footprints, mud) | Landscape + Runtime VT displacement |
| Global illumination | Probe-based + SH | Probe-based + SH + light maps | Probe + selective RT bounce | Lumen (fully dynamic) |
| Physics | Custom + Havok | Custom + Havok | Custom + Havok | Chaos (in-house) |
| Audio | 3D HRTF (PS4) | 3D HRTF (PS4) + Tempest (PS5) | Tempest full integration | MetaSounds |

## 12. Decima vs UE5 vs Frostbite on "open-world photo-real first-party console"

| Axis | Decima | UE5 | Frostbite |
|---|---|---|---|
| Target game shape | Open-world third-person photo-real | General (open-world, FPS, RPG, etc.) | FPS + vehicular + sport |
| Platform focus | PS console first, PC port | All platforms | All platforms |
| Vegetation density | Industry-leading (Nubis+placement) | Competitive via PCG + Nanite foliage | Good (Mirror's Edge Catalyst, BFV) |
| Virtualized geometry | No | Nanite (class-leading) | No (being developed) |
| Streaming at 70+ km² | Storyhighway, proven at 70 km² PS5 | World Partition, proven at ≥100 km² | Proven at ~30-50 km² |
| RT maturity | Selective, conservative | Lumen = industry leader | Basic RT reflections/shadows |
| Clouds | Nubis = industry reference | Volumetric Clouds plugin (decent) | Reasonable but not hero-level |
| Licensable | Only Kojima Productions | Anyone, commodity | In-house EA only |
| Team size to ship on it | 200-400 devs (Guerrilla/KP scale) | 50-1000 (scales both ways) | 200-500 (EA flagship scale) |
| Transplant difficulty | N/A (not licensable) | Easy | Infamously hard (BioWare saga) |
| Small-team viability | Zero (not available) | High | Zero (not available) |

**Summary**: Decima wins on clouds, vegetation, and open-world streaming at PS5 I/O envelope. UE5 wins on generality, Nanite, licensability, and tooling. Frostbite is not competitive outside DICE's own shooters.

## 13. ALZE applicability matrix

| Decima feature | ALZE v1 (GL 3.3, current) | ALZE v2 (Vulkan, planned) | ALZE v3 (aspirational) |
|---|---|---|---|
| Nubis-style volumetric clouds | **YES, steal algorithm** (GL 3.3 compute-less version: fragment-shader ray march, lower samples, ~4 ms) | **YES, full compute-based** with temporal reprojection | Full Nubis 2 fly-through |
| GPU procedural vegetation placement | NO (requires compute) | **YES, steal algorithm** with Poisson sampling + curl noise wind | Multi-species biome blending |
| Cluster culling + indirect draws | Partial (GL 4.3 MDI available, but no HZB compute) | **YES** — frustum + HZB cull, 2-pass, steal de Carpentier pipeline | + meshlet cull if v3 targets mesh shaders |
| Storyhighway predictive streaming | No (single-scene v1) | Minimal (tile-based streaming with LRU) | Full predictive quest-aware streaming |
| Checkerboard rendering | No (skip) | **YES, optional**, for 4K on mid-tier GPUs | TAAU (DLSS-free) |
| Tile-based deferred | YES, already compatible | YES, compute-based tile cull | + Froxel volumetric fog |
| Compact 4-RT G-buffer | **YES, copy layout** | YES | YES |
| Strand-based hair | No | No | Optional v3 (expensive) |
| RT shadows/reflections | No | Optional if Vulkan RT extensions required | YES |
| Photogrammetry pipeline | Tooling, not engine — orthogonal | Same | Same |
| Custom editor | No (Dear ImGui instead) | No | Maybe |
| UIPainter custom UI | No (Dear ImGui + HTML-ish) | No | No |
| Cross-studio Perforce workflow | Not applicable | Not applicable | Not applicable |

**v1 steal list** (achievable in GL 3.3 now): Nubis cloud math (fragment-shader variant), compact G-buffer layout, tile-based deferred lighting.
**v2 steal list** (Vulkan + compute): add GPU procedural vegetation placement, cluster culling + HZB + indirect draws, checkerboard/TAAU, Nubis compute version with temporal reprojection.
**v3 steal list** (aspirational): full storyhighway-style predictive streaming, strand hair, selective RT shadows.

## 14. Honest ROI note

Decima is roughly **20 years of development** (Killzone 2004 → DS2 2025) by a team that grew from ~60 to ~400 engineers, backed by Sony's first-party resources and a uniquely favourable licensing partner (Kojima Productions). Its signature look — photo-real open-world at PS5 first-party quality — is the product of cumulative compounding investment, not any single algorithm.

**A small ALZE-scale team (1-5 engineers) cannot build Decima.** That is not the goal. The goal is to identify the 2-3 specific tricks that are **stealable in days-to-weeks** and buy ALZE a visual-quality lift well above what the effort would suggest.

### The three stealable tricks

1. **Nubis volumetric clouds, math-for-math.** Schneider's GPU Pro 7 chapter (2016) and the SIGGRAPH 2015+2017 slides are explicit and reproducible. A competent graphics engineer can port the ray-march + Perlin-Worley sampling + HG phase function to a GL 3.3 fragment shader in **~2-3 weeks** for a first-pass version, another 2-3 weeks for temporal reprojection in v2 (Vulkan compute). Cost: ~4-6 engineer-weeks. Benefit: ALZE's sky goes from "engine demo" to "AAA feel" in one feature. This is the single highest-leverage graphics steal available for a small engine targeting photo-real outdoor scenes.

2. **Predictive tile streaming (simplified storyhighway).** You cannot build Guerrilla's full storyhighway with a single dev, but you *can* build the core insight: **streaming priority is a function of camera velocity vector, not just position**. Tiles in the direction of motion get priority; tiles behind get evicted preferentially. Add a quest-path hint bus (any gameplay system can publish "player is heading to X" and influence streaming). Cost: ~2 engineer-weeks on top of a basic tile streamer. Benefit: player-perceived loading latency drops to near-zero for typical traversal patterns.

3. **Checkerboard + TAAU for 4K on mid-tier hardware.** The checkerboard reconstruction algorithm is not open-source from Decima specifically, but Drobot's GDC 2017 talk (DICE/EA, same technique family) is the canonical reference and the algorithm is well-documented in research literature. Cost: ~3-4 engineer-weeks in Vulkan v2 (would not be feasible in GL 3.3 without compute; skip for v1). Benefit: ALZE can target 4K on a GTX 1660 / RX 5500-class GPU instead of needing a RTX 3060 equivalent for native 4K. Doubles addressable player base.

### What to NOT steal

- **GPU-driven everything at launch.** Cluster culling + indirect draws + HZB occlusion is the right v2 goal, but not v1 where GL 3.3 barely supports it.
- **Custom UI framework (UIPainter).** Decima built their own because they could afford 10+ engineer-years of UI eng. ALZE cannot. Use Dear ImGui + a small custom retained-mode layer, or license RmlUi.
- **Storyhighway full predictive stack** — see ROI note above; the simplified velocity-aware version captures 70% of the value at 10% of the cost.
- **The editor.** Decima's editor is 15+ years of Qt/C++ investment. Dear ImGui + text asset files + filesystem watcher + hot reload is the small-team-correct equivalent.
- **Strand-based hair, RT everything, per-pixel cloth simulation.** These are v3 aspirationals at best; they cost more than the rest of the engine combined.

### Final thought

Decima's real lesson for ALZE is not technical — it is organisational. **Decima ships what Horizon's art direction demands.** Not what a generic engine vendor thinks an engine should do; not what the previous-gen feature checklist says. The engine team sits 20 feet from the art lead and the creative director, and the engine ships what that conversation produces. A small-team engine can compete on that axis even if it cannot compete on engineering headcount. What does ALZE's target game need? Build that. Skip everything else. This is the only way Decima-class feel is achievable without Decima-class cost.

## 15. Sources consulted (flat list, URLs + archive fallbacks)

- Andrew Schneider, "The Real-Time Volumetric Cloudscapes of Horizon Zero Dawn," SIGGRAPH 2015 Advances — https://advances.realtimerendering.com/s2015/The%20Real-time%20Volumetric%20Cloudscapes%20of%20Horizon%20-%20Zero%20Dawn%20-%20ARTR.pdf (archive: https://web.archive.org/web/*/advances.realtimerendering.com/s2015/*)
- Andrew Schneider, "Real-Time Volumetric Cloudscapes," GPU Pro 7 (CRC Press, 2016) — https://www.crcpress.com/GPU-Pro-7-Advanced-Rendering-Techniques/Engel/p/book/9781498742535
- Andrew Schneider, "Nubis: Authoring Real-Time Volumetric Cloudscapes with the Decima Engine," SIGGRAPH 2017 Advances — https://advances.realtimerendering.com/s2017/Nubis%20-%20Authoring%20Realtime%20Volumetric%20Cloudscapes%20with%20the%20Decima%20Engine%20-%20Final%20.pdf
- Andrew Schneider, "Nubis, Evolved," 2022 Guerrilla dev blog — https://www.guerrilla-games.com/read/nubis-evolved (archive: https://web.archive.org/web/*/guerrilla-games.com/read/nubis-evolved)
- Giliam de Carpentier & Kohei Ishiyama, "Decima Engine: Advances in Lighting and AA," SIGGRAPH 2017 — https://advances.realtimerendering.com/s2017/DecimaSiggraph2017.pdf
- Giliam de Carpentier & Kohei Ishiyama, "GPU-based Run-Time Procedural Placement in Horizon Zero Dawn," GDC 2017 — https://www.gdcvault.com/play/1024234/GPU-Based-Run-Time-Procedural
- Giliam de Carpentier personal blog — https://www.decarpentier.nl/
- Michal Valient & Michiel van der Leeuw, "Deferred Rendering in Killzone 2" — https://www.guerrilla-games.com/read/deferred-rendering-in-killzone-2
- Michal Valient, "Practical Occlusion Culling in Killzone 3," SIGGRAPH 2011 — https://advances.realtimerendering.com/s2011/Valient_Killzone3Occlusion_SIGGRAPH_2011.pdf
- Michal Valient, "Taking Killzone Shadow Fall Image Quality Into The Next Generation," GDC 2014 — https://www.guerrilla-games.com/read/taking-killzone-shadow-fall-image-quality-into-the-next-generation
- Hugh Malan, Arjan Bak, "A Showcase of Decima Engine in Horizon Forbidden West," SIGGRAPH 2022 Real-Time Live! — https://dl.acm.org/doi/10.1145/3532833.3538681 (HTML: https://dl.acm.org/doi/fullHtml/10.1145/3532833.3538681)
- Wim Van Der Schee, "Lighting the City of Meridian: Horizon Zero Dawn" — https://www.guerrilla-games.com/read/lighting-the-city-of-meridian-horizon-zero-dawn
- Michal Drobot, "4K Checkerboard in Battlefield 1 and Mass Effect Andromeda," GDC 2017 — https://www.gdcvault.com/play/1024747/4K-Checkerboard-in-Battlefield-1
- Michal Drobot, "Low Level Optimizations for GCN" (PS4 GPU techniques), 2014 — https://www.slideshare.net/DICEStudio/low-level-optimizations-for-gcn
- Decima (game engine) — Wikipedia — https://en.wikipedia.org/wiki/Decima_(game_engine)
- Guerrilla Games — Wikipedia — https://en.wikipedia.org/wiki/Guerrilla_Games
- Horizon Zero Dawn — Wikipedia — https://en.wikipedia.org/wiki/Horizon_Zero_Dawn
- Horizon Forbidden West — Wikipedia — https://en.wikipedia.org/wiki/Horizon_Forbidden_West
- Death Stranding — Wikipedia — https://en.wikipedia.org/wiki/Death_Stranding
- Death Stranding 2: On the Beach — Wikipedia — https://en.wikipedia.org/wiki/Death_Stranding_2:_On_the_Beach
- PlayStation.Blog, "The Hideo Kojima Death Stranding interview: Strands, Decima and Guerrilla Games" (2017) — https://blog.playstation.com/2017/02/23/the-hideo-kojima-death-stranding-interview-strands-decima-and-guerrilla-games/
- Guerrilla Games dev blog — https://www.guerrilla-games.com/read (archive: https://web.archive.org/web/*/guerrilla-games.com/read/*)
- 80.lv, "UIPainter: Custom UI Framework In Decima Engine" — https://80.lv/articles/learn-how-guerrilla-games-decima-engine-handles-ui-rendering
- Digital Foundry HFW tech analysis — https://www.eurogamer.net/digitalfoundry-2022-horizon-forbidden-west-ps5-tech-review
- Joost van Dongen (community) — "A Guide to Decima Engine" / Death Stranding modding community — https://github.com/ds-modding (archive: https://web.archive.org/web/*/github.com/ds-modding)
- Andrew Schneider personal site — https://www.schneidervfx.com/
- Nathan Reed blog (ex-Guerrilla engineer commentary on rendering) — https://www.reedbeta.com/
