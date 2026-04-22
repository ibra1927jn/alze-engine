# Neural rendering, temporal upscaling, frame generation (2024-2026)

**Round 3 / alze_engine research** — state of the art of ML-assisted real-time rendering, what the IP holders have publicly disclosed, how much of it is reachable from an OpenGL 3.3 C++17 hobby engine, and how much is forever locked inside a vendor SDK.

Scope: DLSS 1-4, FSR 1-4, XeSS, TAA lineage, frame generation, neural texture compression, 3D Gaussian Splatting, neural asset compression, Streamline SDK. NRC is covered in `ray_tracing_2024_2026.md` and only referenced here.

Primary audience: someone who will decide whether ALZE integrates FSR2 at milestone v2 or stays on FXAA+MSAA forever.

---

## 0. Vocabulary cleanup

Three different things are routinely conflated in press releases:

- **Spatial upscaler**: input is one low-res frame, output is one higher-res frame. No temporal information. FSR 1, NIS, Lanczos, bicubic. Quality ceiling is low because information that is missing from a single frame cannot be recovered — you are at best sharpening.
- **Temporal upscaler (TAAU)**: input is a low-res frame *plus* motion vectors *plus* N prior frames (reprojected history). The upscaler reconstructs the full-res frame by fusing history into current. DLSS 2, FSR 2, XeSS, UE's TSR, Unity HDRP TAAU. The "AI" word is optional — some of these are pure hand-crafted heuristics (FSR 2, TSR) and some are learned (DLSS 2, XeSS).
- **Frame generator**: input is two (or more) rendered frames, output is one (or more) synthesised in-between frames. Not an upscaler. Can be layered on top of any upscaler. DLSS 3 FG, FSR 3 FMF, DLSS 4 MFG. Pure interpolation — the generated frame is, by construction, not a response to any input the user gave between rendered frames, so it adds visual smoothness without adding simulation responsiveness.

Do not confuse these axes. "DLSS" on a box can mean any combination of Super Resolution, Ray Reconstruction, and Frame Generation depending on toggles.

---

## 1. DLSS — Deep Learning Super Sampling (NVIDIA)

### 1.1 DLSS 1 (2018-2019, Turing)

First shipping title Battlefield V (Feb 2019), then Metro Exodus, Anthem, FFXV benchmark.

Architecture: per-game CNN, trained on 64×-supersampled "ground truth" frames captured offline at NVIDIA's NGX cloud. Inference on the Tensor cores. Input was the low-res frame only — no motion vectors, no history. Which is why it looked worse than good TAA in most cases and had a reputation for smeary outputs.

The "per-game training" part is the reason it died quickly: every time a title patched a major rendering change, the model needed a re-train trip to NVIDIA. Not scalable.

Public info: NVIDIA blog posts + talks. No paper.

### 1.2 DLSS 2 (2020, Turing/Ampere)

Announced Mar 2020 (GTC), first titles Mechwarrior 5 / Control 1.4. The inflection point — this is the thing that launched the whole category.

Architecture (publicly disclosed, although weights are private):

- Input tensor: jittered low-res color, motion vectors (full-res), depth, per-frame exposure, optional particle/transparency layer.
- A CNN (U-Net style encoder–decoder, per NVIDIA GDC 2020 talks — not a transformer in v2) reprojects the history buffer using motion vectors, mixes it with the current jittered frame, and outputs the full-res target.
- Jitter pattern: Halton(2,3), typically 8-16 samples cycle, applied via projection matrix offset.
- History clamp: neighbourhood-based variance clamping in YCoCg space, akin to Karis 2014 (see §4) but the clamp weights themselves are produced by the network.
- Single *generic* model — not per-game. Training corpus is 100k+ frames across many titles + offline renders.

Performance modes (UHD output):
- Ultra Performance 3× (9×): 1280×720 → 3840×2160.
- Performance 2× (4×): 1920×1080 → 3840×2160.
- Balanced ~1.7× (2.9×).
- Quality 1.5× (2.25×).
- Ultra Quality (added later): 1.3× (1.7×).
- DLAA: native-res denoise, no upscale — same network, scale=1.

Integration cost: ~a week for a competent graphics programmer. Required inputs: jitter projection, per-pixel motion vectors including dynamic+animated geometry (NOT just camera), proper motion-vector encoding for transparencies, LOD/mip bias of -log2(scale) so texture detail stays.

Refs:
- Liu, E. "DLSS 2.0 — Image Reconstruction for Real-time Rendering with Deep Learning." GDC 2020. https://www.nvidia.com/en-us/on-demand/session/gtcsj20-s22698/
- NVIDIA developer blog, Mar 2020: https://developer.nvidia.com/blog/nvidia-dlss-2-0-a-big-leap-in-ai-rendering/
- NVIDIA programming guide PDF shipped with NGX SDK.

### 1.3 DLSS 3 — Frame Generation (Oct 2022, Ada Lovelace)

Ada-only (RTX 4000) because it relies on the Optical Flow Accelerator (OFA) hardware block, significantly upgraded vs Ampere's version.

