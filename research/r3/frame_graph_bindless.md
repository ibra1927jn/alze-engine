# Frame Graph + Bindless — Deep-Dive for ALZE Engine (r3)

> Round 3 research on `/root/repos/alze-engine` (C++17 no-RTTI/no-exceptions, SDL2+GL3.3, ~25-30K LOC).
> Scope: render dependency graphs (frame graphs / RDG) and bindless resource models.
> Prior notes (`aaa_engines.md`, `rendering_libs.md`) name-checked these patterns; this file drills into the mechanics, the barrier math, the cross-queue sync, and the resource-lifetime analysis that actually make them work. Where those files pitched "adopt a frame graph when passes exceed ten," this one specifies what that adoption looks like at the code level.

## 1. Frame Graph fundamentals

The canonical reference is Yuriy O'Donnell's "FrameGraph: Extensible Rendering Architecture in Frostbite" (GDC 2017). The goals named in the talk were concrete and remain the litmus test for every FG that followed:

- **Decouple resource creation from use.** Passes declare what they *read* and *write*; the graph decides when a physical resource backs each virtual one.
- **Automate barriers.** The graph knows the full use pattern of every resource, so it inserts `VkPipelineBarrier` / `D3D12_RESOURCE_BARRIER` / GL memory-barrier calls without the author writing one.
- **Alias transient memory.** Resources whose lifetimes don't overlap share the same physical VRAM block.
- **Cull dead passes.** If nothing reads a pass's outputs (transitively from a "present" / "output" sink), the pass is stripped before submission.

The architecture has three phases that every FG implementation repeats in some form:

1. **Setup** — pass callbacks register (create / read / write) against virtual resource handles. No GPU commands yet. Output: a DAG with typed edges.
2. **Compile** — topological sort, resource-lifetime analysis, transient-memory aliasing plan, barrier insertion, queue assignment. All CPU, no GPU work. Output: a linear list of "physical" passes with barrier records attached.
3. **Execute** — the runtime calls each pass's recorded lambda with the compiled resource handles; the pass submits draws/dispatches into a command buffer. Barriers are emitted at the computed points.

The Frostbite talk ships two practical refinements that often get lost in derivative implementations:

- **Virtual resources are descriptors, not allocations.** A virtual `FrameGraphTexture` holds (format, size, mips, layers, usage flags) but no VkImage. Only after compile is the virtual handle resolved to a physical one — possibly shared with another virtual resource.
- **Move semantics for resource versioning.** Each write to a virtual resource produces a *new* virtual handle representing the post-write state. A pass reading "the depth buffer after the geometry pass" addresses a different handle than "the depth buffer as the geometry pass wrote it." This is what lets the DAG stay acyclic even when a pass both reads and writes the same underlying allocation: the version-before edge and the version-after edge go to logically distinct resources.

Graham Wihlidal's "Halcyon / Render Graphs" (SIGGRAPH 2018) added the public pattern for transient memory aliasing computed as an interval-scheduling / bin-packing problem — pseudo-code shipped in the slides is what every indie "my first frame graph" repo (skaarj1989/FrameGraph, acdemiralp/fg, pplux/rg) copies almost verbatim.

## 2. Virtual resource lifetimes

Lifetimes are the whole ball game. Three classes in practice:

### 2.1 Intra-frame transient

Typical candidates: GBuffer RTs, velocity buffer, SSAO result, bloom mips, TAA history read side-effects, particle-sim readback. These live within a single frame. The compile step computes `[first_use_pass, last_use_pass]` intervals and runs a greedy interval-coloring pass: resources whose intervals don't overlap can share physical memory. On DX12 this is literal heap aliasing (`CreatePlacedResource` on the same `ID3D12Heap`), on Vulkan it is `VK_IMAGE_CREATE_ALIAS_BIT` + a shared `VkDeviceMemory` block, on GL it is effectively doing nothing (you keep a pool of textures by descriptor-hash and reissue).

Aliasing wins are real: Frostbite's 2017 talk cited a ~50% reduction in transient RT memory on typical Battlefield levels. UE5's RDG shows similar savings in production.

