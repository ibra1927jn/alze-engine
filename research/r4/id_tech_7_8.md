# id Tech 7 & 8 — Doom Eternal, Indiana Jones, Doom: The Dark Ages

> ALZE Engine research — round 4, AAA engines we skipped.
> Target engine: `/root/repos/alze-engine`, C++17, OpenGL 3.3 today, Vulkan 1.3 on v2 roadmap.
> Prior rounds summarised id Tech in ~15 bullets inside `aaa_engines.md` and cross-referenced
> id Tech 5's MegaTexture in `r3/virtual_textures_streaming.md`. This file is the full deep
> dive on id Tech 7 (Doom Eternal, 2020) and id Tech 8 (Indiana Jones and the Great Circle,
> 2024 and Doom: The Dark Ages, 2025). Scope: lineage, rendering, async compute, mesh
> shaders, path tracing, streaming, tools, console parity, licensing — with a running
> "stealable for ALZE" filter.

## 0. Why id Tech matters for a small-team engine

id Tech is the only engine family in the AAA landscape that has consistently married
**locked 60 fps on constrained hardware** with **visual fidelity that wins the annual
benchmark comparisons on PC**. Doom Eternal (2020) simultaneously ships:

- 60 fps locked on the Nintendo Switch OLED (2017 Tegra X1 hardware, ~300 GFLOPS).
- 4K/120 fps on Xbox Series X / PS5.
- >1000 fps on a high-end PC (RTX 3080 / 7950X at low internal resolution and framerate cap
  lifted — Digital Foundry and independent benchmarkers have verified numbers in the
  1400-1800 fps band on top-tier 2023-2025 PCs).

No other shipped engine in the post-Carmack era spans that three-order-of-magnitude
performance band while keeping the visuals at AAA ceiling. For a small-team C++ engine
the study target is not "copy mesh shaders" — it is **extract the discipline** that
makes such a range possible: the frame-budget culture, the Vulkan-native architecture,
the aggressive async-compute overlap, the bindless-everything data layout, and the
text-driven asset pipeline that lets ~30 engine programmers support multiple shipping
studios (id, MachineGames, Tango historically).

## 1. Lineage: id Tech 1 → id Tech 8

### 1.1 Carmack era (id Tech 1-5, 1993-2011)

| Engine | Title(s) | Year | API | Core innovation |
|---|---|---|---|---|
| id Tech 1 | Wolfenstein 3D / Doom | 1993 | Software | BSP-based 2.5D rendering; Binary Space Partitioning. |
| id Tech 2 | Quake | 1996 | Software + GL | True 3D + BSP + lightmaps; first mass-market GL port. |
| id Tech 3 | Quake III Arena | 1999 | OpenGL | Shader scripts (`.shader`), curved surfaces, VM for logic (Q3VM). |
| id Tech 4 | Doom 3 | 2004 | OpenGL 2 | Unified per-pixel lighting + stencil shadows; material system. |
| id Tech 5 | Rage | 2011 | OpenGL 3 | MegaTexture (virtual textures), 60 Hz guarantee, job system. |

Carmack's doctrine across those engines was documented publicly in `.plan` files (archived
at `https://github.com/ESWAT/john-carmack-plan-archive` and the Funny-Looking dog mirror)
and in successive QuakeCon keynotes (1995-2013). The recurring themes — single-threaded
to aggressively-threaded, fixed-function to programmable, brush-based to mesh-based, disc
streaming to virtual textures, lightmaps to per-pixel — all carry forward. But his exit
in 2013 to Oculus ended the personal-architect era of id Tech.

### 1.2 Post-Carmack (id Tech 6 / 7 / 8, 2016-2025)

After Carmack's departure, Robert A. Duffy (CTO) held the directorship role but day-to-day
architecture moved to two technical leads:

- **Billy Khan** — lead engine programmer. Job system, threading model, platform ports.
  The "1000 fps on a 7950X" number is Billy Khan's discipline. Interviews in PC Gamer,
  IGN, and Digital Foundry from 2020 onward attribute the data-oriented threading model
  primarily to him.
- **Axel Gneiting** — senior/lead graphics programmer. Animation, rendering, Vulkan
  adoption. His GDC 2020 talk "Doom Eternal: Bringing Hell to Life Through Animation"
  (video archived at GDC Vault) laid out the job-based animation system.
- **Tiago Sousa** — rendering director (ex-Crytek; joined id ~2016 after CryEngine
  SVOGI era). Sousa is the face of id Tech 8's path-tracer work; SIGGRAPH 2025 "Fast as
  Hell: idTech 8 Global Illumination" is the reference.
- **Jean Geffroy** — senior/lead renderer (id Software). SIGGRAPH 2020 "Rendering the
  Hellscape of DOOM Eternal" is the canonical public breakdown of id Tech 7's rendering.