Pipeline per interpolated frame:
1. Render frame N and N+1 normally (with DLSS 2 SR if enabled).
2. OFA computes dense optical flow between the two rendered frames at roughly quarter-res in ~1-2 ms on AD102.
3. Motion vectors from the engine (only camera+object motion) are fused with OFA flow (which catches shadows, reflections, particles, UI — all the stuff motion vectors lie about).
4. A small CNN produces the intermediate frame.
5. Reflex is *required* in the integration — FG adds ~1 frame of latency by definition (you can only interpolate between two frames you have), Reflex eats back most of it by removing render-queue latency.

Cost: interpolated frame costs ~3-5 ms on AD103-class GPU. Net fps roughly doubles if the non-FG scenario was GPU-bound. If CPU-bound, FG is free performance because the CPU doesn't see the synthetic frame. This is why FG was marketed hard on 4K + heavy RT scenarios.

Known failure modes: HUD ghosting (UE5 widgets mis-masked), particle smearing at disocclusion, text flicker. NVIDIA's fix is an explicit UI-color mask the engine must provide.

### 1.4 DLSS 3.5 — Ray Reconstruction (Sep 2023)

Not an upscaler, a denoiser. Replaces hand-tuned spatio-temporal denoisers (like NRD, SVGF) with a CNN that takes noisy 1-spp or 0.5-spp path-traced inputs + G-buffer guides and produces the denoised frame at full res, all fused with upscaling.

Important: RR is both denoiser *and* upscaler in one pass. You don't run DLSS SR after — you run RR instead of (SR + NRD). Quality in Cyberpunk 2077 path tracing went up visibly for the same cost because the previous pipeline was "NRD denoise → DLSS upscale" and each step destroyed some signal; fused is better.

Requires real ray-traced inputs (lit + unlit + normals + roughness + albedo + metallic). Rasterised rendering gets nothing from RR.

### 1.5 DLSS 4 (Jan 2025, Blackwell)

Announced at CES 2025 together with RTX 50 series.

Two separately shipped changes:

**(a) Transformer-based Super Resolution + Ray Reconstruction.** The CNN backbone was replaced with a vision transformer. NVIDIA hasn't published the architecture, but they have said it is ~2× the parameter count of the previous CNN and uses self-attention over the reprojected history tokens. Quality improvements over DLSS 3 are clear on still captures (less shimmer on thin features: power lines, chain-link fences, hair) but the per-frame cost also went up; Blackwell's beefed-up Tensor cores (fp4 support) are what keeps it real-time. Available on RTX 20/30/40/50 though — the transformer SR is not Blackwell-exclusive, it just runs faster on newer silicon. This was a legitimate surprise.

**(b) Multi-Frame Generation (MFG).** Blackwell-only. Instead of one interpolated frame per rendered pair, you get up to three interpolated frames per pair — so ×3 or ×4 nominal FPS multipliers. This exposes the latency issue of FG much more starkly: at ×4 you are only rendering ~25% of the frames you see. Reflex 2 (with "Frame Warp") partially hides it by re-projecting the latest interpolated frame with the most recent camera pose just before presentation.

Refs:
- NVIDIA DLSS 4 announcement: https://www.nvidia.com/en-us/geforce/news/dlss4-multi-frame-generation-ai-innovations/
- Digital Foundry deep-dive video Jan 2025 — best publicly-available visual comparisons.
- No SIGGRAPH paper (yet) on the transformer SR; NVIDIA Research sometimes files one 1-2 years after deployment (they did this for the Turing/Ampere CNN in a JCGT 2022 article by Edelsten et al.).

### 1.6 DLSS integration footprint

- Linkage: closed-source DLL (`nvngx_dlss.dll`) + NGX runtime.
- Public wrapper: Streamline SDK (§10) or direct NGX SDK.
- License: royalty-free for shipping, but the DLL is redistributable only via official NVIDIA channels.
- Hardware lock: hard. Will not execute on non-NVIDIA GPUs. Will gracefully fall back to "off" if no Tensor cores (Pascal and older).

---

## 2. FSR — FidelityFX Super Resolution (AMD)

### 2.1 FSR 1 (Jun 2021)

Spatial upscale. Two passes:
1. EASU (Edge Adaptive Spatial Upsampling) — a Lanczos-like, edge-direction-aware resampler implemented as a single compute/pixel shader. 12-tap kernel, runtime cost ~0.3 ms at 4K on RDNA2.
2. RCAS (Robust Contrast Adaptive Sharpening) — a localised sharpen that avoids overshoot on already-sharp edges.

Entirely hand-authored HLSL, no ML, no temporal data. Runs on literally anything that supports shader model 6 — NVIDIA, Intel, mobile, Switch. Quality ceiling is "slightly better than bilinear + sharpen" because it has no access to sub-pixel information.

Use case today: integrated GPUs and consoles where even FSR 2's ~1 ms temporal cost hurts. Also used as the upscaler portion of several "performance mode" console presets when there are no jittered MVs available.

Repo: https://github.com/GPUOpen-Effects/FidelityFX-FSR (archived, pre-unified SDK).

### 2.2 FSR 2 (Mar 2022)

AMD's answer to DLSS 2. Temporal upscaler, hand-tuned (no ML), single cross-vendor HLSL/GLSL codebase. Paper by Catto and de Oliveira (AMD) at GDC 2022: https://gpuopen.com/fidelityfx-superresolution-2/ — plus open-source implementation.

