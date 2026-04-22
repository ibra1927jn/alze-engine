# Rendering-Focused Libraries — Research Notes for ALZE Engine

Research target: libraries ALZE would integrate *into* the engine, not full engines.
ALZE baseline: C++17, no-RTTI, no-exceptions, OpenGL 3.3 + GLAD, planning to evolve toward Vulkan/D3D12-class APIs with a PBR pipeline.

## Overview

Two overlapping categories live under "rendering library":

- **RHI (Render Hardware Interface)** — a thin abstraction over OpenGL / Vulkan / D3D11/12 / Metal / WebGPU. You call `createBuffer`, `bindPipeline`, `draw`; the RHI handles the backend. Examples: bgfx, sokol_gfx, Diligent, wgpu, The Forge's renderer module.
- **Rendering library (PBR / frame)** — pipeline concepts on top of an RHI: material system, IBL prefiltering, TAA, bloom, render graph. Examples: Filament, Falcor.
- **Both** — The Forge and Filament reach up into full frame; bgfx and wgpu stay strictly at the RHI level.

Why this category exists: writing the same renderer against GL, Vulkan, D3D12, and Metal is a multi-year effort. Everyone who has done it once wraps it, so you don't have to. ALZE currently speaks only GL 3.3 directly — a deliberate design decision that can be upgraded by introducing an RHI seam.

## bgfx

Branimir Karadzic's BSD-2 "Bring Your Own Engine" rendering library. The canonical C++ RHI reference.

- **Backends (~10):** D3D11, D3D12, Metal, GL 2.1, GL 3.1+, GLES 2/3/3.1, Vulkan, WebGL 1/2, WebGPU, plus console SDKs for accredited devs.
- **Opaque handles:** every GPU resource is a 16-bit index inside a typed struct (`bgfx::TextureHandle`, `VertexBufferHandle`). Zero backend types leak into user code.
- **Submit API:** stateless-ish. Each `submit()` finalizes one draw call keyed by a 64-bit sort key; the render thread `radix-sort`s them so submission order is decoupled from execution order. Perfect for gathering draws from many systems without caring about state changes.
- **Threading model:** single-threaded frontend (API thread records into submit buffer) + a dedicated render thread that drains a second buffer. `bgfx::frame()` swaps the two and signals the backend. Multi-threaded encoders exist for parallel command recording.
- **Shader pipeline:** `shaderc` (not Google's — bgfx's own) takes a GLSL-like "bgfx shading language" source, compiles to every backend's binary form (DXBC/DXIL, SPIR-V, MSL, GLSL variants), wraps each with a tiny header identifying the target, and the runtime picks the right blob. One source, N blobs, shipped side-by-side.

## Filament

Google's open-source (Apache 2.0) PBR reference implementation. Not an RHI — it's a full rendering library.

- **BRDF:** Disney/Burley for diffuse, GGX (Trowbridge-Reitz) normal distribution for specular, Smith-GGX correlated visibility. The `docs/Filament.md` (aka `Filament.md.html`) document is the industry's most cited write-up of these formulas and their justifications.
- **IBL:** offline prefiltering via the `cmgen` tool produces a prefiltered specular cubemap (one mip per roughness) + spherical harmonics for diffuse irradiance; runtime uses the split-sum approximation (Karis 2013).
- **Material system:** material definition files (Filament DSL) compiled by the `matc` tool into "material packages" that bundle shaders for every target API (GL, Vulkan, Metal). `matc` expands a source material into N **shader variants** along axes like `directionalLighting`, `dynamicLighting`, `shadowReceiver`, `skin`; `--variant-filter` lets you kill variants you know you won't need.
- **Post:** bloom, TAA, color grading, multiple tone mappers (ACES, Uncharted 2, etc.), all production-grade.
- **Targets:** OpenGL ES 3.0 minimum (mobile-first design — huge win for mid-range phones), Vulkan/Metal for desktop/high-end mobile, WebGL2.