There is a subtlety: aliased resources need an *aliasing barrier* (DX12: `D3D12_RESOURCE_BARRIER_TYPE_ALIASING`; Vulkan 1.2: explicit `vkCmdPipelineBarrier` with `srcAccessMask` covering the previous alias's writes and image-layout init to `VK_IMAGE_LAYOUT_UNDEFINED`). Skipping this is a classic first-FG-bug that shows up only under driver-specific memory reuse patterns (AMD tends to tolerate it; NVIDIA punishes it with stale data).

### 2.2 Cross-frame persistent

History buffers (TAA previous frame, reprojection caches, Hi-Z pyramid fed to next-frame culling, denoiser accumulators, light cluster persistent state). These are registered as *external* resources — pre-allocated VkImages / ID3D12Resources whose lifetime the game owns. The FG still reasons about their per-frame use: it knows pass N+3 reads the history that pass N+1 of the *next* frame will write, so it emits the correct barrier transitioning from SRV read to RTV write at frame boundary.

Frostbite handles this via `importExternal(handle)` — the handle bypasses transient-aliasing but participates in barrier tracking. UE5's RDG spells it `RegisterExternalTexture`.

### 2.3 Persistent but internal

Shadow atlases, virtual-texture page caches, clipmap layers, probe atlases. Conceptually persistent; the FG treats them as external-imported but the engine itself owns the allocation. The only difference from 2.2 is who is expected to free them.

### 2.4 Pool sizing discipline

A hidden benefit of doing lifetime analysis: the FG knows the peak concurrent transient footprint. Frostbite logs this per-frame and the pool shrinks (or refuses to grow) to match the observed peak + a small margin. Without a graph, engines tend to grow transient pools monotonically and never shrink, because no single subsystem has end-to-end visibility.

## 3. Barrier generation

This is where the graph earns its keep; hand-writing barriers is the #1 source of GPU hangs and validation-layer yelling in raw Vulkan/DX12.

### 3.1 What a barrier is for

Two jobs:
- **Execution dependency** — ensure previous work finishes before later work starts (stage-mask to stage-mask on Vulkan; subresource-state transitions on DX12).
- **Memory availability + visibility** — caches flushed in producer, invalidated in consumer (access-mask to access-mask on Vulkan; implicit in DX12's state transitions).

### 3.2 What the FG does

For every (producer pass P, consumer pass C) edge on resource R, the compile step emits:

- `srcStageMask` = union of stages P writes R in (e.g. `COLOR_ATTACHMENT_OUTPUT` for an RT, `COMPUTE_SHADER` for a UAV).
- `srcAccessMask` = union of accesses P performed (e.g. `COLOR_ATTACHMENT_WRITE`, `SHADER_WRITE`).
- `dstStageMask` = union of stages C uses R in (e.g. `FRAGMENT_SHADER` for a sampled read).
- `dstAccessMask` = `SHADER_READ`, etc.
- For images, the `oldLayout` → `newLayout` transition (`COLOR_ATTACHMENT_OPTIMAL` → `SHADER_READ_ONLY_OPTIMAL`, etc.).

### 3.3 Split barriers (DX12)

DX12 exposes `D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY` + `_END_ONLY` so a producer can *start* the transition at its end and the consumer *completes* it right before use. GPUs with long pipelines (GCN, RDNA) overlap the transition with whatever else is scheduled between P and C. A non-split barrier blocks the whole queue at the transition point; split barriers can be effectively free. The FG is the only place with enough information to place both halves — the producer pass's record gets the BEGIN, the consumer's gets the END, and the "anywhere between them" is the compute/graphics work that overlaps.

Vulkan lacked this granularity until `VK_KHR_synchronization2` (core in 1.3) introduced `VkDependencyInfo` with `pMemoryBarriers2` entries that allow event-based split equivalents via `VkEvent` + `vkCmdSetEvent2` / `vkCmdWaitEvents2`. In practice Vulkan FGs either emit pipeline barriers at use time (simpler, small perf loss) or chase events for the biggest transitions. UE5's RDG Vulkan backend uses events sparingly — only where profiling justifies the complexity.

### 3.4 VkPipelineBarrier2

`VkPipelineBarrier2` (synchronization2, core Vulkan 1.3, widely available since 2022) is the barrier API a modern FG targets. Key wins over the 1.0 `VkPipelineBarrier`:

- Per-barrier stage/access masks instead of global masks — the driver has tighter bounds.
- `VkDependencyInfo` is a struct, not scalar args — scales to batched barriers cleanly.
- New stage constants (`ALL_GRAPHICS`, `COPY`, `RESOLVE`, `BLIT`, `CLEAR` separated from `TRANSFER`) let the FG be precise without over-ordering.

An FG that emits Sync2 barriers typically reduces GPU bubbles by single-digit percent vs Sync1 on complex frames — the 2022 Hans-Kristian Arntzen blog post on RADV's Sync1-vs-Sync2 wash-out shows the pathological cases where Sync1 implies way more ordering than the graph actually requires.

### 3.5 How UE5's RDG does it

Unreal's Render Dependency Graph is a direct descendant of Frostbite's FG with one pragmatic twist: *every* engine render path funnels through RDG, not an opt-in subset. This forced Epic to handle exceptional cases cleanly:

- Per-resource state is tracked at *subresource* granularity (mip + slice) — shadow atlases with per-slice writes don't force whole-resource barriers.
- Immediate-mode shims exist for code that must interleave non-RDG passes (e.g. third-party plugins), via `FRDGBuilder::QueueTextureExtraction` and explicit flush points.
- Barriers are emitted at pass boundaries by default but can be hinted with `ERDGPassFlags::SkipRenderPass` or `NeverCull` for profiling-driven optimizations.

The RDG's barrier logic is in `RenderGraphBuilder.cpp` + `RenderGraphResources.h` inside UE5's engine source; around 6-8k LOC handle the state machine. For ALZE, the lesson is that *subresource-granular* tracking is essential once shadow atlases / texture arrays enter the picture — whole-resource tracking forces stalls that shouldn't be there.

## 4. Async compute dispatch

The FG is the natural place to express "this pass runs on the async compute queue" because queue assignment is just another coloring of the DAG.

### 4.1 Why async compute

Modern GPUs expose multiple command queues (AMD GCN/RDNA: 1 graphics + N compute; NVIDIA Turing+: 1 graphics + 1 compute + N copy; Intel Xe: similar). Graphics-bound work (fragment-heavy passes) and compute-bound work (SSAO, bloom downsample/upsample, GPU particles, light culling, BVH refit) do not compete for the same execution units *if scheduled on separate queues*. The hardware can saturate both ALU and fixed-function units simultaneously.

On consoles this is the difference between hitting 60 Hz and missing: PS4/PS5 and Xbox Series explicitly market async-compute overlap as a performance lane. Jason Gee, Alex Dunn, and others at GDC 2015-2017 showed 5-15% wall-clock gains per frame on typical scenes.

### 4.2 Scheduling

The FG compile step partitions passes into queue lanes:

- Graphics queue — anything that touches rasterization / render targets / depth / swapchain.
- Compute queue — pure compute dispatches flagged `AsyncCompute` at declaration time.
- Copy queue — uploads, readbacks, mip generation when separable.

Cross-queue dependencies become **timeline semaphores** (Vulkan `VK_KHR_timeline_semaphore`, core 1.2) or **fences with monotonic values** (DX12 `ID3D12Fence::Signal`/`Wait`). The FG emits one semaphore signal at the producer's queue submission and one wait at the consumer's. Timeline semaphores matter because they can be waited on by CPU *and* GPU and allow wait-before-signal ordering, which is friendlier to dynamic FG rebuilds than binary semaphores.

Cross-queue resources need an ownership transfer: `VkImageMemoryBarrier` with `srcQueueFamilyIndex` != `dstQueueFamilyIndex` on both sides, or equivalent DX12 state transition under a shared state. Forgetting the ownership transfer is another classic bug — works on UMA (integrated) GPUs, breaks on discrete with separate queue families.

### 4.3 PS5 / Xbox Series X realities

Console async compute scheduling is tighter than desktop because the hardware is known. The PS5 GNM driver exposes direct ring-buffer control; Xbox DX12X is closer to desktop DX12 but with ExecuteIndirect optimizations absent on PC. The Frostbite and id Tech async-compute patterns (particles + post on compute, shadow & G-buffer on graphics) ship identically on PS5 and Series X because the FG hides the tuning.

Practical list of "this should be async-compute" passes, compiled from Doom Eternal, Horizon Forbidden West, Alan Wake 2 post-mortems:

- Bloom downsample + upsample chain.
- SSAO / GTAO.
- Screen-space reflections (non-RT path).
- Depth-of-field blur.
- Motion blur.
- Particle simulation (force integration, sort, cull).
- Skinning compute (outputs to position buffer consumed next-frame by geometry pass).
- BVH refit for RT (where applicable).
- Light clustering / tile-frustum assignment.
- Volumetric froxel fog injection + scattering.

### 4.4 What async compute loses

Overlap is not free:
- Cross-queue semaphores have non-trivial CPU cost and some GPU-side sync overhead.
- Resource ownership transfers force the image to canonical layouts (often `GENERAL`), losing optimal-layout caching.
- Debugging correlated bugs across queues is painful — PIX / RenderDoc capture them but interpreting a multi-queue timeline is an acquired skill.

A rule of thumb from the Frostbite talk: async-compute a pass only if its wall-clock is long enough to cover the semaphore latency (~50-200 μs). Short compute passes (< 100 μs) gain nothing and may regress.

## 5. Multi-GPU frame graph (legacy)

Explicit multi-GPU (DX12 Linked Node Adapter, Vulkan VK_KHR_device_group) had a brief window of relevance (2016-2021) before NVIDIA effectively retired SLI on Ampere (2020) and AMD stopped pushing CrossFire. Including for completeness:

- **AFR (Alternate Frame Rendering).** GPU0 renders frame N, GPU1 renders N+1. The FG replicates per GPU; cross-GPU dependencies arise mostly from history buffers (TAA needs the previous frame's output) and must be copied via PCIe. Peer-to-peer copies on supported hardware are cheap enough to keep AFR at 1.6-1.8× scaling on well-behaved content. Real-world: modern TAA-heavy pipelines cap scaling lower because of the inter-frame dependency chain.
- **Split-frame.** Screen divided into N regions, each GPU owns one. The FG has to partition screen-space passes and stitch results at the composite pass — which itself has a large cross-GPU dependency. Rarely ships post-2018 because overhead vs gains flipped the wrong way.
- **Explicit heterogeneous.** E.g. iGPU for post, dGPU for main render. DX12 supports it; nobody ships it outside the Intel Adaptive Sync demos because driver fragility is too high.

ALZE should ignore multi-GPU entirely. It is a legacy bullet and the engineering cost does not justify it for a small team's target audience.

## 6. UE5 Render Dependency Graph (RDG)

Epic's RDG is the most widely deployed FG variant today — every Unreal render pass from Nanite to Lumen to translucency to PostProcessing flows through it. Worth studying for the *public API shape* alone, since it encodes many pragmatic compromises.

### 6.1 Pass declaration

```cpp
// Parameter struct is the contract: inputs/outputs typed.
BEGIN_SHADER_PARAMETER_STRUCT(FMyPassParameters,)
    SHADER_PARAMETER(FVector2f, ViewportSize)
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColor)
    SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, Output)
    SHADER_PARAMETER_SAMPLER(SamplerState, LinearSampler)
END_SHADER_PARAMETER_STRUCT()

// Registering a pass: pass name, parameters, flags, lambda.
FMyPassParameters* Parameters = GraphBuilder.AllocParameters<FMyPassParameters>();
Parameters->ViewportSize = View.ViewRect.Size();
Parameters->SceneColor = SceneColorTexture;
Parameters->Output = GraphBuilder.CreateUAV(OutputTexture);
Parameters->LinearSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();

GraphBuilder.AddPass(
    RDG_EVENT_NAME("MyPostPass"),
    Parameters,
    ERDGPassFlags::Compute,
    [Parameters, Shader, ViewExtent](FRHIComputeCommandList& RHICmdList) {
        FComputeShaderUtils::Dispatch(
            RHICmdList, Shader, *Parameters,
            FComputeShaderUtils::GetGroupCount(ViewExtent, FIntPoint(8,8)));
    });
```

Four architectural choices worth copying:

1. **Parameters are a single typed struct per pass.** The RDG knows every resource the lambda can touch because the lambda closes over `Parameters` and RDG walked the struct at registration. This is the inference mechanism — no manual `.read(X).write(Y)` calls needed; the struct's field types (`SHADER_PARAMETER_RDG_TEXTURE` vs `_TEXTURE_UAV`) encode the access.
2. **Allocator in the graph builder.** `GraphBuilder.AllocParameters<T>()` uses an arena freed at graph-end; no `new`/`delete` churn per-frame.
3. **Lambda captures by value.** The pass-record closure is opaque; it runs at execute time, after compile, against resolved RHI handles. Captures must be POD or carefully ref-counted.
4. **Event names are macro-wrapped.** `RDG_EVENT_NAME("...")` collapses to nothing in shipping builds but generates PIX/RenderDoc markers in debug — profiling discipline wired into the API.

### 6.2 Integration with Nanite / Lumen

Nanite's visibility buffer pipeline is expressed as ~20 RDG passes (cluster culling, raster setup, software raster dispatch, hardware raster dispatch, material classify, material resolve). Each is a pass with explicit RDG resources; the graph culls unused material bins automatically when a given frame doesn't need them.

Lumen's screen-space / world-space probe pipeline is ~60+ passes including radiance caching, world-probe update, screen-probe gather, final gather, and denoising. The RDG's DAG culling is what keeps Lumen's runtime cost workload-proportional — scenes without dynamic geometry skip whole subgraphs.

The RDG documentation lives at the Epic dev-community page (recent URL `dev.epicgames.com/documentation/en-us/unreal-engine/render-dependency-graph`; older `docs.unrealengine.com/.../RenderDependencyGraph` redirects). The open-source engine ships the implementation in `Engine/Source/Runtime/RenderCore/Public/RenderGraph*.h` and `.../Private/RenderGraph*.cpp`.

### 6.3 Why ALZE cannot just port RDG

Size. RDG is ~15-20k LOC if you count parameter-struct macros, reflection, and the RHI layer it assumes. ALZE would implement a ~2-3k LOC subset without parameter reflection — just direct `.read()`/`.write()` calls in a setup lambda. Still gets 80% of the benefit.

## 7. AMD FidelityFX FrameGraph

AMD's GPUOpen team open-sourced FFX FrameGraph in 2024 under the broader FidelityFX SDK. It is deliberately smaller and more prescriptive than UE5's RDG — targeted at developers integrating FidelityFX effects (FSR, CACAO, VariableShading) who need a graph but don't want to adopt an engine.

What it provides:
- A C-friendly pass-registration API (`ffxFrameGraphAddPass`).
- Transient resource aliasing with a simple first-fit interval scheduler.
- Barrier generation for DX12 and Vulkan.
- A sample integration against the Cauldron framework that AMD also open-sources.

What it does not provide:
- Async compute support is minimal (barrier-on-same-queue only in early versions; later versions added multi-queue but it is less battle-tested).
- No subresource-granular state tracking.
- No shader-parameter reflection; authors pass resource handles explicitly.

Parts that generalize well:
- The aliasing scheduler is ~300 LOC and copy-pastable.
- The DX12/Vulkan barrier emission table maps cleanly to a small engine.
- The C API shape matches an RHI with opaque handles (bgfx-ish).

GitHub: `GPUOpen-LibrariesAndSDKs/FidelityFX-SDK` — the `sdk/src/backends/*/FrameGraph*` subtree is self-contained enough to read in an afternoon.

## 8. Bindless resources

"Bindless" is the umbrella term for descriptor models where shaders index into a large pool of resource descriptors via integer indices rather than binding resources to fixed slots before each draw.

### 8.1 Vulkan 1.2 descriptor indexing

Core in Vulkan 1.2 (was `VK_EXT_descriptor_indexing`). Enables:
- `VkDescriptorSetLayoutBindingFlags::UPDATE_AFTER_BIND_BIT` — descriptors can be updated after the set is bound but before draw.
- `PARTIALLY_BOUND_BIT` — not every array slot needs a valid descriptor at bind time.
- `VARIABLE_DESCRIPTOR_COUNT_BIT` — the last array binding's count can be specified at allocation time.
- `shaderSampledImageArrayNonUniformIndexing` and related features — `textures[material.albedo_index]` works even when the index varies across the wavefront (it is *uniform* across the wavefront on most hardware today thanks to driver wave-uniform-load instructions, but the spec feature allows divergence).

Typical setup: one descriptor set with a single binding of type `SAMPLED_IMAGE` and count = N (e.g. 16k or 65k). Bound once at frame begin. Shaders declare `layout(set = 0, binding = 0) uniform texture2D textures[];` and index it.

### 8.2 DX12 resource descriptors via root signatures

DX12's equivalent is CBV/SRV/UAV descriptor heaps with `ResourceDescriptorHeap[]` and `SamplerDescriptorHeap[]` arrays in SM 6.6 (HLSL 2021). Earlier versions required unbounded descriptor tables with a root parameter. The SM 6.6 `ResourceDescriptorHeap[index]` syntax is the modern cleanest form — no explicit root signature array parameter, the heap itself is addressable.

Tier requirements: Resource Binding Tier 3 for unbounded descriptor tables. Supported on AMD GCN+, NVIDIA Maxwell+ (effectively all discrete GPUs since 2014).

### 8.3 Bindless textures vs bindless buffers

Bindless textures are almost universal. Bindless buffers (indexing into an SSBO / UAV heap) are increasingly common — UE5 uses them for Nanite cluster data, material parameter records, and instance data.

The key ergonomic: with bindless, a *single* descriptor set / root signature is the whole binding model for an entire frame. Per-draw bind cost collapses to a push-constant-sized material index + transform index.

### 8.4 Performance wins

From Doom Eternal's SIGGRAPH 2020 talk: the entire frame is drawn with on the order of a few hundred draws total, with a single descriptor table holding ~10-20k textures. Material data lives in an SSBO keyed by material index. Per-draw CPU cost is close to the theoretical floor.

From Alex Tardif's widely-cited "Bindless Rendering" blog: measured CPU cost of descriptor binding dropped from ~30% of frame CPU time on a legacy binding model to ~3% after going full bindless, on a synthetic 10k-draw scene. Actual production gains are smaller (5-15% CPU frame time) but consistent.

## 9. Bindless materials in Frostbite

Johan Andersson's Mantle/DX12 Frostbite talks (SIGGRAPH 2012-2016) established the pattern; Sebastian Aaltonen's GDC and Twitter writing popularized it for indies.

The Frostbite pattern:

- One global texture array (bindless descriptor heap).
- One `MaterialRecord` SSBO:
  ```
  struct MaterialRecord {
      uint albedo_tex_idx;
      uint normal_tex_idx;
      uint mr_tex_idx;          // metal-roughness
      uint emissive_tex_idx;
      vec4 base_color_factor;
      vec2 mr_factor;
      float cutoff;
      uint flags;
  };
  layout(std430) buffer Materials { MaterialRecord materials[]; };
  ```
- Per-draw push constant: `uint material_id, uint transform_id, uint skin_id`.
- Shader pseudocode:
  ```glsl
  MaterialRecord m = materials[pc.material_id];
  vec4 albedo = texture(textures[m.albedo_tex_idx], uv) * m.base_color_factor;
  ```

This replaces "set active material" (which was a whole descriptor-set / root-signature switch, often ~500 ns CPU per draw on legacy paths) with an integer-indexed load (~zero CPU cost, one GPU scalar ALU op).

Aaltonen's contributions that ALZE should read: the "GPU-Driven Rendering Pipelines" GDC 2015 talk with Ulrich Haar (Ubisoft), and the follow-up "Optimizing the Graphics Pipeline with Compute" (Aaltonen, GDC 2016). The combination of GPU-driven draw generation + bindless materials is what modern AAA scene submission looks like.

## 10. Trade-offs and failure modes

### 10.1 Lost per-draw validation

Pre-bindless, every descriptor bind went through the driver, which could validate that a bound resource matched shader expectations (format, sample count, dimensions). Bindless puts that validation on the app — the shader trusts `textures[idx]` is a 2D color texture at slot `idx`.

Bugs become ugly: wrong-format sampling gives garbage, out-of-range indices give undefined behavior (often garbage + occasional GPU hang). Discipline: reserve a "null/debug-pink" descriptor at index 0 so unassigned indices read visibly.

### 10.2 Debugger handling

PIX and RenderDoc reconstruct resource state by replaying commands with their own tracking. Pre-bindless, they could display "this draw bound texture X" trivially. Post-bindless, they must inspect SSBO contents + descriptor heap + shader source to figure out which textures a given draw *actually* sampled. Both tools handle it now (2024+), but early bindless adoption was a profiling nightmare.

RenderDoc has special UI for descriptor indexing since 1.21: it shows the heap and lets you click through to what each draw sampled, reconstructed from shader reflection. NVIDIA Nsight similar.

### 10.3 Mobile / tile-based renderers

Bindless collides with tile-based deferred rendering (TBDR) found on Apple M-series, Qualcomm Adreno, ARM Mali, Imagination PowerVR. TBDR relies on knowing the set of resources a tile will touch for its on-chip tile memory + binning. Truly unbounded descriptor arrays break this model — the tile binner cannot assume which textures are in play.

Practical upshot:
- iOS Metal supports `argument buffers` as a bindless-ish path, but with tier-2 restrictions and residency hints the app must provide.
- Android Vulkan drivers vary wildly: some support descriptor indexing, some don't. Feature query is mandatory.
- Tile residency hints (`makeResident`) on Metal let the driver prefetch before the tile begins.

Filament's Mobile-first design avoids bindless for exactly this reason — it still uses traditional descriptor sets per material. This is a legitimate engineering choice if mobile is a first-class target. For ALZE (desktop-first per project notes), bindless is a clean win.

### 10.4 Heap pressure

Bindless heaps are sized at creation (e.g. 64k descriptors). If content exceeds the heap, you either grow (requires recreation + all bindings re-bound) or evict. Streaming engines need a descriptor-slot allocator with free-list semantics; the allocator itself becomes a small fixed system. Order of 200-400 LOC.

### 10.5 Async compute + bindless interaction

Descriptor sets bound on one queue do not automatically apply to the other. Each queue submission needs its own bindless descriptor set bind. Trivial; just remember it.

## 11. Comparison table

| Axis | bgfx (sort key) | O'Donnell FrameGraph (Frostbite) | UE5 RDG | AMD FFX FG |
|---|---|---|---|---|
| Barrier generation | Manual via explicit state calls; GL-era implicit | Auto, whole-resource granular | Auto, subresource granular, sync2-capable | Auto, whole-resource |
| Transient aliasing | None — pool of sized targets | Yes, interval coloring | Yes, interval + priority hints | Yes, first-fit interval |
| Async compute support | None (single queue model) | Yes, first-class queue lanes | Yes, per-pass `AsyncCompute` flag | Minimal; same-queue preferred |
| Subresource tracking | N/A | Per-resource | Per-mip/per-slice | Per-resource |
| Dead-pass culling | N/A (sort key is flat) | Yes, DAG-walk from sinks | Yes | Yes |
| LOC complexity (impl) | ~3-5k total lib; RG-equivalent = 0 | ~5-8k for core FG (original Frostbite) | ~15-20k including parameter reflection | ~2-3k core + integration glue |
| API surface | `submit(key, state)` | `pass.read/write + setup + execute` lambdas | `BEGIN_SHADER_PARAMETER_STRUCT + AddPass` | C-style `ffxFrameGraphAddPass` |
| Editor integration | None | Internal Frostbite tools only | Unreal Insights + GPU Visualizer deep | No editor — SDK integration |
| Debugger affinity | Bind-level, trivial | Pass-level, explicit | Pass-level, rich (RDG Viewer) | Pass-level, basic |
| Multi-queue semaphores | N/A | Timeline semaphores (Vulkan) / fences (DX12) | Same + custom fence pool | Timeline / fence |
| Shader param reflection | Implicit via bgfx uniforms | No (manual binding) | Yes, macro-generated | No |
| Cross-frame import | N/A | `importExternal` | `RegisterExternalTexture` | `ffxFrameGraphImport` |
| License | BSD-2 | Closed (Frostbite proprietary) | Custom Unreal EULA | MIT (FidelityFX SDK) |
| Publicly buildable | Yes | No (only the concepts are public) | Yes with UE5 source | Yes |

## 12. Primary references

- O'Donnell, Yuriy. "FrameGraph: Extensible Rendering Architecture in Frostbite." GDC 2017. GDC Vault video: `https://www.gdcvault.com/play/1024612/FrameGraph-Extensible-Rendering-Architecture-in`. Slides mirror: `https://gpuopen.com/wp-content/uploads/slides/GDC_2017_FrameGraph.pdf` (AMD-hosted copy, archive).
- Wihlidal, Graham. "Halcyon + Vulkan — Render Graphs." SIGGRAPH 2018 (EA SEED). Slides: `https://www.ea.com/seed/news/siggraph-2018-halcyon-architecture`; PDF archive via `web.archive.org/web/2019*/https://www.ea.com/seed/news/siggraph-2018-halcyon-architecture`.
- Aaltonen, Sebastian + Haar, Ulrich. "GPU-Driven Rendering Pipelines." GDC 2015. `https://www.advances.realtimerendering.com/s2015/aaltonenhaar_siggraph2015_combined_final_footer_220dpi.pdf`.
- Aaltonen, Sebastian. "Optimizing the Graphics Pipeline with Compute." GDC 2016. `https://www.gdcvault.com/play/1023487/Optimizing-the-Graphics-Pipeline-with`.
- Geffroy, Jean; Wang, Yixin; Gneiting, Axel (id Software). "Rendering the Hellscape of Doom Eternal." SIGGRAPH 2020. `https://advances.realtimerendering.com/s2020/RenderingDoomEternal.pdf`.
- Andersson, Johan. "Parallel Graphics in Frostbite — Current & Future." SIGGRAPH 2009. `https://www.slideshare.net/repii/parallel-graphics-in-frostbite-current-future-siggraph-2009-1860503`. Later Mantle/DX12 talks collected at `https://repi.se/`.
- Epic Games. "Render Dependency Graph" documentation. `https://dev.epicgames.com/documentation/en-us/unreal-engine/render-dependency-graph`. Older URL `https://docs.unrealengine.com/5.0/en-US/Programming/Rendering/RenderDependencyGraph/` redirects.
- AMD GPUOpen. "FidelityFX SDK" (includes FrameGraph). `https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK`. Blog overview: `https://gpuopen.com/fidelityfx-sdk/`.
- Khronos. Vulkan 1.2 specification, chapter on "Descriptor Indexing." `https://registry.khronos.org/vulkan/specs/1.2-extensions/html/chap15.html#descriptorsets-descriptor-indexing`.
- Khronos. Vulkan synchronization2 deep-dive (Tobias Hector). `https://www.khronos.org/blog/understanding-vulkan-synchronization`.
- Arntzen, Hans-Kristian. "Synchronization2 and Vulkan." (Themaister blog, 2021) `https://themaister.net/blog/2021/02/13/an-opinionated-post-on-modern-rendering-abstractions/`. Additional Vulkan bindless/FG commentary at `https://themaister.net/blog/`.
- Ong, Jeremy. "Vulkan Bindless Programming." `https://jeremyong.com/vulkan/descriptor-indexing-in-vulkan/` (archive fallback via web.archive.org).
- Microsoft DirectX Team. "SM 6.6 — Dynamic Resources (ResourceDescriptorHeap)." `https://devblogs.microsoft.com/directx/hlsl-shader-model-6-6/`. Resource binding tiers: `https://learn.microsoft.com/en-us/windows/win32/direct3d12/hardware-feature-levels` (subtree on Resource Binding Tier 3).
- Tardif, Alex. "Bindless Rendering." `https://alextardif.com/Bindless.html`.
- Open-source FG reference implementations to read:
  - `https://github.com/skaarj1989/FrameGraph` — ~2k LOC C++17, Vulkan-only, closest to O'Donnell's talk.
  - `https://github.com/acdemiralp/fg` — single-header C++17, ~1k LOC, no backend (BYO RHI).
  - `https://github.com/pplux/rg` — tiny render graph in ~500 LOC, useful for algorithm study.
  - `https://github.com/EmbarkStudios/kajiya` — Rust, Vulkan, has a production-quality render graph + bindless path; read for the async-compute scheduling patterns.
- Coenen, Simon. "DOOM Eternal — Graphics Study." `https://simoncoenen.com/blog/programming/graphics/DoomEternalStudy`. Shows bindless + FG integration in a shipped AAA title.
- Courrèges, Adrian. "DOOM (2016) — Graphics Study." `https://www.adriancourreges.com/blog/2016/09/09/doom-2016-graphics-study/`. Pre-bindless id Tech 6; useful as contrast.

## 13. ALZE applicability

### 13.1 Current state

ALZE (as of r3) has no render graph. The GL 3.3 renderer issues draws in roughly submission order, with explicit bind / uniform / texture calls per draw. Pass count is low (~5-8: shadow, main, skybox, transparency, post). Transient resources (bloom mips, temp buffers) are allocated from a hand-rolled pool with no lifetime analysis. Barriers are nonexistent — GL's implicit synchronization handles it (`glMemoryBarrier` is called at a couple of compute boundaries).

### 13.2 V1 (current-scale): sort-key only

At ~5-8 passes and GL 3.3, a full FG is over-engineered. The bgfx-style sort-key submission pattern (see `rendering_libs.md` §"bgfx") gives 80% of the state-change-minimization win with 5% of the code.

- 64-bit key: `[view:4][pass:4][blend:2][depth:24][material:16][instance:14]`.
- Submit into a pool; radix-sort at frame end; iterate sorted and dispatch through GL.
- Target: ~300-500 LOC incremental over current renderer.
- What this does *not* do: barrier generation, transient aliasing, async compute. All N/A at GL 3.3.

### 13.3 V2 (Vulkan/D3D12 migration): minimal FrameGraph (~2-3k LOC)

Once ALZE introduces an explicit-API backend (Vulkan first per project direction), the FG stops being optional. Scope:

- Pass declaration API: a `Setup(builder)` lambda calls `builder.read(tex)` / `.write(tex)` / `.create(desc)`; `Execute(ctx, resources)` lambda dispatches. Around 400 LOC for the API.
- Virtual → physical resolution: ~300 LOC for resource descriptor dedup + transient pool.
- Interval-coloring aliasing: 200-400 LOC, direct port of Halcyon / FFX FG.
- Barrier generation: ~800 LOC for Vulkan sync2 (whole-resource granular is fine at this stage — skip subresource until shadow atlases force it).
- Async compute queue assignment: ~200 LOC including timeline-semaphore signal/wait bookkeeping.
- Dead-pass culling: ~100 LOC DAG walk from sinks.
- Debug/introspection: dump the compiled graph as GraphViz DOT for offline inspection — ~50 LOC, saves hours of debugging.

Subtotal: ~2-2.5k LOC, one engineer-month to design + ship with tests.

Big design call: **do parameter structs like UE5 or explicit read/write calls like Frostbite?** For a 30k LOC engine without reflection infrastructure, explicit calls win. The UE5 parameter-struct macros depend on a whole reflection subsystem (Unreal Header Tool) that ALZE does not have and should not build.

### 13.4 V3 (post-Vulkan): bindless + async compute

With FG in place, bindless slots in almost for free:

- One global texture descriptor set (16k images, 4k samplers).
- One material SSBO with `MaterialRecord` entries.
- Push constants carry `material_id` + `transform_id` + `skin_id`.
- `textures[]` array in shaders.

Async compute piggybacks on the FG's queue-lane assignment already built in V2: just flag post-FX passes as `AsyncCompute` and the graph emits the timeline semaphores.

Things to plan for in V3:
- **Descriptor slot allocator.** ~300 LOC free-list allocator, with a reserved null-debug-pink descriptor at slot 0 for unassigned indices.
- **Streaming-aware descriptor updates.** When a texture streams in, allocate a slot, write the descriptor under `UPDATE_AFTER_BIND`, hand the slot to whoever owns the material record. On unload, return the slot.
- **Shader variant reduction.** With bindless, many shader permutations collapse — `HAS_NORMAL_MAP` becomes a zero-test on `material.normal_tex_idx` at runtime. Expect 2-4× reduction in compiled pipelines.
- **Debugger story.** Target RenderDoc 1.27+ and PIX 2024+ which both handle descriptor indexing. Keep a debug-only mode that records per-draw "what textures did this pass sample" for capture annotation.

### 13.5 What to skip

- **Multi-GPU FG.** Ignored, per §5.
- **Subresource-granular tracking in V2.** Whole-resource is fine until a shadow atlas with per-slice writes enters the design. Adding it later is a local change inside the FG.
- **Parameter-struct reflection.** Explicit `.read()`/`.write()` calls are enough; do not build an Unreal-Header-Tool equivalent.
- **Async compute before FG exists.** Without a graph, hand-coded cross-queue semaphores are a hang generator. V2 FG first, V3 async second.
- **Bindless on GL 3.3.** `ARB_bindless_texture` exists but vendor support is inconsistent (NVIDIA yes, AMD partial, Intel no). Save bindless for the Vulkan backend where it is a clean win.

### 13.6 Single concrete first deliverable for r3 → r4

A **sort-key submission layer** (V1) that works with the existing GL 3.3 path. It is the only item that does not require the Vulkan migration, it is the only one whose value scales linearly with current pass count, and it lays the runtime data structures (opaque handles + typed submission records) that the V2 FG will reuse. Roughly one engineer-week of work; measurable draw-order and state-change improvements on day one; does not foreclose any V2/V3 decision.