Pipeline:
1. Reconstruct previous frame → current camera pose (disocclusion test via depth + velocity).
2. Lock pixels that survived reprojection, accumulate into history buffer.
3. Upsample current jittered frame with lanczos-ish kernel guided by depth/normal similarity.
4. Blend accumulated history with upsampled current via variance-clamped neighbourhood (Karis 2014 style).
5. RCAS sharpen.

No NN. The "intelligence" is all in the hand-authored reactive mask, lock mask, and luminance-aware clamping. About 1.5 k lines of shader code all in.

Quality parity: in motion, FSR 2 holds up against DLSS 2 decently. On thin sub-pixel geometry (power lines, foliage aliasing) DLSS 2 is clearly ahead because its network learned a better reconstruction prior than hand-tuned heuristics can approximate. In still frames the gap is smaller than the marketing suggests.

Performance: ~1.3 ms at 4K on RX 6800. Roughly 2× slower than DLSS 2 on equivalent-tier hardware, but it works on GPUs that have no tensor hardware.

License: MIT. Repo: https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK (FSR 2 + FSR 3 living in the unified FidelityFX SDK).

### 2.3 FSR 3 (Sep 2023)

FSR 2 + frame generation. Two pieces:

**FSR 3 upscaler**: largely identical to FSR 2 with incremental quality fixes (better disocclusion, better reactive-mask exposure).

**Fluid Motion Frames (FMF)**: the FG part. Shader-based optical flow estimator (GPU compute, no dedicated hardware block) + a hand-written frame interpolator. Runs on RDNA, RTX, and Arc — no hardware lock. Cost on RX 7900 XTX at 4K: ~3-4 ms per interpolated frame, a bit higher than DLSS 3 FG on equivalent-tier Ada because there's no OFA.

Anti-Lag 2 plays the role of Reflex for FMF — it's RDNA only in its fancy form but there is a driver-level Anti-Lag+ for vendor-neutral cases. Critical: without Anti-Lag the added latency of FG is very noticeable.

### 2.4 FSR 3.1 (Mar 2024) and FSR 4 (late 2024 / 2025)

**FSR 3.1**: decouples the upscaler from FG so titles can ship FSR 3.1 upscaler + DLSS 3 FG, for example. Also improves temporal stability on fine detail.

**FSR 4**: AMD's first ML-based upscaler. Ships as part of the RDNA4 launch (RX 9070 series, 2025). Uses the new WMMA matrix cores on RDNA4 (int8/fp8). Architecture is a CNN akin to DLSS 2 — AMD has publicly described it as a "temporal reconstruction neural network" but has not published a paper. FSR 4 is RDNA4-only for now; AMD has talked about backporting a reduced-quality fp16 version to RDNA3 but commitment is vague. This finally closes the quality gap with DLSS 2 on supported AMD HW — external reviews (HUB, DF) place FSR 4 between DLSS 3 CNN and DLSS 4 transformer quality.

Refs:
- Catto + de Oliveira, "FidelityFX Super Resolution 2.0", GDC 2022: https://gpuopen.com/gdc-presentations/2022/GDC-FSR2-Catto.pdf
- de Oliveira, "FSR 3", GPUOpen blog Sep 2023.
- FSR 4 launch materials, AMD RDNA4 reviewers' guide Mar 2025.

### 2.5 DLSS vs FSR side by side

| Axis | DLSS 2/3/4 | FSR 2/3 | FSR 4 |
|---|---|---|---|
| ML / hand-coded | ML (CNN → transformer) | hand-coded | ML (CNN) |
| Vendor HW lock | NVIDIA Tensor cores | none, any SM 6 GPU | AMD RDNA4 WMMA |
| Source available | no (DLL) | MIT repo | not yet |
| Quality still image | 5/5 (DLSS 4) | 4/5 | 4.5/5 |
| Quality motion | 5/5 | 3.5/5 (shimmer on thin detail) | 4.5/5 |
| Integration docs | NDA tier + public | public | public |
| Frame gen available | DLSS 3 / 4 MFG | FSR 3 FMF | FSR 3 FMF (via 3.1) |

---

## 3. XeSS — Intel Xe Super Sampling (2022)

Intel's offering. Announced with Arc Alchemist. Key differentiator: *two* code paths from a single binary.

- **XMX path**: uses the XMX matrix engines in Arc GPUs (and Xe2 in Battlemage). int8 inference. Full quality target.
- **DP4a path**: uses DP4a (4-element dot product, int8) which is supported widely on NV Pascal+, AMD RDNA+, Intel Gen11+, etc. Lower quality target (smaller/reduced network).

Architecture: CNN, larger than DLSS 2's. Input set similar: jittered low-res color, motion vectors, depth. Trained on offline supersampled ground truth. Intel has shared less detail than NVIDIA about the network topology but the whitepaper describes an encoder–decoder with skip connections.

Cost: XMX path on Arc is competitive with DLSS 2; DP4a path on a mid-range NV is ~2× slower than DLSS 2 on the same NV card, for similar quality — which means on an NVIDIA GPU you'd never run XeSS DP4a, you'd run DLSS.