## Falcor (NVIDIA)

NVIDIA's real-time rendering research framework. D3D12 + Vulkan, Windows-first.

- **Slang-native:** uses the Slang shader compiler as the first-class front end (HLSL 2018+ with generics/interfaces). Most NV research papers that ship code ship `.slang` shaders here.
- **Render graph:** DAG of passes (e.g. `GBufferPass` → `ShadowMapPass` → `DeferredLighting` → `TAA`). Edges declare resource dependencies; the engine allocates transient resources and inserts barriers. Has a GUI graph editor (`Mogwai`).
- **Path tracer reference:** a real path tracer + DXR hardware ray-tracing samples, plus hybrid real-time/offline experiments — it's the scaffold used by DLSS, ReSTIR, SVGF, and other published SIGGRAPH/HPG papers.
- **Stability:** frequent API breaks between releases; upstream is a moving research target, not a stable SDK.

## Magnum (Corrade base)

Vladimír Vondruš's lightweight C++11/14 graphics middleware. Explicitly *not* an engine.

- **Modular:** core + `GL`, `Vk`, `Audio`, `Text`, plugins for image/model importers, integrations (ImGui, Bullet, Eigen).
- **Corrade:** the utility substrate under Magnum — containers, plugin manager, test runner, `Utility::Debug`. Replaces STL in many places with allocator-aware, header-light equivalents.
- **Platform abstraction:** one `Platform::Application` base with specializations over SDL2, GLFW, GLX, EGL, EmScripten, Android, iOS. Write one `main`, ship everywhere.
- **Pedagogical:** the API is arguably the cleanest C++ wrapper over OpenGL/Vulkan ever shipped — type-safe `Magnum::GL::Mesh`, strong-typed uniform blocks, RAII. Worth reading even if you don't link it.

## Diligent Engine

Cross-API low-level graphics library by Diligent Graphics. Apache 2.0.

- **Backends:** D3D11, D3D12, Vulkan, OpenGL/GLES, Metal.
- **HLSL everywhere:** HLSL is the common shader source on every backend; Vulkan build links `glslang` to compile HLSL → SPIR-V at runtime, other backends translate similarly.
- **Shader reflection:** automatic resource binding via reflection — descriptor tables on D3D12, descriptor sets on Vulkan are generated from shader metadata. Less boilerplate than raw Vulkan.
- **Vibe:** enterprise / desktop / "correct" C++ with verbose COM-style ref-counted objects (`RefCntAutoPtr<IPipelineState>`). Fine if you like that; heavy if you don't.

## wgpu

Rust implementation of the W3C WebGPU standard by gfx-rs. Runs native (Vulkan, Metal, D3D12, GLES) and in-browser (WebGPU, WebGL2 fallback).

- **Lingua franca:** WebGPU is becoming the portable GPU API — Firefox, Chrome, Servo, Deno ship wgpu as their WebGPU implementation. Bevy, Fyrox, rend3, Macroquad, ggez all use it as their renderer.
- **API shape:** explicit like Vulkan/Metal (command encoders, bind groups, pipeline state objects) but with web-safe validation and a much friendlier vibe than raw Vulkan.
- **Shader language:** WGSL natively; SPIR-V and GLSL accepted through translation.
- **C bindings:** `wgpu-native` exposes the same API to C/C++ engines — viable as an RHI for non-Rust projects.

## Sokol

Andre Weissflog's single-header C89 3D-API wrapper (`sokol_gfx.h`). Part of the broader `sokol_*.h` family (audio, app, time, fetch).

