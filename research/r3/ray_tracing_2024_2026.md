# Ray Tracing in AAA Games 2024-2026 — State of the Art

**Round 3 — alze-engine research.** Companion to `nanite.md`, `lumen.md`, `mesh_shaders_work_graphs.md`, `neural_rendering.md`. Focus: *shipping techniques*, not intro material. Prior r1/r2 coverage (REDengine hybrid RT, RTXDI mention in `redengine_cdpr.md`, NVIDIA Falcor reference in `rendering_libs.md`) is intentionally not re-explained here — this file goes deeper on algorithms, hardware quirks, and frame budgets.

Baseline assumption of the reader: knows what a BVH is, has seen `TraceRay` / `traceRayEXT`, has heard of DLSS/FSR. If not, read `rendering_libs.md` first.

---

## 1. BVH construction and maintenance — TLAS vs BLAS in production

DXR and VK_KHR_ray_tracing model acceleration structures as a two-level hierarchy: the **BLAS** (Bottom-Level AS) holds actual triangle or AABB geometry, and the **TLAS** (Top-Level AS) holds *instances* — transforms pointing at BLASes. The split exists so a forest of 10,000 trees can share one BLAS and 10,000 TLAS instance entries rather than rebuilding 10,000 BVHs. NVIDIA's DXR best-practices blog puts this bluntly: **compact all static BLASes, rebuild the TLAS every frame, and *never* refit the TLAS** — the cost saving is not worth the quality loss (TLAS quality directly affects traversal cost for every ray, so a bad TLAS is a tax on the whole frame) (Juszkiewicz 2020, NVIDIA DevBlog, https://developer.nvidia.com/blog/rtx-best-practices/ ).