The real value of XeSS is political: Intel shipped ONE binary, dual-path, and unlike AMD did not punt on having an ML upscaler. For third-party titles that want one "good upscaler on all vendors" integration, XeSS + DLSS is a reasonable pair because XeSS works on AMD/Intel and DLSS on NVIDIA.

**XeSS 1.3** (late 2024): quality-targeted performance presets — you tell it "I want DLSS-Quality-like visuals" and it picks internal resolution. Also XeSS-FG (frame generation) was announced at CES 2025 but shipped limited titles.

**XeSS 2** (2025): ML-based frame generation + XeLL (low latency), same package model as DLSS 3.

License: custom, mostly OK to ship. Repo + DLL: https://github.com/intel/xess . The source for the DP4a fallback is shared as SPIR-V, the XMX path is a DLL blob.

Refs:
- Intel XeSS white paper, Oct 2022 PDF (linked from the GitHub repo).
- "Intel Xe Super Sampling Programming Guide" shipped in the SDK.

---

## 4. TAA → TAAU → neural upscalers

### 4.1 TAA lineage

The spine of temporal rendering is Karis 2014.

- Karis, B. "High Quality Temporal Supersampling." SIGGRAPH 2014 Advances in Real-Time Rendering course. http://advances.realtimerendering.com/s2014/ (slides PDF).
  - Established the template still used today: jitter via Halton or low-discrepancy sequence; reproject previous frame with per-pixel motion vectors; variance clamp history against the current frame's 3×3 neighbourhood in YCoCg; blend with ~10% current, 90% history; tone-mapping-aware to avoid fireflies.
  - Ghosting fix: clip history against AABB of neighbourhood, not just clamp — Salvi 2016 "An Excursion in Temporal Supersampling" (GDC).
  - Anti-flicker luminance weighting: Karis + Xu 2016.

- Sousa, T. (Crytek) "CryEngine SMAA T2x", SIGGRAPH 2013.
- Pedersen + Lottes, FXAA 3.11 (non-temporal, but the sharpening pair).
- Salvi (Intel) contributions for anti-flicker and better clip.

### 4.2 TAAU

Same pipeline but reconstruction target resolution is higher than render resolution. What makes it work: jitter pattern covers every pixel center at the *output* res over its cycle, so given enough still frames you converge on full-res ground truth. What breaks it: motion disocclusion has no history to reproject from, so those pixels fall back to upsampled current — visible as crawling on moving edges.

UE 4.22 first shipped a production TAAU. UE 5 replaced it with TSR (Temporal Super Resolution), a much improved hand-coded pipeline. TSR is ~2-3 ms at 4K, uses additional guides (coverage mask, translucency layer) to fight ghosting on transparencies. TSR beats FSR 2 in most UE5 titles because it has tighter engine integration.

### 4.3 Modern best practices (what replaced what)

- Ghosting at disocclusion: variance clipping in YCoCg + history rectification by comparing depth/normals. DLSS 2+ uses a learned clamp; FSR 2 and TSR use hand-tuned.
- Thin features (power lines, chain-link): these are the hard case. TAAU fundamentally cannot preserve geometry thinner than one output pixel if it's moving. DLSS 4's transformer handles them best because the attention can "recognise" a linear feature across frames. FSR 2 struggles most here.
- Reactive features (particles, specular, fog): provide a *reactive mask* so the upscaler lowers history weight. This is the single most underused input in indie integrations. Without it you get ghostly smear tails.
- UI: always composite UI *after* upscale (or give the upscaler a UI-mask so it doesn't touch those pixels).
- Translucency: separate layer recommended. Otherwise smoke/glass edges smear.

### 4.4 Comparison

| Technique | Year | Type | Uses ML | Sub-pixel reconstruction |
|---|---|---|---|---|
| SMAA | 2011 | spatial | no | no |
| FXAA | 2011 | spatial | no | no |
| Karis TAA | 2014 | temporal (no upscale) | no | yes |
| TSR (UE5) | 2022 | TAAU | no | yes |
| FSR 2 | 2022 | TAAU | no | yes |
| DLSS 2 | 2020 | TAAU | yes | yes (better) |
| DLSS 4 | 2025 | TAAU + transformer | yes | yes (best) |
| XeSS | 2022 | TAAU | yes | yes |
| FSR 4 | 2025 | TAAU | yes | yes |

---

## 5. Frame generation in detail

### 5.1 Algorithmic shape

Given frames `F(t)` and `F(t+1)` plus motion vectors `MV(t→t+1)`, produce `F(t+0.5)` (or multiple midpoint frames). All production systems today do interpolation — **not** extrapolation — because extrapolation would let the user interact with a frame before it's rendered, at the cost of prediction error on any change in direction (you'd guess wrong every time the player mouse-flicks).

Extrapolation was the "Async Spacewarp" approach used in Oculus VR frame reprojection in 2017 and it's what LSFG (Lossless Scaling) does. Quality is notably worse than interpolation, especially on camera cuts.

### 5.2 Why optical flow matters