- **Backends:** GL 3.3, GLES2, GLES3/WebGL2, D3D11, Metal (macOS/iOS), WebGPU.
- **API model:** borrows from Metal and D3D11 — pipeline state objects, immutable resource handles, explicit render passes; GLES2 is the floor so it runs *anywhere*.
- **State tracking:** implicit — you bind a pipeline + resource bindings + pass, draw. No command buffers visible.
- **Minimalism:** one `.h` file, no dependencies, ~10k LOC total. Compiles in under a second. C89 API consumable by C++, Zig, Odin, Rust (via wrappers).
- **Docs:** Floh's `floooh.github.io` tour is the canonical write-up and reads like a tutorial on how to *design* an RHI.

## The Forge (ConfettiFX)

AAA-grade cross-platform rendering framework. Apache 2.0.

- **Backends:** D3D12, Vulkan, Metal; accredited developers get Xbox One/Series, PS4/5, Switch. Also Quest 2, Android Vulkan + GLES fallback.
- **Production use:** ConfettiFX staff have shipped AAA (Tomb Raider, Battlefield, Mafia 3, Hades, Starfield, Call of Duty Warzone) — the framework reflects that bias. You get a lot (VR, ray tracing, mesh shaders, visibility buffer, GPU-driven pipeline samples).
- **C99 core:** deliberate move toward C99 to keep the codebase small-team-friendly.
- **Complexity:** the most capable option here and the steepest setup. The rendering samples are a mini-graphics-PhD.

## Shader language trends

- **HLSL-as-source + cross-compile.** Write HLSL, compile via **dxc** to DXIL for D3D12 and to SPIR-V for Vulkan (dxc has a first-class SPIR-V backend). Use **SPIRV-Cross** to transpile SPIR-V → GLSL 3.30/4.x for OpenGL, MSL for Metal, ESSL for GLES/WebGL. This is the de-facto toolchain for cross-API engines in 2026 and what Diligent/The Forge largely use.
- **Slang** (Microsoft + Khronos stewardship) — HLSL-compatible superset with real modules, generics with interface constraints (like C++ concepts, but pre-checked), and targets D3D12/Vulkan/Metal/D3D11/CUDA/CPU. Used by Falcor, NVIDIA research, and increasingly by engines wanting generic shader libraries without `#ifdef` hell. Microsoft announced DirectX will adopt SPIR-V as interchange, which pulls Slang further into the mainstream.
- **WGSL** — WebGPU's official shader language. Safer but more verbose; currently the browser target. Most native engines prefer HLSL→SPIR-V→WGSL translation over authoring WGSL directly.
- **GLSL** — legacy for OpenGL/GLES. Still what ALZE ships today. Fine as a target format, painful as the only authoring format.

## En qué es bueno cada uno

- **bgfx** — ship today on every platform that exists. Sort-key pattern is dead simple; docs are terse but the code is readable.
- **Filament** — the PBR math + IBL prefilter pipeline is the industry-reference implementation. Mobile performance is unmatched.
- **Falcor** — reproducing published graphics research is trivial here; Slang integration is first-class.
- **Magnum** — cleanest C++ API design in the space. Excellent Emscripten story. Teaching-quality code.
- **Diligent** — reflection-driven binding saves boilerplate on D3D12/Vulkan; permissive license.
- **wgpu** — future-proof: one API that runs native *and* web. C bindings available. Explicit but not Vulkan-painful.
- **Sokol** — smallest possible footprint RHI. Perfect to copy-paste-study-steal-from.
- **The Forge** — if you eventually need consoles + RT + mesh shaders + visibility buffers, it's the only open option.

## En qué falla cada uno

- **bgfx** — ships its own (non-HLSL) shader dialect; no built-in render graph (sort keys only); docs are thin beyond the headers.
- **Filament** — monolithic; using just its material system without its scene graph is hard; heavy on C++ RTTI/exceptions in places that would clash with ALZE's `-fno-rtti -fno-exceptions`.
- **Falcor** — API churn, D3D12/Vulkan only, Windows-centric build, research-grade not production.
- **Magnum** — heavy template metaprogramming; opinionated on build system (CMake + plugins); learning curve for idioms.
- **Diligent** — COM-style verbosity; feels "enterprise"; less cutting-edge than Falcor or The Forge.
- **wgpu** — Rust-first (C API is second-class); WebGPU's feature set lags native (no bindless yet, limited RT).
- **Sokol** — doesn't scale up: no bindless, no compute on all backends, no D3D12/Vulkan backends at all, no render graph.
- **The Forge** — massive to ingest; opinionated structure; console code is NDA-gated; samples assume you're fluent in modern GPU architecture.