| Engine | Title(s) | Year | Director(s) | APIs | Hallmark |
|---|---|---|---|---|---|
| id Tech 6 | Doom 2016, Wolfenstein II (MachineGames) | 2016 | Sousa + Khan | OpenGL 4.5 + Vulkan | First shipped Vulkan AAA title; clustered forward. |
| id Tech 7 | Doom Eternal, DOOM Eternal (Switch port Panic Button) | 2020 | Sousa + Khan + Geffroy | Vulkan only | Async compute everywhere; bindless-by-default. |
| id Tech 8 | Indiana Jones and the Great Circle (MachineGames, 2024); Doom: The Dark Ages (id, 2025) | 2024-2025 | Sousa + Khan + Gneiting | Vulkan + RT mandatory; mesh-shader baseline | RT GI baseline; path tracing optional; mesh shaders baseline. |

Note: post-Carmack, id Tech is unique among AAA engines in that **the engine ships with
zero external licensees**. Microsoft (via ZeniMax acquisition, 2021) now owns the engine;
it is first-party only, used across id, MachineGames, Tango Gameworks (historically),
Arkane (partially, mostly Void), and Bethesda Game Studios (no — those remain on
Creation Engine for the Elder Scrolls / Fallout / Starfield IP).

## 2. id Tech 7 — Doom Eternal

### 2.1 Headline performance

- Target: **60 fps locked** on all consoles (Xbox One S minimum), 1000+ fps achievable on
  high-end PC.
- Frame budget: 16.6 ms hard ceiling on base consoles; typical PC RTX 3080 frame at 4K is
  ~3-4 ms GPU, CPU at <1 ms per frame.
- Resolution: dynamic resolution scaling (DRS) inside a ±15 % band around native.
- Internal pipeline: **Vulkan only** (D3D11 back-end was removed between Doom 2016 and
  Eternal; Sousa at GDC 2020 stated the D3D maintenance cost was not worth it given
  Vulkan parity across all target platforms including Stadia at launch).

### 2.2 Rendering model

From Jean Geffroy, Yixin Wang, Axel Gneiting — "Rendering the Hellscape of DOOM Eternal,"
SIGGRAPH 2020 Advances in Real-Time Rendering (PDF archived at
`https://advances.realtimerendering.com/s2020/RenderingDoomEternal.pdf`; Web Archive
fallback at `https://web.archive.org/web/2021*/advances.realtimerendering.com/s2020/`):

- **Clustered forward**: screen-space 3D light/decal/probe grid; the frustum is subdivided
  into a `24 × 12 × 32 = 9216` cell voxel grid (numbers vary per-platform). Each cell
  holds an index list of lights + decals + reflection probes that intersect it. The
  primary pass does one lookup per pixel, iterates the small list, skips gracefully if
  the cell is empty.
- **Hybrid deferred**: a skinny G-buffer is used only for decals and screen-space effects
  (SSAO, SSR). Main shading is forward, so material permutations ride in the primary pass.
- **Dual-queue async compute**: every overlap opportunity is taken. See §3 for the queue
  map.
- **Bindless materials**: a single VK descriptor set holds the full texture atlas pool
  (up to ~2048 BC7/BC5/BC6H slots). Per-draw indirection is a `material_id` scalar
  indexing into an SSBO of `MaterialParams { tex_id_albedo, tex_id_normal, tex_id_arm,
  roughness_bias, metalness_bias, ... }`.
- **GPU-driven culling**: two-pass culling (frustum + HZB occlusion) via compute, emitting
  indirect draw arguments. Particles run in a dedicated compute prepass.