Engine motion vectors know about:
- Camera motion.
- Rigid object motion.
- Skinned vertex motion (if the engine bothers to output previous-frame bone transforms, which many don't).

Engine motion vectors do NOT know about:
- Shadow movement (shadows move when light/object moves, but the receiving geometry's MV doesn't).
- Reflection movement (planar/SSR reflections).
- Parallax-occlusion map parallax.
- Most particle and post-process motion.
- UI overlays.

Optical flow computed from the image is how you catch the second list. On Ada, OFA is a dedicated hardware block that does ~300 GOPs of block-matching in 1-2 ms. AMD and Intel do it in shaders, ~3-4 ms, with simpler algorithms (pyramidal Lucas–Kanade). Fusing OFA + engine MVs is the secret sauce of production FG.

### 5.3 Latency and Reflex/Anti-Lag

FG cannot be honest about latency. One-frame interpolation = minimum one extra rendered frame of latency relative to "just render it". In practice the added latency is 5-20 ms depending on base frame-rate. Reflex and Anti-Lag are not "make FG latency zero" — they are "take out the render queue and back-pressure latency that was already there, so the net input-to-photon stays the same as before FG was enabled."

At low base frame-rates FG feels bad because added-latency % is high; NVIDIA's guidance: don't enable FG below ~60 fps base.

### 5.4 Known artefacts

- HUD warp — if the HUD moves slightly between frames (mouse-follow crosshair) it interpolates incorrectly. Fix: UI mask.
- Vertical-bar tearing on fast horizontal pans — aliased MVs at geometry silhouettes.
- "Fried" frame on camera cuts — detector usually catches this and drops the FG frame, causing a 1-frame stutter. Better cut-detection in DLSS 4.
- Text shimmer — tiny high-frequency features are hard for interpolators. Best to draw text at output res post-FG.

---

## 6. Neural Texture Compression (NTC)

Vaidyanathan et al., Intel Labs + NVIDIA, "Random-Access Neural Compression of Material Textures", SIGGRAPH 2023 / also presented as neural BC equivalent at EGSR.
PDF: https://research.nvidia.com/labs/rtr/neural_texture_compression/ (NVIDIA hosts the paper because the follow-up work is at NVIDIA Research).

### 6.1 Idea

Stack of per-texel material channels (albedo RGB, normal XY, roughness, metallic, AO, possibly height — 7-9 channels) is compressed jointly with a small neural network instead of independently per-channel with BC7/BC5. The network has two parts:

- **Latent grid**: per-texel tiny feature vector (e.g. 4 fp8 values per texel, plus a low-res coarse grid). Total stored size ~1-2 bits per texel across all channels.
- **Decoder MLP**: 2-3 tiny hidden layers (≤32 wide) shared across the whole material, that maps (latent, uv) → (albedo, normal, roughness, …).

Decoding is random access — you query a texel's features and run the MLP — which is the prerequisite for using it in a regular fragment shader.

### 6.2 Numbers

Paper reports 4-8× size reduction at equivalent perceptual quality to BC7 across a materials dataset. So a 2K PBR set at ~24 MB via BC7 → ~3-6 MB via NTC.

Decode cost: ~0.2-0.5 ms per full-screen evaluation at 4K on Ampere-class hardware via tensor-core inference. The cost scales with unique materials on-screen; if the same material covers half the screen it's amortised. The paper also shows a path for cooperative-matrix extensions (VK_NV_cooperative_matrix, HLSL 6.6 WaveMatrix) to hit sub-0.1 ms.

### 6.3 Vendor-neutral?

In principle yes — tensor cores are an optimisation; a fallback SIMD path exists. In practice the 2023 reference code was tensor-core-only. NVIDIA then shipped "Cooperative Vectors" in the Agility SDK and announced NTC as a DX/Vulkan-extension-backed feature for RTX Mega Geometry demos in late 2024; that path uses NV-specific cooperative-vector intrinsics but equivalent extensions are coming to Vulkan standardly.

### 6.4 Integration shape

Two-step:
1. **Offline bake**: input BC7+BC5 material → run training script (GPU, ~minutes per material) → output latent grid + decoder weights. Weights are tiny (≤16 KB per material).
2. **Runtime**: fragment shader samples latent grid, runs MLP, returns material channels. Integrates with existing PBR shader as a drop-in replacement for `texture()` calls.

This is the single most "copyable" neural rendering feature for a hobby engine if you accept the offline-bake step. The inference is plain matmul.

### 6.5 Related

- **Neural BRDF compression** — Rainer et al. 2019, neural factorisation of measured BRDFs; not production, but direction-of-travel.
- **Neural material evaluation** — Zeltner et al. "Real-time Neural Appearance Models", SIGGRAPH 2024. Reduces layered BSDF cost by distilling to small MLP.

---

## 7. Neural Radiance Cache — pointer

Covered in `ray_tracing_2024_2026.md §NRC`. One-line recap: Müller et al. 2021, small fully-fused MLP caches irradiance values at world positions; ray tracing terminates into cache after N bounces; cache self-trains during gameplay from actual path-traced samples. NVIDIA RTX GI / Restir GI uses it on RTX Remix, Portal RTX, Alan Wake 2.

---

## 8. 3D Gaussian Splatting (3DGS)

### 8.1 The paper

Kerbl, B., Kopanas, G., Leimkühler, T., Drettakis, G. "3D Gaussian Splatting for Real-Time Radiance Field Rendering." SIGGRAPH 2023. INRIA / MPI.
PDF: https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/
Code: https://github.com/graphdeco-inria/gaussian-splatting (free for academic/non-commercial; commercial needs license).

### 8.2 Algorithm sketch

Scene is represented as a collection of anisotropic 3D Gaussians, each with:
- Position μ ∈ R³
- Covariance Σ (encoded as scale s ∈ R³ + rotation quaternion q)
- View-dependent colour via spherical-harmonic coefficients (usually degree 3 → 48 floats per splat)
- Opacity α

Forward pass:
1. Project each Gaussian to 2D screen space (analytic projection of 3D Gaussian = 2D Gaussian).
2. Sort by depth (tile-based — screen split into 16×16 tiles, per-tile sorted list).
3. α-blend front-to-back within each tile.

The thing is differentiable end-to-end, so training is "SGD on a collection of multi-view photos → point cloud of splats that reproduces the photos". Typical scene: 1-5 million splats, 200-800 MB after training, renders at 60-200 fps at 1080p on a 3090.

### 8.3 Why games noticed

Incredible photo-real capture quality on *static* scenes. You can photograph a room with a phone, run GS training in ~20 min, and have a walkable virtual room with view-dependent specular, glass-like thin geometry, etc. — all at real-time frame rates. Photogrammetry + traditional meshing cannot match the thin-surface quality.

### 8.4 Integration efforts in game engines

- **Unreal Engine**: third-party plugin "XVerse 3DGS for UE5" (2024), "Luma AI Unreal Plugin", internal experiments from Epic. Rendered via a custom primitive in Nanite-style deferred stream, depth integrated into scene depth.
- **Unity**: "UnityGaussianSplatting" by Aras Pranckevičius (github.com/aras-p/UnityGaussianSplatting, MIT, 2023). Decent starting point.
- **Bevy**: `bevy_gaussian_splatting` crate, community.
- **Godot**: proof-of-concept addons.
- Standalone viewers: PlayCanvas Splat Viewer (WebGL), SuperSplat, Antimatter15's web viewer.

### 8.5 What breaks when you try to use splats for opaque game geometry

- **Depth**: splats don't have a well-defined depth — each is a continuous density blob. You need a "median depth" (depth where cumulative α = 0.5) or "peak depth" for shader effects (SSAO, volumetric fog, shadow receive). Both have artifacts.
- **Shadows**: casting shadows *from* a splat cloud requires ray-marching through a density volume or baking into a shadow map with the same α-blending — both expensive and noisy. Receiving shadows on splats is easier.
- **Culling**: per-splat frustum culling is cheap, but you can't easily do occlusion culling (splats are translucent so they don't occlude) without meshifying.
- **Dynamic objects**: a splat cloud is static by construction. 2024 papers ("Deformable 3DGS", "Dynamic 3DGS", "4DGS") extend to dynamic but aren't real-time+high-quality yet.
- **Editing**: moving a group of splats is fine; deforming a surface is hard. No unwrap, no texture edit.
- **Collision**: there is no mesh, no collider. You typically bake a proxy mesh offline.
- **Authoring tools**: nonexistent in the DCC ecosystem. Capture is camera-based or SfM-based.
- **Lighting**: splats are pre-lit in their capture photos; relighting a splat cloud requires separating the appearance into BRDF + environment, which kills the whole "fast capture" premise. Work in progress (Gaussian Shader, Relightable 3DGS).