## Qué podríamos copiar para ALZE Engine

1. **bgfx-style opaque handle RHI seam.** Introduce `alze::rhi::TextureHandle`, `BufferHandle`, `PipelineHandle` as `struct{uint16_t idx;}` types. Wrap current GL 3.3 behind `rhi_gl33.cpp`; later `rhi_vulkan.cpp` drops in without touching a line of game code.
2. **Sort-key submit pattern.** Gather draws from all systems with a 64-bit key `[viewId | pass | blend | depth | material]`. Radix-sort once. State changes amortize across the whole frame. Simpler than a render graph for an indie-scale engine.
3. **Filament-style material DSL → N shader variants.** A small offline tool (`alzec`) that reads a `.mat` file (params + a fragment) and emits permutations keyed by feature flags (lit/unlit, shadows, skinning, normal-map). Ship the blob; runtime picks by variant mask.
4. **HLSL → SPIR-V → GLSL toolchain** via `dxc + SPIRV-Cross`. Author shaders *once* in HLSL, generate GLSL 3.30 today for the GL 3.3 backend, keep SPIR-V around for a future Vulkan backend. This is the cheapest forward-compatibility move available.
5. **Render graph (Frostbite FrameGraph style).** When the engine grows past ~10 passes, introduce a DAG: passes declare `read(resource)`/`write(resource)`; the graph culls dead passes, computes resource lifetimes, and aliases transient targets. Yuriy O'Donnell's 2017 GDC talk is the reference; many small open implementations exist (skaarj1989/FrameGraph, acdemiralp/fg).
6. **Filament GGX + prefiltered IBL.** Port the split-sum approximation (Karis 2013) + offline prefilter. Copy Filament's BRDF formulas verbatim — they're public, well-tested, and cross-referenced in every modern PBR paper.
7. **KTX2 + Basis Universal textures.** Single asset file, transcoded at load time to BCn on desktop / ETC/ASTC on mobile / DXT on web. Dramatic win on disk size, GPU memory, and portability; future-proof across backends.
8. **Sokol-style minimalist state tracking.** Even behind an opaque-handle RHI, keep the mental model of "pipeline + bindings + pass → draw". Don't expose command buffers until ALZE actually needs multi-threaded recording.

## Qué NO copiar

- **Magnum's template metaprogramming.** Type-safe wrappers over GL are gorgeous but demand C++17 template fluency and clash with `-fno-rtti -fno-exceptions`. Admire the API shape; don't copy the templates.
- **Diligent's COM-style ref-counted `IObject` hierarchy.** Verbose, and `RefCntAutoPtr<>` implies RTTI-ish behavior.
- **Falcor's research-first API instability.** Don't model ALZE's public API on a moving target.
- **The Forge's complete module suite.** Copying all of it wholesale = you adopt their build system and their assumptions. Steal specific algorithms (GPU-driven pipeline, visibility buffer) later, piecemeal.
- **bgfx's custom shader dialect.** Author in HLSL instead — broader ecosystem, more tooling, one source for all targets.
- **WGSL as authoring language.** WebGPU-native, but the native-first engine workflow favors HLSL. Keep WGSL for web-target translation only.

## Recomendación práctica para ALZE

**Pattern after bgfx for the RHI; pattern after Filament for PBR semantics; author shaders in HLSL and cross-compile.**