**Three operations, three cost classes** (all numbers rough, 200k-tri test mesh, RTX 3080-class GPU, per Jacco Bikker's *How to build a BVH* series, https://jacco.ompf2.com/2022/04/26/how-to-build-a-bvh-part-4-animation/ ):

| Operation | What it does | Cost (200k tri) | Quality |
|-----------|--------------|-----------------|---------|
| Full rebuild (PREFER_FAST_BUILD) | New SAH/LBVH tree from scratch | ~1-2 ms GPU | optimal |
| Full rebuild (PREFER_FAST_TRACE) | Higher-quality SAH build | 2-5x slower than FAST_BUILD | best for static |
| Refit (ALLOW_UPDATE) | Keep topology, recompute bounds bottom-up | ~0.3-0.5 ms | degrades as deformation grows |
| Compaction | Copy into minimum-sized buffer | cheap; one-off | same quality, ≥50% VRAM saved |

**The rebuild/refit decision tree** that shipping engines use:

- **Static opaque geometry**: build once with PREFER_FAST_TRACE, compact, keep forever. Compaction routinely reclaims 50-60% of the initial BLAS VRAM (NVIDIA DevBlog, "Tips: Acceleration Structure Compaction", https://developer.nvidia.com/blog/tips-acceleration-structure-compaction/ ).
- **Skinned characters / animated vegetation**: build with ALLOW_UPDATE + PREFER_FAST_BUILD, then **refit every frame**. After ~N frames (N = 8-32 depending on how far the skin has drifted), **rebuild from scratch** to restore tree quality. Naughty Dog, Machine Games, and Remedy all do some variant of this "refit until worst-fit ratio crosses threshold, then rebuild" scheme.
- **Fully dynamic procedural mesh (cloth, destruction)**: full rebuild every frame, FAST_BUILD. No other option.
- **TLAS**: always rebuild, every frame, FAST_BUILD. A modern TLAS with 5-20k instances builds in ~0.5-1.5 ms on RTX 4070+.

**Indiana Jones (id Tech 8) BLAS compaction case study.** NVIDIA's post-shipping writeup (Zellmann & Gruen, NVIDIA DevBlog Jan 2025, https://developer.nvidia.com/blog/path-tracing-optimizations-in-indiana-jones-opacity-micromaps-and-compaction-of-dynamic-blass/ ) reports: in the Peru jungle level, compacting dynamic vegetation BLASes cut total BLAS VRAM from **1027 MB to 606 MB** — a 41% reduction that moved the path-tracing mode from "only on 16 GB cards" to "playable on 12 GB RTX 4070". Key trick: they compact *dynamic* BLASes too, not just static, amortizing the copy cost over the LOD lifetime of the foliage.

**PIX/Nsight profiling insights that ship in 2024-2026.** Both tools now expose AS build timings per-BLAS. Common findings when teams profile:

1. *AS-build is single digit % of frame time on PC RTX 4070+ for most games, but ~15-25% on PS5/Series X for RT-heavy workloads.* Console RT units have much less bandwidth to the BLAS memory.
2. *Rebuilding too many foliage BLASes per frame* is the #1 pathology — artists drop 30k unique grass blades, each marked ALLOW_UPDATE. Fix is instancing: share one BLAS per blade archetype and use TLAS instances.
3. *TLAS ordering matters.* Putting the camera-local instances first in the instance buffer measurably improves traversal cache coherence; GPU drivers don't all resort. DICE showed this at Digital Dragons 2023.

---

## 2. API surfaces — DXR 1.0 vs 1.1 vs 1.2, and the Vulkan dual API

Both APIs ship *two ways* to trace a ray from the shader, and the choice materially affects pipeline design.

### DXR 1.0 — "ray tracing pipeline" (2018)

Introduced with Turing. A distinct pipeline object binding five shader stages: **RayGen**, **Intersection**, **AnyHit**, **ClosestHit**, **Miss**. You `DispatchRays(w,h,d)` and the driver dispatches rays, traverses the BVH, invokes the right hit shader. Driver is free to reschedule work across SMs for coherence. The Shader Binding Table (SBT) maps `InstanceContributionToHitGroupIndex` + `MultiplierForGeometryContributionToHitGroupIndex` to shader records — this is what makes per-material hit shaders possible but also the #1 source of bugs (one bad SBT stride and every ray hits the wrong shader).

### DXR 1.1 — "inline ray queries" (2020, SM 6.5)

Added `RayQuery<>` objects usable from *any* shader stage (pixel, compute, mesh). No hit shaders: you call `rq.Proceed()` in a loop and handle the hits yourself with plain branching. Benefits: no SBT, trivially composes with existing G-buffer passes, works inside a compute shader doing anything else. Vulkan's equivalent is `VK_KHR_ray_query`.

**Rule of thumb that every IHV repeats** (Microsoft, NVIDIA, Khronos blogs):

- Use **inline ray queries** when you trace few, short, coherent rays and the hit work is cheap (RT shadows, RT AO, inline probe updates, material RT reflection for pixel shader).
- Use the **ray tracing pipeline** when you trace many divergent rays with heavyweight hit shaders (path tracing, ReSTIR GI, global reflections with material evaluation). The driver's reshuffle buys you real wins here.

Mobile (Arm, Qualcomm, PowerVR) so far ships **ray_query only** — no ray_tracing_pipeline. This is a hard portability constraint: if you want a single RT path that runs on mobile Vulkan, you're inline-only (Arm Learning Paths, https://learn.arm.com/learning-paths/mobile-graphics-and-gaming/ray_tracing/rt03_ray_traversal/ ; Khronos blog, https://www.khronos.org/blog/ray-tracing-in-vulkan ).

### DXR 1.2 — SER + Opacity Micromaps (GDC 2025, SM 6.9, Agility SDK 1.619 Feb 2026)

Microsoft announced DXR 1.2 at GDC 2025 bundling two features (DirectX DevBlog, https://devblogs.microsoft.com/directx/announcing-directx-raytracing-1-2-pix-neural-rendering-and-more-at-gdc-2025/ ):

1. **Opacity Micromaps (OMM)** — per-triangle opacity pre-classification so the driver can skip the AnyHit shader invocation for fully-opaque or fully-transparent micro-regions. For alpha-tested vegetation this is *the* optimization: Indy's Peru jungle reports 10-25% RT time reduction with OMMs.
2. **Shader Execution Reordering (SER)** — see §3 below, the headline feature.

Microsoft's own demos show **+40% on RTX 4090** and **up to +90% on Intel Arc B580** just from enabling SER via DXR 1.2 (Tom's Hardware, https://www.tomshardware.com/pc-components/gpus/microsoft-says-directx-raytracing-1-2-will-deliver-up-to-2-3x-performance-uplift ). Headline 2.3x number is a best case on specifically divergent path-tracing kernels.

### Vulkan's three-way split

- `VK_KHR_ray_tracing_pipeline` — equivalent of DXR 1.0 pipelines.
- `VK_KHR_ray_query` — equivalent of DXR 1.1 inline queries.
- `VK_KHR_acceleration_structure` — AS building, shared by both.

A Vulkan renderer targeting 2020+ hardware typically enables *all three*, then picks per-pass. Mobile drops the first.

---

## 3. Shader Execution Reordering (SER) — what changed after Ada

**The problem SER solves.** GPUs execute in subgroups of 32-64 threads lockstep. In a path tracer, 32 threads in a subgroup hit 32 different materials → the driver must serialize 32 different ClosestHit shaders. Effective occupancy collapses to ~3-4% on divergent material rays. NVIDIA measured this on early DXR titles; it is the single biggest reason path tracing hurts.

**The SIGGRAPH 2022 paper** (Arjan Lefohn et al., NVIDIA Research, "Shader Execution Reordering", https://research.nvidia.com/sites/default/files/pubs/2022-07_Shader-Execution-Reordering/ShaderExecutionReordering.pdf ) describes an Ada Lovelace hardware feature that lets the application *hint* a coherence key (typically `MaterialID | GeometryID`), and the HW schedules next-shader invocations by that key. Subgroups become coherent again; occupancy jumps 2-3x on the divergent stage.

**Software exposure.**

- NVIDIA shipped `NVAPI` HLSL extensions (`NvReorderThread`, `NvHitObject`) available via a custom SM 6.5 preprocessor — used by Cyberpunk 2077 RT Overdrive at launch (April 2023) before any standard existed.
- Khronos standardized `VK_NV_ray_tracing_invocation_reorder` (2023), then promoted to multi-vendor `VK_EXT_ray_tracing_invocation_reorder` (2024) with Intel Arc participation (Khronos blog, https://www.khronos.org/blog/boosting-ray-tracing-performance-with-shader-execution-reordering-introducing-vk-ext-ray-tracing-invocation-reorder ).
- Microsoft standardized in DXR 1.2 (SM 6.9) via the **HitObject** abstraction and `ReorderThread()` intrinsic — which lets the shader author explicitly separate *ray traversal* (building a HitObject) from *hit invocation* (calling ClosestHit), with a reorder point in between (DirectX DevBlog, https://devblogs.microsoft.com/directx/shader-execution-reordering/ ).

**Hardware support 2026:** NVIDIA Ada (RTX 40xx) and Blackwell (RTX 50xx), Intel Arc B-series + Xe2 iGPU. **AMD RDNA 4 (RX 9000)** currently *does not reorder* — the extension is exposed but implemented as a no-op on AMD (Tom's Hardware, Feb 2026). AMD has stated reordering is coming in a future driver but has not dated it.

**Indiana Jones SER case study** (Kozlowski, NVIDIA DevBlog Jan 2025, https://developer.nvidia.com/blog/path-tracing-optimization-in-indiana-jones-shader-execution-reordering-and-live-state-reductions/ ): RT Overdrive mode gained SER before ship; combined with "live state reduction" (shrink ClosestHit payload from 140 to 56 bytes by deferring material fetch to after reorder) — **+20% path-tracing throughput on RTX 4090** vs an otherwise identical build without SER. Live-state-reduction is mandatory partner: SER's benefit collapses if your payload is too fat to fit in the reordering register pool.

---

## 4. The ReSTIR family — what actually ships

### ReSTIR DI (Bitterli et al., SIGGRAPH 2020)

Paper: Bitterli, Wyman, Pharr, Shirley, Lefohn, Jarosz, "Spatiotemporal reservoir resampling for real-time ray tracing with dynamic direct lighting", ACM TOG 39(4), https://research.nvidia.com/publication/2020-07_Spatiotemporal-reservoir-resampling-real-time-ray-tracing-dynamic-direct .

**One-line idea:** Resampled Importance Sampling with a streaming-RIS *reservoir* per pixel, reused temporally (from last frame's pixel) and spatially (from neighbors this frame). Net effect: each pixel, each frame, picks one "best" light sample from an effective pool of thousands of candidates at ~1 ray per pixel cost.

The math: every pixel maintains a reservoir `{sample, weight_sum, M}`. Each frame draws K candidate lights cheaply (uniform), streams them into the reservoir via weighted reservoir sampling, then does a visibility ray on the surviving sample. Temporal reuse grabs last frame's reservoir (reprojected); spatial reuse merges reservoirs from ~5 neighbors. The estimator remains unbiased (with the MIS-weight correction in §5 of the paper) even though samples are recycled.

Shipping integration: **Cyberpunk 2077 2.0+** uses ReSTIR DI for neon/area-light direct lighting; **Alan Wake 2 path tracing** uses it for direct; **Portal RTX** and **Indiana Jones path tracing** use it.

### ReSTIR GI (Ouyang et al., HPG 2021)

Paper: Ouyang, Liu, Zhang, Pantaleoni, Novák, "ReSTIR GI: Path Resampling for Real-Time Path Tracing", HPG 2021, https://research.nvidia.com/publication/2021-06_restir-gi-path-resampling-real-time-path-tracing .

**Extension:** Instead of resampling emissive lights, resample *paths* — each "sample" is a BRDF-sampled secondary ray's endpoint (the indirect light surface). Then apply the same streaming reservoir + spatio-temporal reuse. This gives you ~1 spp path-traced GI that looks like 32+ spp after filtering.

Deployed: **Cyberpunk 2077 RT Overdrive** (the famous flagship, Kozlowski GDC 2024), **Alan Wake 2 PT** (indirect lighting only — Remedy keeps baked direct on the rasterized pass and overlays PT for indirect, per Digital Foundry's breakdown), **Indiana Jones PT**.

### RTXDI — the NVIDIA library that packages both

RTXDI (RTX Dynamic Illumination) is NVIDIA's open-source reference implementation: https://github.com/NVIDIA-RTX/RTXDI . Structure:

- `Rtxdi/Include/Rtxdi/DI/` — DI resampling kernels (HLSL).
- `Rtxdi/Include/Rtxdi/GI/` — GI resampling kernels (HLSL, added in RTXDI 2.0).
- Host code for reservoir buffer allocation, light PDF texture, environment map importance map.
- Two sample apps: `MinimalSample` (one-pass DI, ~500 LOC of host) and `FullSample` (multi-pass integration into deferred pipeline).

Most shipping integrations fork RTXDI rather than depend on it — CDPR, Remedy, Machine Games all have in-tree forks with engine-specific reservoir compression.

### Nuances that bite

- **Temporal reservoir reprojection** is the hard part. Disocclusion (a new surface appearing) or discontinuities must reset M to avoid biased ghosting; shipping engines clamp M to ~20-40 even if the pixel is "stable" to keep responsiveness.
- **Spatial reuse radius** has to adapt: 5 px in flat regions, shrink to 2-3 near edges, or you bleed light across silhouettes.
- **Bias control:** the unbiased variant needs extra visibility rays for each spatial neighbor = 5x traversal cost; Bitterli 2020 §5.3 introduces a *biased* variant that skips those and looks fine 99% of the time. Most shippers use the biased version.

---

## 5. Path tracing modes in shipping games

By April 2026, only a handful of titles ship a full "path tracing" option. They are all NVIDIA-funded or tightly NVIDIA-partnered, and they all share NRD + ReSTIR + DLSS-RR as denoising stack.

### Cyberpunk 2077 RT Overdrive (April 2023, major update Patch 2.1 Dec 2023)

- Engine: REDengine 4 (see r2 `redengine_cdpr.md`).
- Integrator: CDPR + NVIDIA (Pawel Kozlowski lead). Public GDC 2024 talk: "Pushing Path Tracing One Step Further" (https://www.nvidia.com/en-us/on-demand/session/gdc24-gdc1002/ ).
- Pipeline: 2 primary rays/pixel, 2 bounces. Direct lighting via ReSTIR DI over all emissive/analytic lights. Indirect via ReSTIR GI with 5-neighbor spatial reuse. Transmission for glass. All post-denoised with **NRD ReLAX** (diffuse+specular) and **SIGMA** (shadows).
- Ships with a custom SER integration (pre-DXR 1.2) for +30% perf on RTX 40.
- Raw 4K perf on RTX 4090: **16-18 fps native, 110-130 fps DLSS-3 Perf+FG** (Tom's Hardware, https://www.tomshardware.com/features/cyberpunk-2077-rt-overdrive-path-tracing-full-path-tracing-fully-unnecessary ). RTX 4070 at 1440p: 8 fps native, ~60 fps DLSS-Balanced+FG.

### Alan Wake 2 Path Tracing (Nov 2023 launch, PT on RTX 40+ from day 1)

- Engine: Remedy's Northlight.
- Strategy: **PT overlaid on raster, not full-replace.** Remedy keeps baked direct lighting from their offline GI bake and uses PT exclusively for indirect illumination + RT reflections + RT shadows. This is the "hybrid path tracing" compromise Digital Foundry documents extensively.
- Denoiser: NRD ReLAX for diffuse indirect, SIGMA for shadows, then DLSS 3.5 Ray Reconstruction replaces the specular denoiser entirely (neural).
- Targeting: 4K DLSS Performance + Frame Gen on RTX 4090 = 70-90 fps; RTX 4070 same mode ≈ 50 fps (DSOGaming, https://www.dsogaming.com/pc-performance-analyses/alan-wake-2-dlss-3-5-ray-tracing-path-tracing-benchmarks/ ).

### Portal RTX (Dec 2022, DLSS 3.5 update Sept 2023)

- Engine: NVIDIA **RTX Remix** — *replaces* the fixed-function D3D9 pipeline of original Portal (2007) with an interception layer (`dxvk-remix`, https://github.com/NVIDIAGameWorks/rtx-remix ) that captures all draw calls, reconstructs PBR materials, rebuilds geometry as DXR BLASes, and feeds a full path tracer.
- Bounces: up to 8, all emissive lights ReSTIR-sampled.
- Notable because it's pure PT — *no raster baseline* at all, unlike AW2. Used as a reference benchmark for "what happens if you just path-trace from scratch" in a geometry-simple (but visually complex) scene.
- Particle system: path-traced particles added late 2024 — each particle becomes a low-res AABB proxy in the BLAS (NVIDIA blog, https://www.nvidia.com/en-us/geforce/news/rtx-remix-advanced-particle-system-release/ ).

### Indiana Jones and the Great Circle — Full Ray Tracing mode (Dec 2024 + PT update April 2025)

- Engine: id Tech 8 (Machine Games) — see r4 `id_tech_7_8.md`.
- First id Tech title with full PT. Ships with OMMs, SER, BLAS compaction all day 1 — the most modern RT stack shipping.
- Tight VRAM target: **12 GB RTX 4070 for 1080p PT+DLSS**. Achieved via aggressive BLAS compaction (§1) and texture LRU residency.
- Not an "overlay" like AW2 — genuine PT replacing all lighting, with a scripted fallback to their baked PBR-GI bake under PT.

### NRD — the denoiser stack under all four

NVIDIA Real-time Denoisers, https://github.com/NVIDIA-RTX/NRD . Three denoisers:

- **ReBLUR** — spatio-temporal blur for diffuse + specular signals. Good for low-spp workloads but can lag under fast motion. Uses normal-based and hit-distance-based guide weights.
- **ReLAX** — SVGF-inspired (Schied 2017) wavelet reconstruction with history clamping. Designed specifically for RTXDI output — hence used for direct/indirect ReSTIR signals.
- **SIGMA** — shadow-specific denoiser; knows about penumbra width from traced hit distances. Cheap (~0.3 ms at 1440p on RTX 4070).

Since DLSS 3.5 (late 2023), NVIDIA pushes **Ray Reconstruction** as a *neural* replacement for ReLAX's specular path — the RR NN takes noisy per-pixel signals + motion vectors + material buffers and directly outputs the clean upscaled frame, bypassing the separate denoiser+upscaler pipeline. Shipping in Cyberpunk 2.0, AW2, Portal RTX. Costs roughly same as DLSS2 at target res on RTX 40; quality wins on disocclusion, loses occasionally to ReLAX on very slow-moving surfaces.

---

## 6. Neural Radiance Caching (NRC)

Paper: Müller, Rousselle, Novák, Keller, "Real-time Neural Radiance Caching for Path Tracing", SIGGRAPH 2021, https://research.nvidia.com/publication/2021-06_real-time-neural-radiance-caching-path-tracing , PDF: https://jannovak.info/publications/NRC/NRC.pdf .

**Idea.** After bouncing a path a few times, instead of continuing to trace more bounces, evaluate a *tiny* neural network (5-7 hidden layers × 64 units, ~56 KB weights) conditioned on `(position, normal, view_dir, material_params)` to approximate the incoming radiance at that point. This caches diffuse global illumination online, without offline training.

**Online training is the crucial innovation.** The NN trains *during rendering* on the few rays that do trace the full path — each frame, ~0.5% of rays go to "full depth" and produce ground-truth labels, which become a minibatch for SGD. Convergence in 2-10 frames; the cache adapts to moving lights and geometry without pretraining.

**Cost numbers from the paper (RTX 3090):**
- NN inference: **~2.6 ms at 1080p** for cache evaluation + training.
- Throughput: **>1 billion GI queries/second**.
- End-to-end quality win: **"up to 100x" rendering efficiency vs equivalent-quality pure PT**, combined with ReSTIR DI.

**Shipping status 2026.** NRC is in NVIDIA Falcor, ORCA reference builds, and integrated as *experimental* in UE5.4 via NVIDIA's fork ("RTX Branch of Unreal"). Full-ship production integration remains rare — the training latency (5-10 frames to converge) produces light-adaptation visible artifacts that no shipping game accepts yet. Expect 2026-2027 integrations as the training jitter gets damped by NRC v2 work at SIGGRAPH Asia 2025 (mobile NRC adaptation, https://dl.acm.org/doi/10.1145/3757376.3771399 ).

---

## 7. Hybrid pipelines — what "hybrid RT" actually means in 2026

"Hybrid" in 2018 meant *raster G-buffer + RT reflections* (Battlefield V). In 2026 it means a graded stack of lighting techniques, each chosen per-pixel by budget and surface class.

### Battlefield V (DICE, SEED, Nov 2018) — the origin

GDC 2019 talk: Deligiannis, "It Just Works: Ray-Traced Reflections in Battlefield V", https://gdcvault.com/play/1026282/ . Key tricks:

1. **Ray binning.** Group rays by direction + mip-level before dispatch so memory accesses coalesce. Measured ~2x speedup on Turing.
2. **Hybrid SSR→RT promotion.** Start every reflection ray as screen-space; if it leaves the screen or gets occluded, *promote* to a world-space RT ray from the failure point. Keeps cost bounded by screen-space coverage.
3. **Variable rate per material.** Roughness-driven per-pixel reflection toggle: rough materials get 0 rays; mirror-like get 1 ray/pixel.

All three tricks are still standard in 2026. REDengine (`redengine_cdpr.md`), Snowdrop (`snowdrop_ubisoft.md`), and RE Engine (`re_engine_capcom.md`) all use the SSR→RT promotion pattern.

### Lumen HW (UE5.4+) — SDF + BVH graded by surface distance

UE5's Lumen (see `lumen.md`) is the most elaborate shipping hybrid. For each trace, in order:

1. **Screen-space trace** against the Z-buffer. Hit → done.
2. **Mesh SDF** (per-mesh signed distance field). Hit within `X` units → sample surface cache, done.
3. **Global SDF** (world-scale compound SDF) for far hits.
4. **Optional: HW BVH** if RT is enabled — path 1 stays, 2+3 replaced by `RayQuery` against the BLAS hierarchy, evaluating *Hit Lighting* (full material eval) or cheaper *Surface Cache* lookup.

Lumen's *Hit Lighting* mode (UE5.4, 2024) is HW-RT + full material eval at each hit — most expensive, best quality. *Surface Cache* mode (default) skips material eval at hit points and reads a 2D card atlas precomputed at mesh authoring time (NVIDIA UE5.4 Ray Tracing Guide, https://dlss.download.nvidia.com/uebinarypackages/Documentation/UE5+Raytracing+Guideline+v5.4.pdf ). Typical frame cost: **Lumen GI + reflections 2-5 ms on RTX 4070+ at 1440p**, doubled in SDF-heavy indoor scenes.

### Frostbite 2026 — RT GI production

DICE's current (FY26) pipeline replaces the 2018-era RT reflections with full RT GI for Battlefield 2026 and the upcoming Mass Effect. Public info is thin but GDC 2024 talks imply: ReSTIR DI + probe-based indirect (DDGI) + SSR→RT for reflections, all gated on a 4 ms RT budget at 60 Hz. Console path uses FSR 3.1 Frame Gen.

---

## 8. BVH for animated skinning — the hard case

Skinned characters break static BVH assumptions because every frame the vertex positions move. Three shipping strategies in 2026:

### Strategy A — "skin-then-refit every frame" (most common)

1. CPU or GPU skinning writes new vertex positions to a "skinned VB" buffer.
2. DXR `BuildRaytracingAccelerationStructure` with `PERFORM_UPDATE` flag refits the BLAS over that buffer.
3. Every `N` frames (N=16 typical), do a full rebuild to restore SAH quality.

GPU skinning for RT is done in a compute shader *before* the AS build, not via the normal vertex shader pipeline. This is a measurable cost: for a 50k-vertex character at 60 Hz, ~0.2 ms skinning compute + 0.3 ms BLAS refit = 0.5 ms/character. With 10 visible characters = 5 ms just for character BLASes.

### Strategy B — "pre-skinned BLAS cache" (Naughty Dog, rumored Insomniac)

Pre-bake skinned BLASes at a small number of keyframes (8-16 per animation) and interpolate transforms in the TLAS only. The character is "piecewise-rigid": works surprisingly well for combat, breaks on facial animation and ragdolls. Cheaper per frame (no refit) but costs VRAM and authoring.

### Strategy C — "skip the character in RT, use screen-space fallback" (Cyberpunk 2.0 before RT Overdrive)

Characters are excluded from the BLAS; they appear in the G-buffer and use SSR for reflections. Cheap, but produces the famous "Cyberpunk missing-reflection body" bug in mirrors. RT Overdrive fixed this by moving to Strategy A for V and NPCs.

### Strategy D — "split skinned from procedural" (id Tech 8, Remedy)

Vegetation uses wind-SDF displacement evaluated at BLAS-build time; characters use Strategy A. Keeps the per-frame refit list small (characters only) while still getting RT foliage.

**The budget reality.** On PS5/Series X, skinned-character RT budget is ~1 ms total for all characters combined. On RTX 4070 it's ~3 ms. Past that, engine falls back to fewer RT characters per scene or SSR-only.

---

## 9. Performance numbers 2024-2026 — by platform and technique

All numbers 1440p native where not noted, representative in-scene measurements from Digital Foundry, Hardware Unboxed, Tom's Hardware, Chips & Cheese reporting 2024-2026. These are "order of magnitude" — per-scene variance is ±50%.

### RT feature frame costs @ 1440p, per platform

| GPU | RT Shadows | RT AO | RT Reflections (hybrid) | RT GI (ReSTIR) | Full PT |
|-----|------------|-------|-------------------------|----------------|---------|
| RTX 4090 | 0.5 ms | 0.6 ms | 1.5 ms | 2.5 ms | 18-25 ms |
| RTX 4080 Super | 0.7 ms | 0.8 ms | 2.0 ms | 3.5 ms | 25-35 ms |
| RTX 4070 Ti Super | 0.9 ms | 1.1 ms | 2.8 ms | 5 ms | 40-55 ms |
| RTX 4070 | 1.2 ms | 1.5 ms | 3.5 ms | 6-7 ms | 60-80 ms (not viable native) |
| RTX 4060 | 2.0 ms | 2.5 ms | 6 ms | 11 ms | >100 ms (not viable) |
| RTX 5090 (Blackwell) | 0.3 ms | 0.4 ms | 1.0 ms | 1.5 ms | 10-14 ms |
| RX 7900 XTX | 1.1 ms | 1.3 ms | 3.2 ms | 6 ms | >120 ms |
| RX 9070 XT (RDNA 4, 2025) | 0.7 ms | 0.9 ms | 2.2 ms | 4 ms | ~70 ms |
| Arc B580 | 1.8 ms | 2.3 ms | 5.5 ms | 10 ms | >120 ms |
| PS5 (RDNA 2) | 2.5 ms | 3.0 ms | 7 ms | 13 ms | not viable |
| PS5 Pro (RDNA 2 + "BVH8" RT) | 1.2 ms | 1.5 ms | 3.5 ms | 6.5 ms | not viable (maybe at 720p/30) |
| Xbox Series X | 2.3 ms | 2.8 ms | 6.5 ms | 12 ms | not viable |

**PS5 Pro specifics.** Sony claims "2-3x RT perf vs PS5" using enhanced RT units with PS5-Pro-specific BVH8 traversal (4-wide was PS5, 8-wide fanout is the Pro addition). PSSR upscaler adds ~2 ms at 4K output — negligible compared to a 16.6 ms 60 Hz budget (TweakTown, https://www.tweaktown.com/news/100436/ ). Cyberpunk 2077 PS5 Pro "RT Pro" mode at 30 Hz enables RT reflections + shadows + AO + emissive — roughly equivalent to RTX 4060 RT Medium.

**Rule of thumb.** Full path tracing is **RTX 4080 + DLSS Perf + FG** as a minimum playable bar at 4K 60 Hz. Everything below requires 1080p input or 30 Hz. On consoles, full PT isn't coming before PS6 (rumored 2027-2028).

---

## "RT techniques shipping 2024-2026" — consolidated

| Technique | Frame cost @ 1440p RTX 4070 | Quality vs raster baseline | Vendor-agnostic? | When to use |
|-----------|-----------------------------|----------------------------|------------------|-------------|
| RT Shadows (hard + soft) | 1.2 ms | Eliminates shadow-map peter-panning, contact hardening | Yes (DXR 1.1/ray_query) | Everywhere if budget allows; first RT feature to add |
| RT AO | 1.5 ms | Replaces SSAO, no view-dependent artifacts | Yes | When already paying for BLAS; cheap incremental |
| RT Reflections (hybrid SSR→RT) | 3.5 ms | Off-screen geometry reflections; glass stacks | Yes | Mirrors, wet streets, car bodies |
| RT GI (DDGI probes) | 4 ms | Soft indirect, dynamic | Yes | Large open-world, dynamic lighting |
| RT GI (ReSTIR GI, RTXDI) | 6-7 ms | Near-offline quality, caustics approximate | Yes (lib is open) but tuned for NVIDIA | Flagship titles on flagship HW |
| Path Tracing (Overdrive-tier) | 60-80 ms native | Reference quality, all lighting unified | Technically yes; practically NVIDIA-only viable | Ultra-preset showcase modes |
| Lumen HW (UE5) | 2-5 ms (GI+refl) | Dynamic GI + reflections at prod cost | Yes (UE5 feature) | Any UE5 title, console-viable |
| NRC (Neural Radiance Cache) | ~2.6 ms + query savings | Recovers 3rd+ bounce quality | Yes (open paper) | Path-tracing pipelines only; niche 2026 |
| SER (DXR 1.2) | -20 to -40% PT cost | No quality change — pure speedup | NVIDIA Ada+/Intel Arc B+; AMD no-op | Divergent hit workloads (PT, ReSTIR GI) |
| OMM (Opacity Micromaps) | -10 to -25% RT cost | No quality change | NVIDIA RTX 40+/Intel Arc B+ | Alpha-tested foliage |
| Ray Reconstruction (DLSS 3.5) | Same as DLSS2 | Replaces specular denoiser; wins on disocclusion | NVIDIA only | Any PT title on RTX |
| NRD (ReBLUR/ReLAX/SIGMA) | 1-2 ms total | Required for <1 spp RT to look acceptable | Yes (MIT-ish license) | Every low-spp RT pipeline |

---

## ALZE applicability — realistic tiers

ALZE today is C++17 + SDL2 + OpenGL 3.3 (see `aaa_engines.md` background table). OpenGL has *no* standard ray tracing — `GL_NV_ray_tracing` exists but is vendor-locked and deprecated. Realistic tiering:

| Version | Feature | Verdict | Reasoning |
|---------|---------|---------|-----------|
| **v1 (today, GL 3.3)** | Any HW RT | **Impossible** | GL 3.3 predates RT by a decade. Could implement a software BVH + compute-shader-free CPU ray tracer for a UI gimmick, but not production. |
| v1 | Software raymarched SDF shadows | Possible | Pure GLSL 3.30 in the fragment shader. ~5-10% perf. Already more value than nothing. |
| v1 | Screen-space reflections (SSR) | Possible and recommended | Pixel-space raymarch against depth; ~100 LOC GLSL. Ships in every AAA game pre-2018. |
| v1 | Screen-space GI (SSGI, GTAO) | Possible | Intel's XeGTAO port to GL 3.3 compute — slightly ugly hack but doable. |
| **v2 (hypothetical Vulkan port)** | `VK_KHR_ray_query` inline | **Yes, if baseline is RTX 20xx+ / RDNA 2+ / Arc** | Start with RT shadows (Strategy: DXR 1.1-style inline ray queries from the deferred lighting compute shader). Budget 2-3 ms. |
| v2 | `VK_KHR_ray_tracing_pipeline` for reflections | Possible | Add after shadows work; use hybrid SSR→RT promotion pattern. |
| v2 | RT AO | Easy add once shadow path exists | Same BLAS, different ray direction. |
| v2 | BLAS management (rebuild/refit) | Mandatory | Must implement a scheduler that rebuilds ~3 BLASes per frame and refits animated ones. Copy NVIDIA DXR best-practices. |
| v2 | RTXDI / ReSTIR DI | **Aspirational** | Library fork is MIT-ish; integration is 4-6 weeks of one graphics engineer. Worth it only if the game has >50 dynamic lights per scene. |
| v2 | NRD denoisers | Mandatory if doing any RT GI/reflections | Direct port of the NRD Vulkan sample — 1-2 week integration. |
| **v3 (aspirational)** | Full path tracing mode | **Do not ship** | Requires a team of 3+ graphics engineers, 6-12 months, NVIDIA partnership for SER tuning. No indie engine should promise this. |
| v3 | NRC | Do not ship | Research-grade; not stable enough for a small team. |
| v3 | Lumen-like hybrid | Possible if v2 lands first | Can build a naive SDF+BVH hybrid in v3 once v2 RT is solid. ~3 months engineering. |

**Hard pragmatic recommendation for ALZE.**

1. Stay GL 3.3 for v1; add **SSR + SSGI (XeGTAO)** to get "RT-like" quality at zero RT dependency. These are in every AAA title pre-2020 for a reason.
2. If porting to Vulkan for v2, the *first* RT feature is **RT shadows via inline ray queries** on RTX 20xx baseline — measurable quality win, ~2 ms, no SBT complexity.
3. Never promise path tracing. It's a marketing trap; the denoiser tuning alone is a full-time role at CDPR.
4. If neural rendering matters, **integrate XeSS or FSR 3 first** (vendor-agnostic upscalers) — they pay back more per engineer-hour than any RT feature.

---

## References — primary papers and talks

- Bitterli, Wyman, Pharr, Shirley, Lefohn, Jarosz — "Spatiotemporal reservoir resampling for real-time ray tracing with dynamic direct lighting" — SIGGRAPH 2020, ACM TOG 39(4). https://research.nvidia.com/publication/2020-07_Spatiotemporal-reservoir-resampling-real-time-ray-tracing-dynamic-direct and author's site https://benedikt-bitterli.me/restir/
- Ouyang, Liu, Zhang, Pantaleoni, Novák — "ReSTIR GI: Path Resampling for Real-Time Path Tracing" — HPG 2021. https://research.nvidia.com/publication/2021-06_restir-gi-path-resampling-real-time-path-tracing
- Müller, Rousselle, Novák, Keller — "Real-time Neural Radiance Caching for Path Tracing" — SIGGRAPH 2021, ACM TOG 40(4). https://research.nvidia.com/publication/2021-06_real-time-neural-radiance-caching-path-tracing — PDF https://jannovak.info/publications/NRC/NRC.pdf — arXiv https://arxiv.org/abs/2106.12372
- Lefohn et al. (NVIDIA Research) — "Shader Execution Reordering" — SIGGRAPH 2022. https://research.nvidia.com/sites/default/files/pubs/2022-07_Shader-Execution-Reordering/ShaderExecutionReordering.pdf
- Kozlowski (NVIDIA, CDPR) — "RT: Overdrive in Cyberpunk 2077 Ultimate Edition: Pushing Path Tracing One Step Further" — GDC 2024. https://www.nvidia.com/en-us/on-demand/session/gdc24-gdc1002/ and course slides https://intro-to-restir.cwyman.org/presentations/2023ReSTIR_Course_Cyberpunk_2077_Integration.pdf
- Deligiannis (DICE/SEED) — "'It Just Works': Ray-Traced Reflections in Battlefield V" — GDC 2019. https://gdcvault.com/play/1026282/ — PDF https://developer.download.nvidia.com/video/gputechconf/gtc/2019/presentation/s91023-it-just-works-ray-traced-reflections-in-battlefield-v.pdf
- NVIDIA NRD (Real-Time Denoisers) — https://github.com/NVIDIA-RTX/NRD — sample repo https://github.com/NVIDIA-RTX/NRD-Sample — Vulkan sample https://github.com/nvpro-samples/vk_denoise_nrd
- NVIDIA RTXDI (ReSTIR DI + GI library) — https://github.com/NVIDIA-RTX/RTXDI — RestirGI doc https://github.com/NVIDIA-RTX/RTXDI/blob/main/Doc/RestirGI.md
- NVIDIA RTX Remix / Portal RTX — https://github.com/NVIDIAGameWorks/rtx-remix and https://docs.omniverse.nvidia.com/kit/docs/rtx_remix/1.2.4/docs/runtimeinterface/renderingtab/remix-runtimeinterface-rendering-pathtracing.html
- Microsoft DirectX DevBlog — "Announcing DirectX Raytracing 1.2, PIX, Neural Rendering and more at GDC 2025" https://devblogs.microsoft.com/directx/announcing-directx-raytracing-1-2-pix-neural-rendering-and-more-at-gdc-2025/ — and "D3D12 Shader Execution Reordering" https://devblogs.microsoft.com/directx/shader-execution-reordering/
- Tom's Hardware — "DXR 1.2 up to 2.3x uplift" https://www.tomshardware.com/pc-components/gpus/microsoft-says-directx-raytracing-1-2-will-deliver-up-to-2-3x-performance-uplift — "SER on Intel Arc B +90%" https://www.tomshardware.com/pc-components/gpus/microsoft-adds-shader-execution-reordering-ser-in-latest-directx-sdk-for-more-efficient-ray-tracing-intel-arc-b-series-gpus-show-90-percent-performance-uplift — "Cyberpunk RT Overdrive" https://www.tomshardware.com/features/cyberpunk-2077-rt-overdrive-path-tracing-full-path-tracing-fully-unnecessary — "Alan Wake 2 punish your GPU" https://www.tomshardware.com/features/alan-wake-2-will-punish-your-gpu
- NVIDIA DevBlog "RTX Best Practices" — https://developer.nvidia.com/blog/rtx-best-practices/
- NVIDIA DevBlog "Tips: Acceleration Structure Compaction" — https://developer.nvidia.com/blog/tips-acceleration-structure-compaction/
- NVIDIA DevBlog "Managing Memory for Acceleration Structures in DXR" — https://developer.nvidia.com/blog/managing-memory-for-acceleration-structures-in-dxr/
- NVIDIA DevBlog Indy PT — "Path Tracing Optimization in Indiana Jones: Shader Execution Reordering and Live State Reductions" https://developer.nvidia.com/blog/path-tracing-optimization-in-indiana-jones-shader-execution-reordering-and-live-state-reductions/ — "Opacity MicroMaps and Compaction of Dynamic BLASs" https://developer.nvidia.com/blog/path-tracing-optimizations-in-indiana-jones-opacity-micromaps-and-compaction-of-dynamic-blass/
- Khronos blog — "Ray Tracing In Vulkan" https://www.khronos.org/blog/ray-tracing-in-vulkan — "VK_EXT_ray_tracing_invocation_reorder" https://www.khronos.org/blog/boosting-ray-tracing-performance-with-shader-execution-reordering-introducing-vk-ext-ray-tracing-invocation-reorder
- Arm Learning Paths — "Ray traversal: ray tracing pipeline versus ray query" https://learn.arm.com/learning-paths/mobile-graphics-and-gaming/ray_tracing/rt03_ray_traversal/
- Jacco Bikker — "How to build a BVH" series, https://jacco.ompf2.com/2022/04/13/how-to-build-a-bvh-part-1-basics/ through part 5 https://jacco.ompf2.com/2022/05/07/how-to-build-a-bvh-part-5-tlas-blas/ (part 4 animation https://jacco.ompf2.com/2022/04/26/how-to-build-a-bvh-part-4-animation/ )
- Digital Foundry (Alex Battaglia) — Alan Wake 2 path tracing analysis (YouTube + NeoGAF mirrors https://www.neogaf.com/threads/digital-foundry-alan-wake-2-pc-path-tracing-the-next-level-in-visual-fidelity.1663114/ and https://www.resetera.com/threads/digital-foundry-alan-wake-2-pc-path-tracing-the-next-level-in-visual-fidelity.781736/ ) — Indy RT breakdown https://www.neogaf.com/threads/digital-foundry-inside-indiana-jones-and-the-great-circle-the-ray-tracing-breakdown.1678768/
- DSOGaming — "Alan Wake 2 DLSS 3.5 RT/PT Benchmarks" https://www.dsogaming.com/pc-performance-analyses/alan-wake-2-dlss-3-5-ray-tracing-path-tracing-benchmarks/
- Chester Lam, Chips & Cheese — "Cyberpunk 2077's Path Tracing Update" https://chipsandcheese.com/p/cyberpunk-2077s-path-tracing-update
- TweakTown — PS5 Pro specs https://www.tweaktown.com/news/100436/sonys-new-playstation-5-pro-official-45-faster-2-3x-in-rt-launches-nov-7-for-699/index.html
- Epic — "Lumen Technical Details" https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-technical-details-in-unreal-engine — NVIDIA UE5.4 Ray Tracing Guide https://dlss.download.nvidia.com/uebinarypackages/Documentation/UE5+Raytracing+Guideline+v5.4.pdf
- Windows Forum/Technetbook — DXR 1.2 Agility SDK 1.619 release (Feb 2026) https://windowsforum.com/threads/shader-execution-reordering-arrives-in-dxr-1-2-with-sm-6-9-and-agility-sdk-1-619.403960/ and https://www.technetbooks.com/2026/02/shader-model-69-and-agility-sdk-1619.html