Verdict for games circa 2026: GS is production-ready for *backdrops* (Disney-style static scenery behind gameplay area), photogrammetry replacement for cinematics, and in VR experiences. Not ready as the primary representation for playable space.

### 8.6 Related work worth naming

- "Mip-Splatting" (Yu 2024) — anti-aliasing for splats (essential, original paper aliased badly on zoom).
- "Scaffold-GS" (Lu 2024) — anchor-based, better memory, better dynamic range.
- "2D Gaussian Splatting" (Huang SIGGRAPH 2024) — planar Gaussians, better surface recovery.
- "Gaussian Frosting" (Guédon 2024) — meshified splats for editing.

---

## 9. Neural asset compression

### 9.1 Neural geometry compression

- **Meta "Neural Geometry Fields"** (2024) — represents geometry as implicit signed distance function via small MLP + feature grid. Random-access evaluation, ~5-10× smaller than equivalent tri-mesh at comparable silhouette fidelity. Decoding cost is the MLP eval per ray/query.
- **NVIDIA "Neural Appearance Models for Geometry"** — research-tier, not productised.
- **Microsoft "Neural Bezier Patches"** (2024) — displacement encoded as MLP on patch parameter space.

None of this is productised in any game engine as of 2026. The reason is that you can do the same job with virtual geometry + Nanite-style streaming without shipping an MLP per mesh, and authoring pipelines around triangles won the last 30 years.

### 9.2 Neural material compression

See §6 NTC — this is the productisable one.

### 9.3 Neural animation compression

- **"Learned Motion Matching"** (Holden 2020 SIGGRAPH) — replaces the motion database with a small NN. Not "asset compression" proper but same spirit.
- **"Phase-Functioned NN"** (Holden 2017).
- DeepMimic, AnimationRL — physics-based, not production.