- **Why bgfx for RHI:** it's the proven pattern (opaque handles + sort-key submit + backend-agnostic shader blobs) that lets a small team add Vulkan/D3D12/Metal backends *incrementally* without rewriting game code. The current GL 3.3 renderer becomes `rhi_gl33` behind the same handle API.
- **Why Filament for PBR:** Google's docs are the canonical reference; the GGX + split-sum IBL + Disney diffuse formulas are battle-tested across a billion Android devices. Copy the math and the offline IBL prefilter workflow (`cmgen`-equivalent tool). Do *not* pull in Filament as a dependency — ALZE's no-RTTI/no-exceptions policy rules that out.
- **Why HLSL + dxc + SPIRV-Cross:** single-source shaders across GL today + Vulkan/D3D12/Metal tomorrow. This is what Diligent, The Forge, and most modern AAA pipelines do. Sidesteps bgfx's custom dialect and keeps Slang as an optional upgrade once the HLSL path is stable.
- **Defer:** a full render graph until the pass count justifies it; WGSL until a web build is on the roadmap; The Forge-level features (visibility buffer, bindless, mesh shaders) until after Vulkan is up.

First deliverable on this path: extract current GL calls into `alze::rhi::*` with opaque handles. No behavior change. Second deliverable: `alzec` shader compiler wrapping dxc + SPIRV-Cross producing GLSL 3.30. Third: GGX + prefiltered IBL port.

## Fuentes consultadas

- bgfx overview & API reference — https://bkaradzic.github.io/bgfx/overview.html
- bgfx internals (threading, sort key) — https://bkaradzic.github.io/bgfx/internals.html
- bgfx GitHub — https://github.com/bkaradzic/bgfx
- Filament PBR document (Filament.md) — https://google.github.io/filament/Filament.md.html
- Filament Materials guide — https://google.github.io/filament/Materials.md.html
- Filament GitHub — https://github.com/google/filament
- matc manpage (shader variants) — https://manpages.ubuntu.com/manpages/jammy/man1/matc.1.html
- NVIDIA Falcor GitHub — https://github.com/NVIDIAGameWorks/Falcor
- Falcor research landing — https://research.nvidia.com/labs/rtr/publication/kallweit22falcor/
- Magnum docs — https://doc.magnum.graphics/magnum/
- Magnum GitHub — https://github.com/mosra/magnum
- Diligent Engine — https://github.com/DiligentGraphics/DiligentEngine
- Diligent overview — https://diligentgraphics.com/diligent-engine/
- wgpu project — https://wgpu.rs/
- wgpu GitHub — https://github.com/gfx-rs/wgpu
- Sokol GitHub — https://github.com/floooh/sokol
- Weissflog "A Tour of sokol_gfx.h" — https://floooh.github.io/2017/07/29/sokol-gfx-tour.html
- The Forge GitHub — https://github.com/ConfettiFX/The-Forge
- Slang language site — http://shader-slang.org/
- Slang GitHub — https://github.com/shader-slang/slang
- DXC SPIR-V codegen — https://github.com/microsoft/DirectXShaderCompiler/blob/main/docs/SPIR-V.rst
- SPIRV-Cross — https://github.com/KhronosGroup/SPIRV-Cross
- "DirectX Adopting SPIR-V" (Microsoft DevBlog) — https://devblogs.microsoft.com/directx/directx-adopting-spir-v/
- O'Donnell, "FrameGraph: Extensible Rendering Architecture in Frostbite" (GDC 2017) — https://www.gdcvault.com/play/1024612/FrameGraph-Extensible-Rendering-Architecture-in
- Render Graphs overview (Loggini) — https://logins.github.io/graphics/2021/05/31/RenderGraphs.html
- Basis Universal / KTX2 — https://github.com/BinomialLLC/basis_universal
- Khronos KTX — https://www.khronos.org/ktx/
- IBL multiple-scattering write-up (Opsenica) — https://bruop.github.io/ibl/