- **Shader permutation discipline**: material ubershader with runtime uniform branches
  rather than combinatorial SPIR-V variants; fallback variants are minimal (~30-50 total
  in the Eternal renderer per Geffroy's talk). The PSO cache at launch is ~500 pipelines
  — tiny compared to UE5's 5000-50000.

### 2.3 Sparse residency + megatexture legacy

Doom Eternal keeps a **limited virtual texture path** for sky domes and a few very large
authored panoramic assets, but the default path is:

- BC7 / BC5 / BC6H compressed textures with full streaming-mip pyramid.
- **Vulkan sparse residency** (`VK_KHR_sparse_binding` + `sparseResidencyImage2D`) lets
  the engine page 64 KiB tiles of a large image in/out without reallocating. Used for
  the HZB, some post targets, and the streaming pool.
- The "transcode every mip" pipeline inherited from id Tech 5 survives: textures are
  authored as floating-point / linear source, quantised offline to BCx, packed into
  fixed-size page streams, streamed from disk as 64 KiB units via async DMA on the
  transfer queue. The transcoder itself is **offline** in id Tech 7 (unlike id Tech 5's
  runtime HD transcode) — all the clever runtime work is now streaming, not codec.

### 2.4 Zone-based level streaming

Doom Eternal levels are authored as "zones" — self-describing text manifests (`.decl`
files) listing entity placements, lights, volumes, triggers. At runtime, a streaming
volume drives:

1. Prefetch of adjacent zones' meshes + textures into a ring buffer.
2. PSO pre-warming from telemetry traces of prior play sessions (the asset build pipeline
   bakes the hot PSO set into the shipping archive).
3. Eviction of far zones based on LRU + designer-marked priority.

The zone boundary is also where **shader modules** stream — Vulkan lets id load/unload
SPIR-V `VkShaderModule` objects, so the working set of compiled pipelines stays bounded.

## 3. Async compute patterns in id Tech 7

This is the section worth copying regardless of genre. Reference: Simon Coenen,
"DOOM Eternal — Graphics Study" (blog, 2020, `https://simoncoenen.com/blog/programming/graphics/DoomEternalStudy`;
Web Archive fallback under `https://web.archive.org/web/2020*/simoncoenen.com/`).

### 3.1 Queues used

Doom Eternal uses all three Vulkan queue families when the device exposes them:

- **Graphics queue** — primary raster passes, G-buffer / forward shading / post.
- **Compute queue** — particle simulation, AO resolve, bloom downsample, SSR, HZB build,
  some light/decal clustering.
- **Transfer queue** — streaming DMA (texture tiles, mesh buffers, PSO blobs).

On platforms with two compute queues (RDNA / Ampere) the engine uses two async compute
streams — one for "long" compute tasks (particle sim, physics-like broadphase) and one
for "short" compute tasks (post-FX, HZB).

### 3.2 Concrete overlap patterns

The canonical frame timeline from Coenen's study plus Geffroy's SIGGRAPH slides:

```
  GFX queue:   [Shadow cascades  ] [Depth prepass] [Clustered forward ] [Post] [UI] [Present]
  CMP queue:   [Particle sim    ]   [HZB build  ] [AO + SSR         ] [Bloom        ]
  XFER queue:  [Texture page-in  ]                [Mesh upload       ]
```

Specific overlap examples documented in the talk:

1. **Particle simulation (compute) overlaps shadow cascades (graphics).** Shadow map
   rendering is vertex-bound; particle sim is ALU/memory-bound. The two saturate
   different hardware blocks and overlap cleanly.
2. **HZB build (compute) overlaps depth prepass (graphics).** HZB mip 0 is built from
   the previous frame's depth while the current frame's depth prepass renders, so the
   current-frame HZB is ready in time for occlusion culling.
3. **SSAO + SSR (compute) overlap clustered forward shading (graphics).** AO and SSR
   read the G-buffer produced by the forward pass; they begin as soon as the relevant
   G-buffer MRT is `VkImage::layout = SHADER_READ_ONLY_OPTIMAL`. The main shading pass
   continues on the graphics queue writing other targets.
4. **Bloom downsample (compute) overlaps UI/HUD (graphics).** UI is low-triangle,
   high-fillrate; bloom is ALU-heavy on a shrunken target. Perfect ALU / ROP split.
5. **Texture page-in (transfer) runs continuously across the frame.** No stalls on main
   queues; the transfer queue quietly ships 64 KiB tiles all frame long.

### 3.3 Synchronisation

id Tech 7 uses **VkSemaphore** for cross-queue ordering, **VkEvent** for in-queue fine
ordering, and **binary fences** for the swap-chain. Because the frame graph (id Tech's
internal render graph, analogous to Frostbite's FrameGraph) declares every pass's
read/write set, the scheduler can insert semaphores automatically. Per Geffroy's talk:
the engineers "write zero manual semaphores in game-side code" — all cross-queue sync is
derived from the declared pass dependency graph.

This is the single biggest safety lesson: do not hand-write async compute barriers.
**Derive them from declared resource usage.** ALZE's own Vulkan path (v2) should
replicate this discipline.

### 3.4 Bubble hiding

Async compute can create new bubbles if the compute queue's long task finishes while the
graphics queue is still busy — the compute queue then starves. id mitigates with:

- **Split long compute passes** into 2-3 shorter dispatches reordered to interleave with
  graphics-queue milestones.
- **Compute-fills** — the scheduler keeps a pool of cheap compute work (probe update,
  low-priority culling refinement) to drop into queue bubbles opportunistically.

## 4. id Tech 8 — Indiana Jones and the Great Circle (2024)

### 4.1 First shipped mesh-shader-as-baseline AAA game

MachineGames' Indiana Jones and the Great Circle (December 2024) is — per Billy Khan's
interviews with PC Gamer and Digital Foundry late 2024 — the **first shipping AAA title
that mandates mesh shaders on the PC minimum spec**. The minimum GPU on PC is an RTX
2060 Super (Turing) or RX 6600 (RDNA2) — both mesh-shader-capable. GTX 10-series cards
(which lack mesh shader support) are explicitly unsupported.

Percentage of geometry through the mesh shader path (Billy Khan, IGN and PC Gamer
interviews, December 2024):

- **~100 % of static opaque geometry** goes through mesh shaders in id Tech 8. The
  legacy VS/IA path is only retained for a small number of special cases (screen-space
  fullscreen passes, some particles, UI).
- Skinned meshes: mesh shaders with per-meshlet skinning in the task stage — the compute
  skinning prepass from id Tech 7 is subsumed into the mesh pipeline.

This matters because every prior mesh-shader-capable game (Alan Wake 2, Avatar, AC
Shadows) has used mesh shaders as an **optional fast path**. Indy is the first to commit
to mesh shaders as the *only* path, which lets id Tech 8 drop the dual VS/MS code
maintenance burden.

### 4.2 Meshlet layout

Meshlets are authored per meshopt (zeux/meshoptimizer) defaults:

- 64 vertices max, 124 primitives max, cone-culling data per meshlet.
- Stored as SoA SSBO streams: vertex positions (f16 or snorm16 quantised), normals (oct16),
  UVs (f16), and a separate index stream of 8-bit local indices.
- Total per-meshlet overhead: ~3 KiB, amortised across 124 triangles.

### 4.3 Ray-traced global illumination as baseline

id Tech 8 makes **hardware ray tracing a minimum spec**. No fallback to SSR/SSAO GI. This
is the industry's second mandatory-RT AAA title after Metro Exodus Enhanced Edition; Indy
is the first mainstream mandatory-RT game on consoles (Series S/X + PS5).

The GI stack (Tiago Sousa, SIGGRAPH 2025 "Fast as Hell: idTech 8 Global Illumination",
`https://advances.realtimerendering.com/s2025/content/SOUSA_SIGGRAPH_2025_Final.pdf`):

- **Ray-traced irradiance probes** on a sparse 3D grid, updated per-frame at ~1/8 density
  (8-frame temporal amortisation).
- **Ray-traced reflections** with ReSTIR-like reservoir resampling for glossy + mirror.
- **Screen-space fallback only for contact shading**, not for primary GI.
- Denoiser: SVGF-family with temporal reprojection, guided by ReLAX (NVIDIA) on RTX
  hardware; custom denoiser on console.

### 4.4 Open-ish levels — streaming for Indy

Indy is structurally different from Doom: it ships large "hub" levels (Vatican City,
Gizeh, Himalayas, Sukhothai) that are closer to Dishonored or Hitman scale than to
Doom's corridor combat. id Tech 8 adapts:

- Zone-based streaming scales to 4-8× the tile count of Doom Eternal zones.
- **Background PSO compilation** runs continuously on the transfer+worker threads as
  the player nears new zones; the zone transition waits on a PSO readiness fence.
- Texture pool size bumped to ~2 GB on Series X / PS5 (vs ~1 GB in Eternal).
- Foliage and crowd systems were added specifically for Indy — not part of the Doom
  engine feature set. GPU-driven instance-cluster rendering (compute cull + indirect
  draw) for grass and NPC crowds.

### 4.5 Shader model

id Tech 8 uses SPIR-V 1.6 minimum (Vulkan 1.3), with these mandatory features:

- `VK_EXT_mesh_shader`
- `VK_KHR_ray_query` (for shaders that want inline RT without a full raygen)
- `VK_KHR_ray_tracing_pipeline` (for the main GI/reflections path)
- `VK_EXT_descriptor_indexing` / `VK_KHR_maintenance5` (bindless)
- `VK_KHR_dynamic_rendering` (no VkRenderPass objects; everything is implicit barrier)

This is an aggressive minimum — it is essentially the Vulkan 1.3 core-plus-RT profile
from 2022, raised to "must-have" in 2024.

## 5. id Tech 8 — Doom: The Dark Ages (2025)

### 5.1 Path tracing mode

Doom: The Dark Ages (May 2025) ships with **full path tracing** as a selectable PC GI
mode. Per NVIDIA's developer blog "How id Software Used Neural Rendering and Path
Tracing in DOOM: The Dark Ages" (`https://developer.nvidia.com/blog/how-id-software-used-neural-rendering-and-path-tracing-in-doom-the-dark-ages/`;
Web Archive fallback at `https://web.archive.org/web/2025*/developer.nvidia.com/blog/how-id-software-used-neural-rendering-and-path-tracing-in-doom-the-dark-ages/`)
and Sousa's SIGGRAPH 2025 talk:

- Up to **8 bounces** of indirect light (typical setting is 3-5 bounces).
- **ReSTIR GI** (spatiotemporal reservoir resampling for indirect illumination).
- **Shader Execution Reordering (SER)** on Ada (RTX 4000) for divergence reduction —
  mandatory path on Ada, falls back to vanilla traversal on Turing/Ampere.
- **Opacity micro-maps (OMM)** for foliage / chain-mail / grate materials — accelerates
  AHS by an order of magnitude.
- **Displaced micro-meshes (DMM)** for some high-detail surfaces.

Frame time budget for PT mode on a 4090 at 4K native: ~18-22 ms (roughly 45-55 fps
native). DLSS 4 Ray Reconstruction + Super Resolution pulls that to 60-100+ fps depending
on quality setting.

### 5.2 Denoiser stack

- **DLSS Ray Reconstruction** (NVIDIA) on RTX — replaces classical denoiser entirely, using a
  neural network trained on PT ground truth.
- **FSR 3.1 upscaler** + ReLAX denoiser on AMD.
- **Custom lightweight denoiser** on Xbox Series X / PS5 Pro because those platforms do
  not expose the RTX Ray Reconstruction model.

Path tracing is **PC-only on current consoles**: Series X and PS5 Pro run "full RT GI"
(the Indy-tier RT stack) but not full PT — the console RT hardware + denoiser ML silicon
is not yet quite fast enough for the 1-2 sample-per-pixel multi-bounce PT that DLSS RR
hides on RTX 4000.

### 5.3 Was the 6-month path tracer claim real?

Sousa's SIGGRAPH 2025 remark that path tracing shipped in "roughly six months" after
decision to commit is broadly corroborated by the short gap between Indy's December 2024
launch (no PT) and DTDA's May 2025 PT mode. The feasibility comes from three things:

1. The renderer already had a clean ray query abstraction from Indy's mandatory RT GI.
2. The material system is disciplined: every surface declares its BSDF in a structured
   way, so the PT shader can evaluate any material with a single dispatch.
3. Tools — the asset pipeline's automatic OMM / DMM extraction was already in place for
   Indy.

Honest filter: "six months" elides the multi-year groundwork in id Tech 6/7/8 that made
the PT extension cheap. Path tracing at the cost of six more months is only possible on
a foundation that has been ray-query-ready for *three* years. ALZE should read this as
"the last mile is cheap if the first mile is right," not as "path tracing is a six-month
project from scratch."

## 6. Resource streaming

### 6.1 Texture streaming evolution

- **id Tech 5 (Rage, 2011)**: MegaTexture — single 128k×128k virtual atlas, CPU JPEG-XR
  transcoder, runtime BC1 compression on the GPU. Famous pop-in.
- **id Tech 6 (Doom 2016)**: tiled atlas (16k×8k) of 128×128 BC-compressed tiles, GPU-driven
  cache. Retained the "every mip in transcoder" philosophy but moved transcoding offline.
- **id Tech 7 (Eternal)**: hybrid — static VT for a few large assets (sky domes, some
  environments), conventional mip-streamed BC textures for props and characters. The
  renderer picks per-material at authoring time; runtime just follows the author's choice.
- **id Tech 8 (Indy / DTDA)**: "VT where it helps, mip streaming where it's enough"
  (Sousa SIGGRAPH 2025). MegaTexture as an architecture is effectively dead; streaming
  discipline is the surviving innovation.

### 6.2 Streaming philosophy

What id Software kept across all generations — and what is genuinely worth copying:

- **Fixed-size streaming pages** (64 KiB or 128×128 BC7 tiles). Predictable allocation,
  trivial LRU eviction, maps cleanly to Vulkan sparse residency.
- **Every texture is mipped offline and stored mip-by-mip in the archive.** No runtime
  mipgen. Worst-case frame cost is deterministic.
- **Priority-weighted streaming queue.** UV-space feedback from the previous frame
  drives which mip to page in next. On-screen area + camera-facing weighting.
- **Budget-capped per-frame upload.** The transfer queue is allowed at most ~20-40 MB
  per frame of new tile data; excess is deferred. This is what keeps the streamer from
  starving the render even when the player is running down a corridor.

### 6.3 Indy's open-ish hub streaming

For Vatican / Gizeh / Himalayas hubs — which are 10-50× the zone footprint of a Doom
Eternal level — id Tech 8 adds:

- **Hierarchical streaming volumes**: the zone graph is no longer a flat list but a
  BVH-like hierarchy. Prefetch crosses zone boundaries based on player trajectory
  prediction (velocity + facing).
- **Asynchronous BVH build**: BLASes for static geometry are pre-built offline; TLAS
  refit per frame; dynamic objects build their BLAS asynchronously as they enter streaming
  radius.
- **Ambient entity loading**: NPC crowds and foliage are streamed as archetype instances
  rather than per-instance data, because the Vatican scene has ~4000 extras on screen
  which would never fit in the Doom-style per-entity manifest.

## 7. MegaTexture legacy in id Tech 7/8

Short answer: **mostly gone**. MegaTexture's DNA survives only as three architectural
habits:

1. **Every mip is baked.** No runtime mipgen; all LODs are precomputed. (id Tech 5's
   transcoder was runtime; 6/7/8 moved transcoding to offline.)
2. **Fixed-size page-based streaming.** 64 KiB tiles / 128×128 BC7 blocks are the units
   everything streams. Even non-VT textures obey this page size.
3. **Feedback-driven LRU cache.** The runtime decides residency based on what the GPU
   actually sampled last frame, not what the CPU guessed.

What id explicitly abandoned:

- **World-space unique texturing**. Artists no longer paint in 3D paint-over-world
  workflow. All texture authoring is UV-space + shader, same as any modern PBR pipeline.
- **Single giant virtual atlas**. The "128k × 128k per level" mental model is dead.
- **Runtime GPU transcode**. The JPEG-XR / HD-transcode runtime hit was not worth the
  disk space savings once HDDs gave way to SSDs and NVMe.

For ALZE this is a clean directive: **do NOT implement virtual textures for v1**. Use
sparse residency (`ARB_sparse_texture` on GL 4.4+, or `VK_KHR_sparse_binding` on v2) for
streaming mip working sets only. Reserve VT for v3 if ever.

## 8. Console parity — Switch OLED to Series X

### 8.1 Switch port of Doom Eternal (Panic Button, 2020)

Panic Button (Austin, TX) shipped the Switch port of Doom 2016 (2017) and Doom Eternal
(2020, docked to 720p/30 with dynamic res, handheld to 540p). Digital Foundry's "Doom
Eternal on Switch" analysis (`https://www.digitalfoundry.net/`) confirmed:

- Texture detail reduced ~2 tiers from PC High.
- Shadow map resolution halved.
- Async compute still present — Tegra X1 supports multiple queues.
- Full bindless texture arrays intact — the trick is a smaller pool (~512 textures
  resident vs 2048 on console).
- Vulkan throughout — no D3D fallback needed because the NVN (Nintendo's native API) is
  not a target; Panic Button uses the Vulkan path.

The lesson: a Vulkan-native engine ports to Switch essentially for free modulo memory
budgeting. An OpenGL engine hits a wall (OpenGL on Switch is deprecated, and GL 4.x
features beyond 4.3 are patchy).

### 8.2 Series S as minimum spec

Xbox Series S (4 TFLOPS, 10 GB RAM shared) is the current-gen minimum. id Tech 7/8 treats
it as a first-class target:

- 1080p / 60 fps on Eternal, 1080-1440p / 60 fps on Indy.
- Lower-resolution shadow cascades and GI probes; same bindless / async / compute-culling
  architecture as Series X.
- Dynamic resolution scaling band is wider on Series S (±20 %) vs Series X (±10 %).

Near-parity is achieved by **shrinking resolution, not cutting features**. Series S has
the same mesh shader + RT hardware as Series X (both RDNA2 with RT), so the architectural
path is identical; only the buffer sizes differ.

## 9. Tools + DCC pipeline

### 9.1 The "no editor" myth

id Tech is often described as "no editor, all text files". That is half true:

- **Levels, materials, entities, streaming volumes** are all text (`.decl`, `.map`, etc.)
  with a defined schema, hot-reloadable on save. This is the authentic text-driven part.
- **Geometry, animation, characters** are authored in **Autodesk Maya + Substance +
  Houdini**, exported via plugins to id's internal binary formats. This is a normal AAA
  DCC pipeline.
- **Lighting and post** use an in-engine debug panel (similar role to Dear ImGui, though
  id's is a custom UI), not a separate editor process.
- **The id Studio** (or "DoomEd" lineage) exists but is an asset browser + zone manifest
  editor + in-game console, not a UE-style monolithic editor.

### 9.2 Maya + Houdini integration

- Maya plugin: exports skeletal meshes, skinning weights, animation clips to id's `.md5`
  / `.md6` / `.idmesh` binary formats. Maintained by the animation team (Gneiting et al.
  in the GDC 2020 animation talk describe the export pipeline).
- Houdini: used for procedural geometry (rubble, debris, some environment details) and
  FX (particle systems) — exported as baked meshes or particle spawn curves.
- Substance: authored texture sets baked to BC7/BC5/BC6H streaming pages at build time.
- Perforce backs the whole content pipeline; asset build DAG is driven by a custom tool
  (similar in role to UE's DerivedDataCache but bespoke to id's asset types).

### 9.3 The "small engine team" math

id Software has ~70-100 engine engineers across the engine team plus game-specific teams;
MachineGames adds another ~30 engine contributors. Compared to Unreal (~1000+ engine
engineers at Epic) that is tiny — but much larger than ALZE's target. The lesson is not
"id is small" — it is that **id's discipline on scope** (text authoring, no monolithic
editor, bespoke pipeline per game) keeps the per-feature cost low even at that headcount.

## 10. Licensing

### 10.1 Ownership chain

- 2009: ZeniMax Media acquires id Software.
- 2021: Microsoft acquires ZeniMax (~$7.5 B deal including Bethesda, Arkane, MachineGames,
  Tango, id).
- **Microsoft now owns id Tech** outright through Bethesda/ZeniMax.

### 10.2 External licensing — effectively zero

Post-Carmack (2013+) id Tech has **no external licensees**. The last external licensee
was Splash Damage (Enemy Territory: Quake Wars, 2007, id Tech 4+). No indie or third-party
studio can license id Tech 6/7/8.

This is a deliberate choice — Microsoft/ZeniMax treats id Tech as a strategic asset for
first-party Xbox studios, not a revenue-generating engine product. It is used by:

- id Software (Doom, Quake).
- MachineGames (Wolfenstein, Indiana Jones).
- Tango Gameworks (partially, historically — The Evil Within used id Tech 5 branch).
- Never by Bethesda Game Studios (Elder Scrolls, Fallout, Starfield stay on Creation Engine).
- Never by Arkane (Void Engine, also id Tech 5-lineage but heavily modified).

### 10.3 Contrast with Unreal

Unreal Engine is Epic's commercial product: 5 % royalty after $1M revenue, source available
to licensees, modifiable under source-access agreement, ~5000+ licensed studios. id Tech
is the opposite model — zero dollars of licensing revenue, engine stays internal, used
to differentiate first-party titles only.

For ALZE this is instructive in two ways:

1. **Specialisation wins when you control the full stack**. id can tune every frame for
   Doom specifically because Doom is the only game the engine has to run.
2. **Generality wins when you need a licensing business**. UE has to render every genre
   reasonably; id just has to render hellscapes at 60 fps.

ALZE is on the id side of that dial: small team, single game-shape, no licensees. Copy
the specialisation discipline, not Unreal's generality.

## 11. Table — id Tech per title (6 / 7 / 8)

| Feature | id Tech 6 (Doom 2016) | id Tech 7 (Doom Eternal, 2020) | id Tech 8 (Indy 2024, DTDA 2025) |
|---|---|---|---|
| Primary API | OpenGL 4.5 + Vulkan | Vulkan only | Vulkan 1.3 + VK_EXT_mesh_shader + VK_KHR_ray_* mandatory |
| D3D back-port | Yes (D3D11) | No | No |
| Renderer | Clustered forward | Clustered forward + async compute | Mesh-shader-native + RT GI baseline |
| Ray tracing | None | None | GI baseline (Indy), PT optional (DTDA PC) |
| Mesh shaders | No | No | Yes, baseline (minimum GPU Turing / RDNA2) |
| Async compute | Yes (compute queue for select passes) | Pervasive, dual-queue where possible | Pervasive + RT queue overlap |
| Virtual texturing | Tiled atlas (16k×8k) | Hybrid VT + mip streaming | Mip streaming dominant; VT vestigial |
| Bindless materials | Yes | Yes (canonical) | Yes |
| Path tracing | No | No | Optional PC mode (DTDA, 2025) |
| Denoiser | n/a | n/a | DLSS RR / FSR3 / custom console |
| Target platforms | PC + XB1 + PS4 + Switch (via Panic Button) | PC + XB1/S/X + PS4/5 + Stadia + Switch | PC (RTX 20+/RDNA2+) + Series S/X + PS5 / PS5 Pro |
| Minimum CPU cores | 4 | 4 | 6 (effective) |
| Minimum GPU tier | GCN 1.1 / Maxwell | GCN 1.1 / Maxwell | Turing / RDNA2 (mesh-shader + RT required) |
| Frame rate target | 60 fps console / uncapped PC | 60 fps console / uncapped PC / ~1000+ fps achievable | 60 fps console / PC scales to 240+ fps non-PT |
| Texture compression | BC1/BC3/BC5/BC6H/BC7 | Same + sparse residency | Same + OMM + DMM metadata |
| Editor | id Studio (asset browser + text decls) | Same | Same + expanded foliage/crowd tools |
| DCC | Maya + Substance + Houdini | Same | Same + expanded Houdini for open-hub procedural |

## 12. ALZE applicability — what's stealable

| id Tech idea | ALZE copy-level | Cost | ALZE version |
|---|---|---|---|
| Frame graph with declared read/write sets | High value — already on v2 Vulkan roadmap | ~1500 LOC | v2 Vulkan path; drives barrier inference + async compute sync. |
| Async compute for post-FX + particles | High value — directly applicable | ~500 LOC plus frame graph | v2 Vulkan. Overlap bloom + SSAO with UI / shadows as per §3.2 patterns 1-4. |
| Bindless textures + material SSBO | High value | ~400 LOC | Single VkDescriptorSet for texture pool; per-draw `material_id`. Massive draw-call reduction. |
| Ubershader + few permutations | High value — stops shader cache explosion | discipline, not code | Target ~50 shader variants total for v2. |
| Sparse residency for streaming mips | Medium | ~800 LOC | v2 Vulkan. Skip virtual texturing entirely. |
| Fixed-size 64 KiB streaming pages | High — cheap structural choice | ~200 LOC | Adopt the unit of transfer across all asset types. |
| Mesh shaders as *opt-in* fast path | Medium — not v1/v2, but reserve API surface | ~1200 LOC when GPU-mandated | v3 optional path. Keep VS path for Intel Arc / older laptops. |
| Ray tracing as *opt-in* mode | Low for v2, Medium for v3 | ~2000 LOC + BVH mgmt | Explicit long-term. Basic ray queries first, not full PT. |
| Text-driven `.decl` authoring | High — replace any dream of monolithic editor | discipline + parser (~600 LOC) | Adopt TOML/JSON for entities, materials, levels; hot reload on save. |
| Zone-based streaming with pre-warm | High | ~700 LOC | Zone hash IDs, prefetch graph, PSO pre-warm from telemetry. |
| Offline mip generation (no runtime) | High | pipeline tooling | Asset build tool bakes all mips; engine never runs mipgen. |
| Job-based threading (Billy Khan style) | Very high — foundational | ~800 LOC fibre scheduler | Every subsystem a job, declared R/W sets per job. |
| `0` manual async compute barriers | Very high — discipline | derive from frame graph | Never hand-write `vkCmdPipelineBarrier` for async compute. |
| Telemetry-driven PSO pre-warm | Medium — requires shipped game | ~300 LOC collector | v3. Ship a telemetry path that records PSO usage per zone, bake into archive. |

What NOT to copy:

| id Tech thing | Why skip |
|---|---|
| MegaTexture / virtual textures | id itself is walking this back; irrelevant for non-corridor games at ALZE scale. |
| Full path tracing | 6 months of id time = 2-3 years for a small team. v3 aspiration only. |
| Mandatory-RT minimum spec | Cuts off half the audience for a small game. Offer RT as an option, not as a floor. |
| Internal bespoke UI framework | Use Dear ImGui or RmlUi. id's UI is not documented externally. |
| Full bespoke job fibre scheduler | Acceptable to ship with a simpler std::thread + work-stealing queue. Fibres are nice-to-have, not required. |
| Microsoft-level PSO pre-warm infra | Requires telemetry-scale ingestion; ship without and iterate on user traces instead. |

## 13. Closing note — id Tech as gold standard for FPS

id Tech is **not a general engine**. It is a *fast-FPS-on-a-deadline* engine tuned across
four generations to run at 60+ fps on constrained hardware with visuals at or near the
annual benchmark ceiling. Its DNA is:

- Vulkan first, everything else second.
- Async compute pervasive, not occasional.
- Bindless data, ubershader discipline.
- Text-driven authoring, no monolithic editor.
- Frame-budget culture — every engineer owns a ms budget.

If ALZE ever leans toward an FPS or fast-action genre, **id Tech's patterns are the gold
standard** and should be the primary study target above UE5 or Unity. Even if ALZE
stays in a different genre, the discipline on frame budgeting is worth extracting
wholesale:

- Per-frame hard ceiling (16.6 ms or 8.33 ms for 120 Hz).
- Per-subsystem sub-budgets owned by named engineers.
- Continuous capture of frame time in telemetry; regression blocks commits.
- Zero tolerance for gradual "the frame got longer" drift.

That culture is the real id Tech IP, and it is copyable for free — it costs discipline,
not code.

## Fuentes consultadas

- Jean Geffroy, Yixin Wang, Axel Gneiting (id Software) — "Rendering the Hellscape of
  DOOM Eternal," SIGGRAPH 2020 Advances in Real-Time Rendering.
  `https://advances.realtimerendering.com/s2020/RenderingDoomEternal.pdf`
  (archive: `https://web.archive.org/web/2021*/advances.realtimerendering.com/s2020/RenderingDoomEternal.pdf`)
- Simon Coenen — "DOOM Eternal — Graphics Study," 2020.
  `https://simoncoenen.com/blog/programming/graphics/DoomEternalStudy`
  (archive: `https://web.archive.org/web/2020*/simoncoenen.com/blog/programming/graphics/DoomEternalStudy`)
- Adrian Courrèges — "DOOM (2016) — Graphics Study," 2016.
  `https://www.adriancourreges.com/blog/2016/09/09/doom-2016-graphics-study/`
- Axel Gneiting (id Software) — "Doom Eternal: Bringing Hell to Life Through Animation,"
  GDC 2020. GDC Vault: `https://www.gdcvault.com/play/1027007/`
- Billy Khan interviews — PC Gamer "Doom Eternal runs 1000 fps" feature (2020);
  IGN Indiana Jones tech deep-dive (December 2024); Digital Foundry Indiana Jones
  Tech Review (December 2024, `https://www.digitalfoundry.net/` — videos on YouTube
  `https://www.youtube.com/@DigitalFoundry`).
- Tiago Sousa (id Software) — "Fast as Hell: idTech 8 Global Illumination," SIGGRAPH 2025.
  `https://advances.realtimerendering.com/s2025/content/SOUSA_SIGGRAPH_2025_Final.pdf`
- NVIDIA Developer Blog — "How id Software Used Neural Rendering and Path Tracing in
  DOOM: The Dark Ages," 2025.
  `https://developer.nvidia.com/blog/how-id-software-used-neural-rendering-and-path-tracing-in-doom-the-dark-ages/`
- Digital Foundry — "Doom Eternal: Switch Tech Review," 2020.
  `https://www.eurogamer.net/digitalfoundry-2020-doom-eternal-switch-tech-review`
- Digital Foundry — "Indiana Jones and the Great Circle: DF Tech Review," December 2024
  (YouTube).
- Digital Foundry — "Doom: The Dark Ages Tech Review," May 2025 (YouTube + Eurogamer
  feature, archive at `https://web.archive.org/web/2025*/www.eurogamer.net/`).
- John Carmack `.plan` file archive — `https://github.com/ESWAT/john-carmack-plan-archive`
  (historical reference for lineage).
- John Carmack — QuakeCon keynotes 2000-2013 (YouTube archive, various uploaders).
- 80.lv — "Using id Tech 7: Doom Eternal Analysis," 2020.
  `https://80.lv/articles/using-id-tech-7-doom-eternal-analysis`
- Wikipedia — "id Tech 6" / "id Tech 7" / "id Tech 8".
  `https://en.wikipedia.org/wiki/Id_Tech_7`, `https://en.wikipedia.org/wiki/Id_Tech_8`
- Vulkan specification — sparse residency, mesh shader, ray tracing extensions.
  `https://registry.khronos.org/vulkan/`
- zeux/meshoptimizer — meshlet generation reference implementation (matches id Tech 8's
  meshlet layout defaults). `https://github.com/zeux/meshoptimizer`