---

## 10. Streamline SDK (NVIDIA)

Repo: https://github.com/NVIDIAGameWorks/Streamline — MIT for the wrapper, per-plugin licenses inside.

### 10.1 What it is

A vendor-neutral-ish interface that lets your engine call "DoUpscaling()" and "DoFrameGen()" and the right backend (DLSS / FSR / XeSS) runs based on GPU detection. Reflex is also wrapped.

### 10.2 How it actually plays

- Shipped with DLSS, DLSS-G, Reflex, DLSS-RR backends out of the box, all NV-authored.
- Intel shipped a XeSS backend plugin. Works.
- **No official AMD FSR backend.** There is a community FSR 2 backend plugin (`sl.fsr2` in a fork) but AMD never bought in because Streamline is an NVIDIA project and they don't want to ship new features through NVIDIA's abstraction. AMD prefers direct FSR 3 integration.
- Result 2026: Streamline is "NVIDIA SDK + XeSS + community FSR". Fine if you only care about DLSS/XeSS. Not a real vendor-neutral abstraction.

### 10.3 Alternatives

- **DirectSR** (Microsoft, announced GDC 2024, shipping in Agility SDK): DirectX-level SR abstraction. `ID3D12VideoDevice3::CreateSuperResolutionExtension1` roughly. Sits on top of the vendor drivers; at runtime the driver exposes its own SR variant. DLSS/XeSS/FSR all pluggable. Windows-only, D3D12-only.
- **SDL3 GPU and Vulkan Video ext** flirt with standardisation but don't cover SR/FG yet.
- Roll your own wrapper with #ifdef per backend — what most engines do in 2024-2026.

---

## 11. Cross-cut comparison table

| Technique | Developer | Baseline HW | Integration effort (engineer-weeks) | License | Vendor lock | Quality (1-5) |
|---|---|---|---|---|---|---|
| DLSS 2 SR | NVIDIA | Turing+ Tensor | 1-2 | Closed binary, royalty-free | NVIDIA only | 4 |
| DLSS 3 FG | NVIDIA | Ada OFA | 2-3 (needs Reflex + UI mask) | Closed | NVIDIA Ada+ | 4 |
| DLSS 4 SR | NVIDIA | Turing+ (faster on Blackwell) | 1-2 (drop-in w/ Streamline) | Closed | NVIDIA | 5 |
| DLSS 4 MFG | NVIDIA | Blackwell | 3 (latency tuning) | Closed | NVIDIA Blackwell | 4 |
| FSR 1 | AMD | any SM6 | 0.5 | MIT | none | 2 |
| FSR 2 | AMD | any SM6 | 1-2 | MIT | none | 3.5 |
| FSR 3 FMF | AMD | any SM6 (Anti-Lag best on AMD) | 2-3 | MIT | none (Anti-Lag+ is AMD) | 3 |
| FSR 4 | AMD | RDNA4 WMMA | 1-2 | TBD (not released as src) | AMD RDNA4 | 4.5 |
| XeSS XMX | Intel | Arc XMX | 1-2 | custom permissive | Intel best | 4 |
| XeSS DP4a | Intel | any DP4a GPU | 1-2 | same | none | 3 |
| TSR (UE5) | Epic | any DX12/VK GPU | bundled w/ UE5 | UE EULA | UE only | 4 |
| FXAA | NVIDIA | any | 0.2 | MIT-ish | none | 1.5 |
| Karis TAA | Epic/UE | any | 1 | DIY | none | 3 |
| NTC | NVIDIA/Intel Research | tensor/DP4a + offline bake | 4-6 (bake pipeline + shader) | research code, productising | ~none at runtime | 4 (size) |
| 3DGS renderer | INRIA | any compute GPU | 4-8 (renderer + asset pipe) | research non-commercial | depends on impl | 5 (backdrops) |
| Streamline | NVIDIA | N/A wrapper | 1-2 | MIT wrapper | de facto NV-centric | n/a |
| DirectSR | Microsoft | DX12 Agility | 1 | MS OS | Windows/DX12 | n/a |

(Quality scores subjective, calibrated for still-image quality at Quality preset / 4K output.)

---

## 12. ALZE applicability

ALZE today: OpenGL 3.3, SDL2, C++17 (-fno-exceptions -fno-rtti), forward + deferred PBR, CSM, IBL, FXAA, SSAO. No motion vectors, no jitter, no compute shaders (GL 3.3 predates CS).

