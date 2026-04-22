# Mesh Shaders, Task/Amplification Shaders & DirectX 12 Work Graphs

> ALZE Engine research — round 3, rendering SOTA 2024–2026.
> Target engine: `/root/repos/alze-engine`, C++17, OpenGL 3.3 today, Vulkan 1.3 planned for v2.
> Prior rounds touch mesh shaders only in passing (Northlight one line in `aaa_engines.md`,
> The Forge feature list in `rendering_libs.md`). This file is the deep dive.

## 0. Why this matters

The classic GPU pipeline — `IA → VS → HS → TS → DS → GS → RS → PS` — was designed in the
DirectX 10 era around fixed-function input assembly and 1 triangle in / 1 triangle out
vertex shading. On modern content (millions of triangles per frame, sub-pixel detail) it
wastes hardware in predictable ways:

- Index buffer fetch and post-transform vertex cache are bandwidth-bound.
- Geometry shaders were a compatibility dead end: serial amplification, awful on every
  architecture that shipped them.
- Tessellation (HS/DS) is tuned for a narrow set of patch-based workflows (terrain,
  water, subdivision surfaces) and adds two pipeline stages whether or not you need them.
- Back-face and tiny-triangle culling happen *after* vertex shading runs, so the VS work
  on culled triangles is pure waste.

The mesh/task pipeline and its successor, GPU work graphs, are the industry's answer: a
compute-shader-shaped programmable front end where the GPU itself dispatches geometry
work, performs per-meshlet culling before vertex transforms, and emits triangles straight
to the rasterizer.

## 1. The mesh shader pipeline

### 1.1 Stages

New pipeline (Turing/RDNA2+):

```
  Task/Amplification shader  (optional)  →  groupshared counts, payload
  Mesh shader                (required)  →  verts + prims direct to rasterizer
  Fragment shader            (required)  →  unchanged
```

Replaces `IA + VS + HS + TS + DS + GS` with two programmable stages (plus the existing
fragment stage). There is no fixed-function input assembly: a mesh shader is invoked as a
compute-style workgroup and *generates* its own vertex and index data into
implementation-defined output arrays, which the rasterizer consumes directly.

### 1.2 Task / amplification shader

Vulkan calls it "task shader", DirectX and Metal call it "amplification shader". Same
thing. Runs first, optionally. Roles:

- **Amplify or cull work before mesh dispatch.** A single task workgroup can emit 0 — N
  mesh workgroups via `EmitMeshTasksEXT` (Vulkan) / `DispatchMesh` (HLSL) / Metal's
  `mesh_grid_properties`. Output N=0 means the mesh shader is never launched.
- **Cluster-level culling.** Frustum, back-face cone, HZB occlusion, LOD selection —
  done once per cluster of ~64 vertices instead of once per vertex.
- **Pass a payload to the mesh shader.** Up to 16 KiB of groupshared data (implementation
  minimum; on many drivers the hard limit is larger). Typical payload: surviving meshlet
  indices + LOD level + per-cluster transform.

### 1.3 Mesh shader

One workgroup per meshlet. Emits two arrays:

- `SV_Position` + user varyings for up to `maxVertices` vertices (typically 64).
- A primitive index array of up to `maxPrimitives` triangles (typically 124).

Also writes per-primitive attributes (flat interpolants, cull bit, RT slice). `OpSetMeshOutputsEXT`
in SPIR-V / `SetMeshOutputCounts` in HLSL tells the rasterizer how many of each actually
survived. Triangles with a set cull bit are dropped before rasterization; this is how
fine-grained (per-triangle) culling is expressed without rewriting the index array.

### 1.4 What goes away

- No vertex / index buffer bindings. Mesh shaders fetch directly from SSBOs / structured
  buffers using compute-style indexing. This is effectively *native bindless geometry*.
- No input layout struct. No `VkVertexInputAttributeDescription`. No `IA` state.
- No primitive restart, no auto-strip expansion — you assemble primitives yourself.
- No geometry shader stage (good riddance).
- Tessellation is still available as a separate pipeline, but most engines shipping mesh
  shaders also ship subdivision/displacement through compute or through mesh shader
  amplification rather than HS/DS.

## 2. Meshlet format

### 2.1 Canonical layout