| Tech | ALZE viable today (v1) | v2 (GL 4.3 + compute + MVs) | v3 (Vulkan + vendor SDK) | Notes |
|---|---|---|---|---|
| FXAA | already in | kept | kept | baseline AA |
| Karis TAA | no (needs MVs + jitter) | **yes** | yes | ~2 weeks to implement |
| TAAU (FSR 2-style) | no | **yes** | yes | requires GL 4.3+ / VK for compute shaders and MRT velocity buffer |
| FSR 1 | **yes — spatial only** | kept as fallback | kept | ~1 day of shader porting, EASU+RCAS are fragment shaders |
| FSR 2 | no | **yes via GL 4.3 fork or VK** | native | reference is HLSL/VK; GLSL port exists community-side |
| FSR 3 FG | no | maybe | yes | needs MVs, compute, present hooks — non-trivial |
| FSR 4 | no | no | no | RDNA4 WMMA, not exposed to GL/VK cleanly |
| DLSS 2/3/4 | no | no | **yes** | requires Vulkan or D3D12, Streamline or NGX, NV-only |
| XeSS | no | no | **yes (DP4a fallback path)** | requires Vulkan + the Intel SDK, same tier as DLSS work |
| NTC runtime decode | no | offline bake only | **yes (VK cooperative matrix)** | biggest win per unit effort if asset sizes matter |
| 3DGS renderer | no | no | **yes (Vulkan, compute)** | nice feature for level backdrops / cinematic envs |
| Neural geometry | no | no | no (too early) | wait for productised tech |

### 12.1 Minimum viable plan for ALZE

1. **v1.x** (no major API bump): ship FSR 1 as a presentation-pass option. EASU + RCAS are two fragment shaders totalling ~400 lines of HLSL/GLSL. No engine changes beyond a render-target-scale toggle. Low-end users get a decent perf knob.
2. **v2** (migrate to OpenGL 4.3 core or Vulkan 1.2 — whichever requires less rewriting of the existing renderer): add motion vectors to the G-buffer / forward pass (encode previous-frame MVP, write per-pixel velocity), add jitter to the projection matrix, implement **Karis TAA** first (one frame of history, variance clamp). About 2 weeks. Then port FSR 2 reference shaders on top of TAA machinery — the infrastructure is the same, FSR 2 is "TAAU with AMD's specific clamp + lock heuristics".
3. **v2 offline**: add NTC baker as an offline tool (even CPU-based at first, minutes per material is fine). Runtime decode in a deferred fetch shader. Target: cut PBR material stream from ~24 MB to ~4 MB per material set. This is the single feature that would meaningfully change memory/IO characteristics.
4. **v3** (Vulkan mature + vendor SDKs): Streamline integration for DLSS / XeSS. FSR 3 FG via the FidelityFX SDK. This is the point where ALZE stops being a hobby engine and starts shipping third-party DLLs. The decision to cross that line should be deliberate.

### 12.2 What ALZE should NOT try

- Training a custom upscaler network. NVIDIA spent hundreds of GPU-years. Wrong problem to solve with one person's time.
- 3DGS as the primary renderer. Interesting tech, not for an ECS-based action engine with dynamic geometry.
- Frame generation before Reflex-equivalent. Adding latency to a 60 Hz fixed-timestep engine is a regression in responsiveness.
- Shipping DLSS while NOT shipping FSR. Optics issue — looks vendor-captured. FSR 2 first, DLSS later.

---

## 13. Key references (consolidated)

- Liu, E. "DLSS 2.0 — Image Reconstruction for Real-time Rendering with Deep Learning." GDC 2020. https://www.nvidia.com/en-us/on-demand/session/gtcsj20-s22698/
- Edelsten, A., Jukarainen, P., Patney, A. "Truly Next-Gen: Adding Deep Learning to Games and Graphics." NVIDIA GDC 2019 (DLSS 1 origins).
- NVIDIA DLSS 3 Frame Generation whitepaper, Sep 2022.
- NVIDIA DLSS 4 launch material, CES 2025; Digital Foundry frame-by-frame analysis Jan 2025.
- Karis, B. "High Quality Temporal Supersampling." SIGGRAPH 2014 Advances in Real-Time Rendering. http://advances.realtimerendering.com/s2014/
- Salvi, M. "An Excursion in Temporal Supersampling." GDC 2016.
- Sousa, T. "CryEngine SMAA T2x." SIGGRAPH 2013.
- Catto, E., de Oliveira, T. "FidelityFX Super Resolution 2.0." GDC 2022. https://gpuopen.com/gdc-presentations/2022/GDC-FSR2-Catto.pdf
- FidelityFX SDK (FSR 1/2/3): https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK
- AMD FSR 4 / RDNA4 reviewer's guide, Mar 2025.
- Intel XeSS white paper, Oct 2022 (shipped in https://github.com/intel/xess).
- Vaidyanathan, K., Salvi, M., McGuire, M., Aila, T. et al. "Random-Access Neural Compression of Material Textures." SIGGRAPH 2023. https://research.nvidia.com/labs/rtr/neural_texture_compression/
- Zeltner, T. et al. "Real-time Neural Appearance Models." SIGGRAPH 2024.
- Kerbl, B., Kopanas, G., Leimkühler, T., Drettakis, G. "3D Gaussian Splatting for Real-Time Radiance Field Rendering." SIGGRAPH 2023. https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/
- Yu, Z. et al. "Mip-Splatting: Alias-free 3D Gaussian Splatting." CVPR 2024.
- Holden, D. et al. "Learned Motion Matching." SIGGRAPH 2020.
- NVIDIA Streamline SDK: https://github.com/NVIDIAGameWorks/Streamline
- Aras Pranckevičius, Unity Gaussian Splatting: https://github.com/aras-p/UnityGaussianSplatting
- Microsoft DirectSR announcement, GDC 2024.