A *meshlet* is a small vertex + triangle cluster that fits in one mesh shader workgroup.
The NVIDIA Turing whitepaper and early samples use `max_vertices = 64`, `max_triangles =
124`. The 124 is not arbitrary: it aligns primitive index storage to a 128-byte boundary
after a 4-byte header. AMD RDNA2 prefers `max_vertices = 64`, `max_triangles = 64` in the
first-generation tooling (Arseny Kapoulkine's `meshoptimizer` exposes both presets).

Stored per meshlet:

```
struct Meshlet {
    uint32_t vertex_offset;   // index into global vertex index array
    uint32_t triangle_offset; // index into global packed triangle array
    uint32_t vertex_count;    // <= 64
    uint32_t triangle_count;  // <= 124
};
```

Vertex indirection: `meshlet_vertices[vertex_offset + i]` yields a 32-bit (or 16-bit,
when under 65 K verts) index into the global vertex buffer. This extra level of
indirection is the price of deduplicating shared vertices across meshlets.

Triangle storage: three 8-bit indices per triangle, each addressing the meshlet-local
vertex array (hence `vertex_count <= 255` in principle, but 64 in practice). Packed to 4
bytes per triangle with a pad byte, or to 3 bytes tightly packed — `meshoptimizer` does
the latter.

### 2.2 Per-meshlet culling metadata

The culling payload is where the speed-up lives. Sebastian Aaltonen and NVIDIA both
document the same pattern:

- **Bounding sphere** (`float4 center + radius`) for frustum and occlusion tests.
- **Normal cone** (`float3 axis + float cutoff`): a cone enclosing all triangle normals
  of the meshlet. A meshlet is back-face-cullable iff `dot(axis, view_dir) < cutoff`. This
  is a conservative test, done once per meshlet instead of per triangle. Typical cull
  rate: 40–55 % on closed meshes.
- **LOD error bound** for Nanite-style continuous LOD.

`meshoptimizer` (github.com/zeux/meshoptimizer) builds all three: `meshopt_buildMeshlets`
produces the clusters, `meshopt_computeMeshletBounds` produces sphere + cone.

### 2.3 Storage math

A 1 M-triangle mesh at 64v/124t/meshlet ⇒ roughly 8 K meshlets, ~300 KB of cluster
metadata, ~3–4 MB of vertex + index data. Per-meshlet metadata is small enough to keep
entirely in LLC during a culling pass; per-triangle metadata is not. This asymmetry is why
cluster culling scales.

## 3. API details

### 3.1 Vulkan: VK_EXT_mesh_shader (ratified 2022)

- Extension finalized at Vulkan 1.3 era, Sep 2022. Authors: Christoph Kubisch (NVIDIA),
  Timur Kristóf (Valve), plus Arm/AMD/Qualcomm input. See the Khronos blog post
  "Mesh Shading for Vulkan" (https://www.khronos.org/blog/mesh-shading-for-vulkan).
- Replaces `VK_NV_mesh_shader` (2018) with a cross-vendor spec. The NV extension is now
  legacy; new code should target `VK_EXT_mesh_shader`. The EXT differs materially:
  single-pass task→mesh payload via `OpEmitMeshTasksEXT`, SPIR-V opcodes unified, draw
  indirect count commands added, MultiDraw support.
- Features exposed via `VkPhysicalDeviceMeshShaderFeaturesEXT`:
  `taskShader`, `meshShader`, `multiviewMeshShader`, `primitiveFragmentShadingRateMeshShader`,
  `meshShaderQueries`.
- Limits in `VkPhysicalDeviceMeshShaderPropertiesEXT`:
  `maxMeshOutputVertices` (≥ 256), `maxMeshOutputPrimitives` (≥ 256),
  `maxPreferredMeshWorkGroupInvocations`, `maxTaskPayloadSize` (≥ 16384),
  `maxMeshSharedMemorySize`.
- Commands: `vkCmdDrawMeshTasksEXT(groupCountX, Y, Z)`,
  `vkCmdDrawMeshTasksIndirectEXT(buffer, offset, drawCount, stride)`,
  `vkCmdDrawMeshTasksIndirectCountEXT(buffer, offset, countBuffer, countOffset, maxDrawCount, stride)`.
- SPIR-V: the mesh entry point uses the `MeshEXT` execution model and writes to the
  `PrimitiveTriangleIndicesEXT` / `PrimitiveIndicesEXT` builtins.

### 3.2 DirectX 12: DispatchMesh

- Introduced in DX12 Ultimate (Shader Model 6.5), Windows 10 20H1 / Windows 11.
  `ID3D12GraphicsCommandList6::DispatchMesh(ThreadGroupCountX, Y, Z)`.
- HLSL entry points marked `[numthreads(x,y,z)] [outputtopology("triangle")] void MSMain(...)`.
- Amplification shader: `[numthreads(x,y,z)] void ASMain(...)` + `DispatchMesh(tx, ty, tz, payload)`.
- `ExecuteIndirect` with an `D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH` command signature
  allows GPU-driven dispatch counts — the basis for occluded-mesh culling chains.
- Root signature stays the same; there is no geometry input layout slot.

### 3.3 Metal 3

- Metal 3 (WWDC 2022, iOS 16 / macOS 13) added mesh shading via `MTLMeshRenderPipelineDescriptor`.
- Stages are called **object** and **mesh** (Metal's naming for amplification/mesh).
  Object shader dispatches a `mesh_grid_properties` struct to the mesh shader.
- Apple Silicon M3 (Oct 2023) is the first Apple GPU that runs mesh shaders in hardware
  with useful performance; on M1/M2 they are emulated / very limited.

### 3.4 Portability concerns for a cross-API wrapper

Two real differences bite when building a portable abstraction:

1. **Payload delivery.** Vulkan EXT and DX12 both use a "payload parameter" model
   (`EmitMeshTasksEXT` takes one argument; `DispatchMesh` takes a groupshared struct).
   The NV extension used a legacy per-workgroup SSBO read which is not portable. Abstract
   at the "task emits opaque POD payload" level.
2. **Limits.** `maxVertices`/`maxPrimitives` minimum-guaranteed values differ:
   - Vulkan EXT minimum: 256 verts / 256 prims.
   - DX12 SM6.5: 256 verts / 256 prims.
   - Metal 3: 256 verts / 512 prims.
   - Practical portable ceiling used by meshoptimizer presets: **64 / 124**.
3. **Indirect dispatch.** Vulkan's indirect-count variant maps 1:1 to DX12's
   `ExecuteIndirect`. Metal does not yet expose indirect mesh dispatch count in the same
   form; a CPU readback or compaction pass is needed on Apple.

Practical ALZE wrapper shape: one `DispatchMesh(x,y,z)` call, one
`DispatchMeshIndirect(argBuffer, countBuffer, maxDraws)` call, both taking a shader
module opaque handle. No attempt to portably expose per-builtin `SV_` naming; compile
separate shader variants per backend.

## 4. Hardware support baseline (2026)

| Vendor | Family | Year | Mesh shaders | Notes |
|---|---|---|---|---|
| NVIDIA | Turing (RTX 2060+, GTX 1660 variants NOT) | 2018 | Yes (NV_mesh first, EXT via driver 525+) | First HW implementation. |
| NVIDIA | Ampere / Ada / Blackwell | 2020+ | Yes | Ada doubles mesh throughput vs Turing per NVIDIA claims. |
| AMD | RDNA2 (RX 6000, PS5, Xbox Series X/S) | 2020 | Yes | Console HW path: PS5 exposes it via PSSL, XSX/XSS via D3D12. |
| AMD | RDNA3 / RDNA4 | 2022+ | Yes | Extra primitive-order pixel-shader interaction. |
| Intel | Xe-HPG (Arc A-series) | 2022 | Yes | First Intel HW mesh path. |
| Intel | Xe2 (Battlemage, Arc B) | 2024 | Yes | Parity feature set, significantly better perf. |
| Apple | M3 / M4 | 2023+ | Yes (Metal 3) | M1/M2 practically unusable for mesh. |
| Qualcomm | Adreno 7x0 + | ~2023 | Partial (Vulkan EXT on newer drivers) | Mobile path still immature. |
| Nintendo Switch 1 (Tegra X1) | Maxwell | 2017 | **No** | Below the baseline. |
| Nintendo Switch 2 (Tegra T239, Ampere) | Ampere | 2024 | Yes | Confirmed at HW level; whether first-party titles use it is another question. |

**2026 baseline reality:** anything shipping since ~2020 on PC/console supports mesh
shaders. The long tail is pre-Turing GeForce (GTX 10-series), pre-RDNA2 Radeon
(RX 5000 and older), Intel iGPUs pre-Xe, Apple M1/M2, and Switch 1. On Steam hardware
survey snapshots 2025-Q4, roughly 70–75 % of active GPUs support mesh shaders; on
consoles (excluding Switch 1) it is 100 %.

## 5. Performance claims

### 5.1 NVIDIA

Christoph Kubisch's 2018 "Introducing Turing Mesh Shaders" blog post
(https://developer.nvidia.com/blog/introducing-turing-mesh-shaders/) gives the first
public numbers. Highlights:

- On a 2–6 M triangle asteroid scene, mesh-shader pipeline runs 3–5× faster than the
  classic VS pipeline at frustum-cull-heavy views because the back-face and frustum cone
  test happens before any vertex transform.
- On worst-case (no cullable geometry, all on-screen), mesh shaders match classic VS
  throughput; they do not win and they do not lose.
- Post-transform cache is not relevant anymore — the mesh shader produces its own local
  vertex array in workgroup memory.

Kubisch's GTC 2019 talk "Mesh Shaders in Turing" (archived at NVIDIA Developer On-Demand)
extends with practical numbers: ~2.5× win on a forest scene, ~4× on a destructible debris
pile, ~1.1× on a flat "ground plane" workload.

### 5.2 AMD / RDNA2

- "Mesh Shaders on AMD RDNA Graphics Cards" (AMD GPUOpen blog, 2022,
  https://gpuopen.com/learn/mesh_shaders/mesh_shaders-from_vertex_shader_to_mesh_shader/)
  — a four-part series authored by Max Oakley and Stephan Hodes. Core claims:
  - RDNA2 mesh shaders execute on the same Primitive Assembler hardware as Primitive
    Shaders (the internal "NGG" path). Performance ceilings are therefore bound by
    the same front-end triangle rate.
  - 1–1.5× speedup on naive ports; 2–3× once per-meshlet culling is turned on; 4×+ when
    combined with GPU-driven occlusion culling against HZB.
  - RDNA2 preferred meshlet size: 64 v / 64 t. RDNA3 relaxed this to 64 v / 128 t.
- "Geometry processing on the AMD RDNA architecture" (GDC 2022) by AMD's Hans-Kristian
  Arntzen (since at Valve) covers the underlying NGG pipeline that mesh and primitive
  shaders compile down to.

### 5.3 Intel Arc and the emulation window

Intel published "Mesh Shading for Vulkan" application notes on their GPU portal (2023).
Key observation: on Xe-HPG the mesh pipeline hits within 10 % of the classic pipeline on
well-tuned content but is 2–3× faster on asset-dense scenes. Intel's Xe2 architecture
reduces the gap further. They also emulate the EXT extension via compute + indirect draw
on iGPUs that do not support it natively, which is useful for portability testing.

### 5.4 The flagship shipping case: Alan Wake 2 / Northlight

Remedy's Alan Wake 2 (Oct 2023) is **the** headline mesh-shader-only title. Digital
Foundry interviewed Tatu Aalto (lead rendering programmer) and Ville Ruusutie (engine
director) around launch; the relevant public material:

- **Mesh shader is a hard requirement.** Alan Wake 2 has no fallback path. On PC this
  excluded GTX 10-series, RX 5000-series, and older. Remedy explained this publicly as a
  tradeoff: maintaining two geometry pipelines was not feasible for their ~150-person
  engine/tech team.
- **Everything is an occluder.** Because the mesh shader pipeline is cheap to dispatch
  against the HZB, Remedy rebuilds the HZB each frame from the prior frame's depth plus
  a fast reprojection pass, then culls meshlets against it. Compare to classic engines
  that handpick occluders (a dozen "occluder proxy" meshes); in Northlight every
  meshlet, tree leaf, and character limb participates in HZB occlusion.
- **Sub-pixel culling.** Meshlets whose projected bounding sphere falls under ~1 px are
  skipped entirely — mesh shader dispatch is gated on screen-space area.
- **Meshlet count as a budget axis.** Art director and engineers agree a meshlet budget
  per frame (hundreds of thousands in 4K), not a draw-call budget. Content can exceed
  geometric density without punishing the CPU.

References:
- Remedy Entertainment, "Northlight and Alan Wake 2: A Technical Deep-Dive,"
  https://www.remedygames.com/article/how-northlight-makes-alan-wake-2-shine
- Digital Foundry, "Alan Wake 2 PC tech review," Nov 2023 (youtube.com/DigitalFoundry).
- Tatu Aalto, Ville Ruusutie, "The Rendering of Alan Wake 2," presented at REAC 2024
  and excerpted in advances.realtimerendering.com 2024 (slides archived at
  https://advances.realtimerendering.com/).

### 5.5 id Tech 8 (Doom: The Dark Ages, Indiana Jones and the Great Circle)

Less public detail than Alan Wake 2, more second-hand speculation. What is public:

- Tiago Sousa's SIGGRAPH 2025 talk "Fast as Hell: idTech 8 Global Illumination"
  (https://advances.realtimerendering.com/s2025/content/SOUSA_SIGGRAPH_2025_Final.pdf)
  mentions mesh shaders in passing as the geometry primitive; it focuses on GI.
- Billy Khan's GameDeveloper interview (2024) on Indiana Jones (MachineGames + id Tech 8)
  confirms mesh-shader-only geometry on PC ultra. Hardware requirement on the Great
  Circle: Turing+/RDNA2+ for ray tracing; mesh shaders are implicit.
- Doom: The Dark Ages (May 2025) has a hard Turing/RDNA2 minimum; public PR confirms
  mesh shaders are the primary geometry path with a fallback compute-rasterizer for
  extremely dense foliage (similar to Nanite's software path, but narrower in scope).

## 6. DirectX 12 Work Graphs

### 6.1 Concept

Announced publicly at Microsoft Ignite 2023, previewed GDC 2024, shipped as Work Graphs
1.0 in the Agility SDK 1.613 (2024) and in DirectX 12 Ultimate. The short pitch:
**producer/consumer GPU dispatch without CPU involvement.**

A work graph is a DAG of *nodes*. Each node is:

- a compute shader (or, as of 2025 preview, a mesh shader node),
- with declared input record type(s),
- with declared output record type(s) to named downstream nodes.

At dispatch time, the GPU itself schedules nodes: when a producer writes N output
records into a downstream node's input queue, the scheduler launches consumer workgroups
to drain that queue. The application submits a single `DispatchGraph` command with an
*entry-point* record list; the GPU runs the whole graph to completion.

### 6.2 Why this exists

Classic GPU-driven rendering is a chain of `ExecuteIndirect` / `DispatchIndirect` calls
hand-wired together by the CPU, with UAV barriers between every stage. Each stage needs
enough work to amortize the barrier cost, so engines batch aggressively. Work Graphs
remove the CPU-side plumbing and remove the barrier per stage — the scheduler knows the
dependency structure and coalesces launches.

Concretely:

- A visibility classifier produces "this meshlet needs shading path A / B / C" records.
- Three downstream nodes consume A/B/C records respectively.
- The scheduler launches A/B/C nodes as records accumulate, not after a CPU round-trip.

Recursion is allowed (bounded by a max-depth annotation). Coalesced-dispatch nodes can
pack sparse records into dense workgroups — the scheduler batches inputs before launching,
which is the entire point of the feature on heterogeneous GPUs with high launch cost.

### 6.3 Primary references

- Amar Patel, Tex Riddell, "Advanced API and shader features: Work Graphs,"
  DirectX Developer Blog, Jun 2023,
  https://devblogs.microsoft.com/directx/d3d12-work-graphs-preview/
- Amar Patel, "GPU Work Graphs: The Path Forward for GPU-Driven Rendering," GDC 2024,
  https://www.gdcvault.com/play/1034808/GPU-Work-Graphs
- Microsoft DirectX-Specs, "Work Graphs" spec document,
  https://microsoft.github.io/DirectX-Specs/d3d/WorkGraphs.html
- AMD Capsaicin / GPUOpen "Work Graphs Mesh Nodes" preview, 2024,
  https://gpuopen.com/learn/work_graphs_mesh_nodes_introduction/

### 6.4 Hardware and driver status (2025/2026)

- NVIDIA: Turing+ via Agility SDK. Driver 551+ exposes Work Graphs 1.0.
- AMD: RDNA3+ runs Work Graphs natively; RDNA2 supports a subset via fallback.
- Intel: Arc Alchemist+ supports Work Graphs via Agility SDK + recent drivers.
- Console: PS5 has no Work Graphs API (PSSL-specific equivalents exist but differ).
  Xbox Series X/S inherits the DX12 path.

### 6.5 Vulkan equivalent (none official, as of 2026-Q1)

No ratified Vulkan extension at knowledge cutoff. Khronos has publicly indicated
"device-generated commands" (`VK_EXT_device_generated_commands`, `VK_NV_device_generated_commands_compute`)
as partial coverage: GPU can write command buffers that launch indirect dispatches, but
there is no scheduler-level producer/consumer model. The AMD internal GPU-work-graph
implementation (used for Work Graphs Mesh Nodes on RDNA3) has been proposed as a
possible basis for a cross-vendor Vulkan extension; no timeline public.

### 6.6 Work Graphs Mesh Nodes

Announced GDC 2024 as an extension of Work Graphs: a graph node can be a *mesh shader*
rather than a compute shader. The implication is that the classic "cull → compact →
draw" chain collapses into a single Work Graph dispatch, where mesh nodes emit geometry
directly from scheduled records. This is the first API that unifies GPU-driven culling
and geometry emission in one programmable fabric.

Nobody has shipped a game using Work Graphs Mesh Nodes as of knowledge cutoff; tech
demos from AMD (Capsaicin), NVIDIA (Falcor sample), and the Microsoft samples repo are
the only public uses.

## 7. Comparison tables

### 7.1 Pipeline comparison

| Axis | Classic VS(+GS) pipeline | Mesh shader pipeline | Work Graphs |
|---|---|---|---|
| Front-end | Fixed-function IA + VS | Task + mesh shaders | Compute/mesh graph nodes |
| Culling granularity | Per draw, then per triangle post-VS | Per meshlet (pre-VS), per triangle (cull bit) | Per node + per meshlet |
| Scalability with scene density | CPU-bound on draw count | Scales with meshlet count, GPU-bound | Scales with record volume, GPU-scheduled |
| Hardware baseline | DX11 class, GL 3.3 class — anything since 2010 | Turing / RDNA2 / Xe-HPG / M3 / Adreno 7xx | DX12 Ultimate, Agility SDK 1.613+, Turing+ / RDNA2+ / Arc A+ |
| API surface | Vertex + index buffers, input layout, draw calls | Task + mesh shader, DispatchMesh or EmitMeshTasksEXT | DispatchGraph, node metadata, records |
| Shader complexity | Lowest — separate VS per material | Medium — mesh shader does indexing + culling | Highest — producer/consumer model, manual record design |
| GPU-driven indirect | Via ExecuteIndirect / MDI | Yes, via DrawMeshTasksIndirect | Native, scheduler-level |
| Console portability | Universal | PS5, XSX/XSS via D3D12/PSSL | XSX/XSS only; no PS5 equivalent shipped |
| Mobile portability | Universal | Partial — Adreno 7xx+ only | None |
| Web (WebGPU) | Yes | Not in core spec; extension proposals exist | No |
| LOD mechanism | Discrete switches per mesh | Per-meshlet continuous LOD feasible (Nanite does it) | Same, with scheduled LOD selection nodes |
| Debugging | Mature (PIX, RenderDoc, NSight) | Supported (RenderDoc 1.25+, PIX 2310+) | Supported but sparse |

### 7.2 Meshlet vs. draw-call throughput, back-of-envelope

| Scene | Classic (draw calls / frame) | Mesh shader (meshlets / frame) | Speedup after tuning |
|---|---|---|---|
| Corridor shooter (Doom E.) | 5–20 K draws | 50–200 K meshlets | 1.5–2× |
| Dense foliage (Horizon) | 20–80 K draws | 200 K–1 M meshlets | 3–4× |
| Cinematic static asset (Alan Wake 2 hotel lobby) | 2–5 K draws | 500 K meshlets | 3–5× |
| UI / text / 2D | ~100 draws | 100 meshlets | ~1× |

Numbers are illustrative, drawn from NVIDIA blog + AMD GPUOpen + Digital Foundry Alan
Wake 2 coverage — exact per-title numbers are not public.

## 8. ALZE applicability

### 8.1 Hard constraint: GL 3.3 is impossible

- Core GL 4.6 does **not** expose mesh shaders. `GL_NV_mesh_shader` exists (NVIDIA only,
  2018) but never went cross-vendor in GL. There is no OpenGL path.
- Consequence: mesh shader support is a Vulkan-or-nothing feature. It is
  architecturally impossible to bolt onto ALZE v1's GL 3.3 backend.
- Workaround for ALZE v1: GPU-driven indirect draw of *classic* VS (SSBO + compute
  culling + `glMultiDrawElementsIndirectCount` which *is* in GL 4.6 core). This delivers
  most of the CPU-side win without the mesh shader API. It is the right move for v1.

### 8.2 Minimum viable for ALZE v2 (Vulkan 1.3)

- Vulkan 1.3 + `VK_EXT_mesh_shader` + `VK_KHR_draw_indirect_count` as hard requirements.
- Meshlet tooling: `meshoptimizer` (BSD, ~5 KLOC header+sources, zero dependencies, used
  by Godot/Bevy/The Forge/Khronos glTF meshopt extension) is the right and only serious
  choice. `meshopt_buildMeshlets`, `meshopt_computeMeshletBounds`, done.
- Minimum target hardware: NVIDIA Turing, AMD RDNA2, Intel Xe-HPG. Pragmatically that
  excludes pre-2020 GPUs — consistent with Vulkan 1.3 anyway (Vulkan 1.3 drivers are not
  available for Kepler/Maxwell).
- Engineering cost estimate: ~1500–2500 LOC wrapper code + 2–3 kLOC of shaders. Plus
  ~1 kLOC of meshlet-build offline tool integration. Plus the frame graph work to let
  meshlet culling share HZB with other passes. Call it 2–4 engineer-months.
- Payoff: meshlet-level culling against HZB yields 2–3× throughput on asset-dense scenes
  vs classic-pipeline GPU-driven indirect draw. For a small-team engine this is a *big*
  lever, bigger than most renderer refactors of similar cost.
- Risk: two shader variants (classic + mesh) during transition, increased PSO count,
  driver bugs on the EXT path (still maturing as of 2026). Mitigation: ship mesh-shader
  pipeline as opt-in behind a feature flag for the first six months.

### 8.3 Work Graphs = aspirational v3

- Requires DX12 Ultimate — therefore a D3D12 backend. ALZE has none planned.
- Requires reshaping the renderer around producer/consumer nodes, not around frame-graph
  passes. That is a second architectural rewrite, not an extension of the mesh-shader
  path.
- No Vulkan equivalent exists, so there is no cross-vendor path for an engine that
  targets PC + consoles without a D3D12 backend.
- Verdict: interesting to watch, not a v2 or v3 commitment for a small-team engine.

### 8.4 Version-by-version table

| Version | API baseline | Mesh shaders | Meshlet tooling | Work Graphs |
|---|---|---|---|---|
| v1 (current) | GL 3.3 core | Impossible (no GL extension cross-vendor) | Out of scope | Impossible |
| v2 (planned) | Vulkan 1.3 | `VK_EXT_mesh_shader` opt-in behind feature flag; classic indirect-MDI path as default; mesh-shader path for dense-geometry scenes | `meshoptimizer` offline at asset-bake, runtime dispatches per-mesh metadata | Not targeted |
| v3 (aspirational) | Vulkan 1.4/1.5 + optional DX12 Ultimate backend | Mesh shaders are default; per-meshlet Nanite-style continuous LOD; HZB-driven culling | meshopt + custom two-pass clusterizer for continuous LOD | DX12 path only; revisit after Khronos ships a ratified equivalent; avoid splitting codepaths across Vulkan/DX12 until there is parity |

### 8.5 Is it worth the complexity vs. classic VS + GPU-driven indirect draw?

Short answer: **not for v1, probably for v2, yes for anything that calls itself AAA-scale by v3.**

Long answer. Mesh shaders win when two conditions hold simultaneously:

1. The scene has >500 K triangles worth of meshlets per frame after coarse culling.
2. HZB-based occlusion is already implemented and paying for itself.

For a scene that is mostly 5–20 K draw calls of hand-placed assets, classic-pipeline
GPU-driven indirect draw captures most of the benefit with a quarter of the engineering
cost and no hardware exclusion. That is the right v1 and v2-launch posture. Turning on
the mesh path is a *post-shipping* optimization gated on telemetry ("which scenes are
front-end bound?").

The small-team honesty test: every hour spent on a mesh-shader path is an hour not spent
on a better editor, a better animation system, or better tooling. A ~30 K-LOC engine
cannot ship three competitive renderer paths. Pick one default (GPU-driven indirect MDI
in v1, keep it in v2) and let the mesh path be an opt-in upgrade, not a parallel codebase.

## 9. References

Primary sources (author · year · venue · URL):

- Christoph Kubisch (NVIDIA) · 2018 · NVIDIA Developer Blog ·
  "Introducing Turing Mesh Shaders" ·
  https://developer.nvidia.com/blog/introducing-turing-mesh-shaders/
- Christoph Kubisch · 2019 · GTC 2019 · "Mesh Shaders in Turing" ·
  https://developer.nvidia.com/gtc/2019/video/S9833 (archive:
  https://web.archive.org/web/2024*/developer.nvidia.com/gtc/2019/video/S9833)
- Martin Fuller, Claire Andrews, Shawn Hargreaves (Microsoft) · 2020 · DirectX Dev Blog ·
  "Coming to DirectX 12: Mesh Shaders and Amplification Shaders" ·
  https://devblogs.microsoft.com/directx/coming-to-directx-12-mesh-shaders-and-amplification-shaders-reinventing-the-geometry-pipeline/
- Max Oakley, Stephan Hodes (AMD) · 2022 · AMD GPUOpen · "Mesh Shaders on AMD RDNA
  Graphics Cards" (4-part series) ·
  https://gpuopen.com/learn/mesh_shaders/mesh_shaders-from_vertex_shader_to_mesh_shader/
- Hans-Kristian Arntzen · 2022 · GDC 2022 · "Geometry Processing on the AMD RDNA
  Architecture" ·
  https://gpuopen.com/gdc-presentations/2022/Geometry_Processing_on_RDNA.pdf
- Khronos Group · 2022 · Vulkan 1.3 registry · `VK_EXT_mesh_shader` spec ·
  https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_mesh_shader.html
- Christoph Kubisch, Timur Kristóf, Daniel Koch et al. · 2022 · Khronos blog ·
  "Mesh Shading for Vulkan" ·
  https://www.khronos.org/blog/mesh-shading-for-vulkan
- Apple · 2022 · WWDC 2022 · "Transform your geometry with Metal mesh shaders" ·
  https://developer.apple.com/videos/play/wwdc2022/10162/
- Arseny Kapoulkine · ongoing · GitHub · `meshoptimizer` (meshlet build, LOD, culling
  metadata) · https://github.com/zeux/meshoptimizer
- Amar Patel, Tex Riddell (Microsoft) · 2023 · DirectX Developer Blog · "D3D12 Work
  Graphs Preview" ·
  https://devblogs.microsoft.com/directx/d3d12-work-graphs-preview/
- Amar Patel · 2024 · GDC 2024 · "GPU Work Graphs: The Path Forward for GPU-Driven
  Rendering" · https://www.gdcvault.com/play/1034808/GPU-Work-Graphs
- Microsoft · 2024 · DirectX-Specs · "Work Graphs" specification ·
  https://microsoft.github.io/DirectX-Specs/d3d/WorkGraphs.html
- Matthäus Chajdas, Max Oakley (AMD) · 2024 · GPUOpen · "Work Graphs Mesh Nodes
  Introduction" · https://gpuopen.com/learn/work_graphs_mesh_nodes_introduction/
- Tatu Aalto, Ville Ruusutie (Remedy) · 2024 · REAC 2024 / advances.realtimerendering.com
  · "The Rendering of Alan Wake 2" (excerpts) ·
  https://advances.realtimerendering.com/
- Remedy Entertainment · 2023 · corporate blog · "How Northlight Makes Alan Wake 2
  Shine" · https://www.remedygames.com/article/how-northlight-makes-alan-wake-2-shine
- Digital Foundry / Alex Battaglia · 2023 · YouTube · "Alan Wake 2 — PC Tech Review: A
  Modern Rendering Showcase" · https://www.youtube.com/@DigitalFoundry
- Tiago Sousa (id Software) · 2025 · SIGGRAPH 2025 · "Fast as Hell: idTech 8 Global
  Illumination" ·
  https://advances.realtimerendering.com/s2025/content/SOUSA_SIGGRAPH_2025_Final.pdf
- Sebastian Aaltonen · ongoing · personal Twitter/X archive · GPU-driven rendering
  threads on meshlets, visibility buffers, HZB · https://twitter.com/SebAaltonen (live),
  https://web.archive.org/web/2024*/twitter.com/SebAaltonen (archive fallback)
- Graham Wihlidal (Frostbite / Microsoft) · 2022 · "GPU-driven Rendering — Mesh Shaders,
  Meshlets, and More" · GDC 2022 ·
  https://www.gdcvault.com/play/1028154/Rendering-the-Hellscape (adjacent talk in same
  track) — referenced alongside id Tech coverage.

## 10. TL;DR for ALZE

- Mesh shaders are the 2026 default geometry front-end on anything post-2020. Work
  Graphs are the 2026+ future of GPU-driven dispatch on D3D12 but are not portable.
- v1 ALZE on GL 3.3: can't have mesh shaders. Don't pretend otherwise. The right v1
  play is classic VS + compute culling + `glMultiDrawElementsIndirectCount`.
- v2 ALZE on Vulkan 1.3: add `VK_EXT_mesh_shader` as an opt-in path, integrate
  `meshoptimizer` into the asset baker, keep the classic path as default. Expected
  budget: 2–4 engineer-months for a ~2–3× front-end throughput win on dense content.
- v3 ALZE: consider mesh shaders as the default geometry front-end. Watch Work Graphs
  but don't commit until there is a Vulkan equivalent — splitting the engine across
  Vulkan and D3D12 code paths is a trap a small team cannot afford.
- The hardest engineering question is not "should we adopt mesh shaders?" — the answer
  is yes for v2+. It is "can we budget the discipline to keep two geometry pipelines
  (classic + mesh) working correctly during the transition window?" For a small team,
  **the honest answer is short transition window + feature flag + delete the classic
  path as soon as HW telemetry justifies it.**
