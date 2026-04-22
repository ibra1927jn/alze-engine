# Editor Architecture — Deep-Dive for ALZE Engine (r5)

> Round 5, cross-cutting systems. Target: `/root/repos/alze-engine` (C++17, SDL2+GL3.3, R1 recommended Dear ImGui).
> Editor has been touched only obliquely in prior rounds: `ue5.md` §8 flagged UHT/UPROPERTY + Slate monolithic cost, `unity_godot.md` §7 praised Godot's node-tree as the workflow itself, `re_engine_capcom.md` §6 noted RE Engine Studio as UE-like, and `ecs_engines.md` sketched property-schema macros. None of these drilled into *how you actually build one*. This file does.
> Bias: indie-scale pragmatism. An editor is the single biggest chunk of an engine most hobby projects never finish — Slate is **~100k+ LOC** inside UE5, Unity's editor is multi-million LOC, Godot's editor is **a Godot game** (~200k LOC of GDScript+C++). Treat every feature below as optional unless proven necessary.

---

## 1. The two editor-deployment patterns

A game engine editor lives in one of two worlds:

### 1.1 Pattern A — Play-in-Editor (PIE) / editor-is-runtime

The editor process *contains* a full copy of the game runtime. "Play" = spawn a simulated world in a viewport inside the same exe. UE, Unity, Godot, CryEngine, Source 2, RE Engine, Frostbite all ship this way.

**Pros:**
- **Iteration loop is cheap.** No "save → launch standalone → close → edit → relaunch" cycle. Edit → Ctrl+P → Esc → continue. Naughty Dog, Insomniac, UE, Unity all cite sub-second PIE entry as non-negotiable.
- **Debugger is already attached.** Hit a breakpoint from a script edited 3 seconds ago.
- **Shared memory layouts.** Same Mesh, Texture, Scene classes used at runtime are shown in the inspector — no serialize-to-disk round trip.
- **WYSIWYG.** Editor gizmos overlay the actual viewport; what you see is literally the game.

**Cons:**
- **Editor-only code contaminates runtime.** `#if WITH_EDITOR` is pervasive across UE's entire source tree (~30k occurrences in UE5.4). The shippable runtime binary must `#ifdef`-strip all of it or ship a bloated exe.
- **GC + reflection overhead in release.** UObject's editor metadata persists into runtime classes unless cooked out. UE's "cook" step is largely motivated by this stripping.
- **Crash in game script = crash in editor.** Unity mitigates with AppDomain reload (now Assemblies); UE's C++ hot reload is famously crash-prone and Live Coding tries to paper over it.
- **Editor boot time.** UE5 editor on a medium project: 60–120 s cold load. Unity: 30–90 s cold. Godot: 2–5 s. Boot time is almost entirely a function of how much runtime + import pipeline starts up.

### 1.2 Pattern B — Editor exports; runtime is a separate exe

Editor writes project files → "build" produces a standalone runtime exe that consumes a cooked data blob. Source (pre-2), old idTech, many custom AAA engines (Frostbite splits editor and runtime processes more cleanly than UE), most indie engines built on top of tools like TrenchBroom or custom Qt.

**Pros:**
- **Tiny runtime.** No editor-only code paths anywhere. Shippable exe can be <10 MB.
- **Editor can be anything.** Qt, Electron, WPF — doesn't have to be C++. Blender editor is Python+C; Source's Hammer is C++/MFC; Quake's WorldCraft was MFC.
- **Clean separation enforces architecture.** If the runtime can't talk to editor-mode assumptions, you won't accidentally rely on them.

**Cons:**
- **Iteration is expensive.** Every play-test is a full export + launch. Source 2 specifically moved toward PIE to fix this.
- **Two asset pipelines to maintain.** Editor format vs runtime format; bugs in the exporter are a constant tax.
- **Gizmo + preview quality lag runtime quality.** Editor lighting never quite matches shipped game, artists complain.

### 1.3 Pattern A' — Editor-as-viewport-in-game ("Handmade Hero" / Casey Muratori)

A degenerate, interesting case: the editor *is* the game, accessed via a debug console / toggle. No separate process, no separate tool. Every Handmade Hero stream demonstrates editing entities inside the running game via F1-toggled immediate-mode panels.

**Pros:** dead simple; zero extra code paths; any data you can reach from the game, you can edit.
**Cons:** no project management, no asset browser, no multi-scene; scales only to tiny projects or as a complement to a "real" editor later.

**ALZE fit:** the right v1 answer. Dear ImGui inside the game exe, editor panels toggled with F1/F2. Muratori's stream #400s and his "Writing a Better Editor" talk (Handmade Con 2016) are the playbook.

### 1.4 Pattern C — Web IDE / browser editor

Godot 4's web-editor, Roblox Studio (native but close), Dreams (in-game creation), Unity Muse. The editor runs in a browser or even on console. Useful for low-friction sharing and for modders, but production-grade authoring still happens in a native tool. Not a v1 concern for ALZE.

### Table 1 — Editor architecture patterns

| Pattern | Iter speed | Runtime bloat | Complexity | WYSIWYG | Examples |
|---|---|---|---|---|---|
| A — Play-in-Editor (PIE) | Excellent | High (need cook step) | Very high | Perfect | UE, Unity, Godot, RE Engine |
| B — Editor exports, separate runtime | Poor | Minimal | Medium | Imperfect | Source 1 / Hammer, idTech 4, most custom AAA |
| A' — ImGui-in-game (Handmade) | Excellent | Low (few MB ImGui) | Low | Perfect | Handmade Hero, many Bevy egui projects, jam engines |
| C — Web / remote IDE | Medium (network) | N/A | Very high | Medium | Godot web, Roblox Studio, Dreams |

---

## 2. Scene graph + serialization

### 2.1 What a "scene file" has to carry

Minimum: a tree or flat list of entities, each with a stable ID, a type tag, and a bag of properties. References between entities (prefab instancing, parent/child links, component→component pointers). Asset references (textures, meshes, scripts) as stable GUIDs, not raw paths. Sometimes: entity-relative transforms, authoring-only data (notes, bookmarks), editor-only metadata (last-opened camera, collapsed state of outliner rows).

### 2.2 Format options and their engines

**YAML** — Unity's `.unity`, `.prefab`, `.asset`, `.mat`, `.controller`. Human-readable, git-diffable, hand-editable in emergencies.

Unity specifically uses a restricted YAML subset (no anchors, stable doc IDs, deterministic key ordering via their serializer, "force text" asset mode for version control). A trivial cube scene:

```yaml
--- !u!1 &123456789
GameObject:
  m_Component:
  - component: {fileID: 123456790}
  - component: {fileID: 123456791}
  m_Name: Cube
--- !u!4 &123456790
Transform:
  m_LocalPosition: {x: 0, y: 1, z: 0}
```

`fileID` is scene-local, cross-scene references use GUIDs via `.meta` sidecar. Merge conflicts resolve with UnityYAMLMerge (semantic merger that understands the schema, not just text). Without it, naïve git merges of Unity YAML are lossy ~20% of the time.

**Text (custom) — Godot's `.tscn` / `.tres`.** INI-like, headered sections `[node name="..." type="..."]`, ref-counted resources expressed inline or by path. Smaller than YAML, still diffable. Godot wrote its own parser because YAML is slow to parse at scale.

**Binary (engine-custom) — UE's `.uasset` / `.umap`.** One header + serialized UObject tree + optional bulk-data sidecars (`.uexp`, `.ubulk`). Zero human-readability; git LFS territory. Fast to load, compact, but merges require Unreal Diff Tool (binary-aware). Cooked output is a *further* transform (`.pak` / IoStore) optimized for runtime, stripped of editor fields. UE also supports text-mode asset export (`.copy`) but it's a developer-only escape hatch.

**JSON** — many indie engines (Urho3D, bgfx-based hobby engines, Bevy's `scene.ron` is conceptually JSON-like). Slightly more structural noise than YAML, but universal tooling, trivial to parse with `nlohmann/json` or `rapidjson`, schema-less by default. Bloaty for large scenes: a 10k-entity scene that's 2 MB in binary can be 15–40 MB JSON.

**FlatBuffers / Cap'n Proto** — zero-copy, schema-first binary formats. Load time = mmap + cast. Godot's `.res` binary resources and many AAA engines' cooked formats use conceptually similar tricks (Insomniac's own format, Decima's `.core`, UE's IoStore chunks). FlatBuffers is by Wouter van Oortmerssen (Google, 2014). Cap'n Proto by Kenton Varda (Sandstorm, 2013). Both give you forward/backward compatibility via field IDs and default values, at the cost of an `.fbs`/`.capnp` schema file you must maintain alongside your C++ types.

**Protobuf / MessagePack** — intermediate: parse step required but fast, schema support good. Protobuf is overkill for scene data (its strength is network RPC).

### 2.3 Dual-format strategy (the industry answer)

Almost every modern engine does **two** formats:
- **Source-of-truth** = text (YAML/INI/JSON/custom) in version control.
- **Runtime** = binary (custom/FB/CP) generated by an import / cook step, gitignored.

UE: `.uasset` (authoring, still binary but semantically authoring) → cooked `.pak`. Unity: `.unity` YAML → Library/ binary cache + final Asset Bundles. Godot: `.tscn` text → `.pck` binary pack. ALZE should follow this: `.alze-scene.json` in git, `.alze-pack` binary on disk at build time. Text-first authoring means mergeable, diffable, grep-able projects — which is worth 10× more at indie scale than the microsecond load-time savings of binary-only.

### 2.4 Stable IDs — the single hardest serialization problem

The moment you have references between entities (e.g., a trigger volume that references a door entity), naïve integer indices break the instant someone inserts, duplicates, or reorders. Solutions used in practice:

- **GUIDs per entity**, assigned at creation, never reused. Unity does this (128-bit). Cost: 16 bytes per entity, nontrivial lookup.
- **Scene-local stable IDs**, uint64, assigned monotonically, never reused. UE's `FGuid` for assets, object-local `FName` for components. Cheaper than GUIDs inside a scene.
- **Path-based references** ("Root/Enemies/Goblin_03/Health"). Godot-style. Human-readable in YAML/tscn but fragile under renames.
- **GUID + path hint** (Unity's actual choice): GUID is canonical, path is a debug/repair hint if the GUID is ever lost.

### 2.5 Prefabs / nested scenes — the second hardest

A prefab is a scene fragment reused with overrides. UE Blueprint-actor instances, Unity Prefabs (with Variant overrides since 2018.3), Godot PackedScenes (inherited scenes). Serialization must store **only the diff** from the prefab, not the full object — otherwise an update to the prefab source doesn't propagate. Implementation: store `{prefab_guid, overrides: [{property_path, value}]}`. Merging conflicts between "base prefab changed X" and "instance overrode X" is the classic UX hellhole — UE and Unity have shipped and re-shipped this UI multiple times.

### Table 2 — Serialization format comparison

| Format | Git-friendly | Compact | Load speed | Schema evolution | Tooling |
|---|---|---|---|---|---|
| YAML (Unity) | Good (w/ semantic merge) | 3–5× vs binary | Slow | Manual | Huge ecosystem |
| Custom text (Godot .tscn) | Excellent | 2–3× vs binary | Medium | Manual, versioned headers | Engine-specific |
| JSON | Good | 3–6× vs binary | Medium (nlohmann/rapidjson) | Manual | Universal |
| Binary opaque (.uasset) | Poor (LFS mandatory) | Best | Fast | Versioned headers + migration code | Engine-specific |
| FlatBuffers | Poor (binary) | Very good | Fastest (mmap zero-copy) | Built-in via field IDs | Cross-language |
| Cap'n Proto | Poor (binary) | Very good | Fastest | Built-in | Cross-language |
| Protobuf | Poor | Good | Medium (parse step) | Built-in | Cross-language |
| MessagePack | Poor | Very good | Fast | Manual | Multiple parsers |

---

## 3. Undo / redo

### 3.1 Command pattern vs Memento

**Command pattern** (Gang of Four 1994): every edit is an object with `execute()` and `undo()`. Easy to compose, testable, but every feature has to write its inverse — if you add "rotate gizmo," you also write "un-rotate."

**Memento** (Gang of Four 1994): snapshot relevant state before edit, diff-restore on undo. Easier to add new edit types (no per-edit code), costs memory proportional to state size. Used by Blender for most operators (Blender's undo is a mix: full mesh duplication for destructive edits, step-stored transform deltas for simple ones).

Real editors hybridize:
- **Unity** uses a serialized-property-diff approach: on any `SerializedObject.ApplyModifiedProperties()`, it records a diff into the undo stack. Cheap for flat data; hairy for scene-graph topology changes.
- **UE** uses an explicit transaction system (`FScopedTransaction`) wrapping edits; inside the scope, modified UObject properties are snapshotted via the reflection system. Very powerful (reflection drives it automatically) but heavyweight — the transaction buffer is often the biggest allocation in the editor.
- **Blender** uses per-step memory pushes; full mesh data snapshotted for destructive edits, which is why Blender undo during sculpt gets slow on 10M-poly meshes (~200 MB per step).

### 3.2 Merging micro-operations (the dragging-slider problem)

Dragging a slider emits hundreds of value changes per second. Naïvely, each is a separate undo entry — now Ctrl+Z steps back through one slider at a time and the user is furious. Solutions:

- **Time-based coalescing**: if two adjacent commands are the same type + same target + within N ms (Unity: 200 ms default), merge. Released slider = commit.
- **Explicit begin/end transaction** (UE `FScopedTransaction`, Maya `undoInfo -openChunk/-closeChunk`). Gizmo enters drag → open chunk; drag release → close chunk. All intermediate values are a single undo.
- **Command `merge(prev)` method** (Qt's `QUndoCommand::mergeWith`): each command decides if it wants to collapse with its predecessor.

For ALZE v1 with ImGui: the ImGui paradigm is "every frame, call `DragFloat3(&position)`." You can detect drag start/end via `ImGui::IsItemActivated()` / `ImGui::IsItemDeactivatedAfterEdit()` and emit a single undo entry on deactivation. This is the idiomatic ImGui pattern and is much simpler than time-coalescing.

### 3.3 Undo across topology changes

Deleting an entity then undoing must restore it *with the same ID* and every other entity's references to it must still resolve. Easy if your commands store the full serialized entity (Memento-ish); hard if you try to diff. Most editors just accept the memory cost: Unity holds the full scene state of every non-trivial undo step, and the undo buffer is capped (default: 100 steps).

### 3.4 Redo-after-new-edit truncation

Standard behaviour: if you undo 3 steps then make a new edit, the 3 redo entries are discarded. Every major editor does this. Some (Emacs, Vim) implement undo-trees instead — branches on re-edit. Never seen in game editors; overkill.

---

## 4. Hot reload

The single biggest iteration-speed multiplier. Ordered easiest → hardest.

### 4.1 Data / asset hot reload (easy)

File watcher (inotify/ReadDirectoryChangesW/FSEvents) → on `WRITE_CLOSE` of a tracked asset, the engine's asset manager invalidates the cached handle and reloads. Textures, materials, shaders, meshes, sounds — trivial if assets are reference-counted and the engine already has a reload path from its load path. Implementation: ~200 LOC wrapping `efsw` (Martin Lucas Golini, cross-platform file-watch library) or directly inotify.

### 4.2 Shader hot reload (easy–medium)

Recompile GLSL/HLSL on disk change. Two gotchas:
- **Pipeline objects in Vulkan/DX12 bake the shader in.** Must rebuild the PSO. On GL, re-link the program. On ALZE (GL 3.3) this is trivial: on shader file change, re-compile + `glAttachShader` + `glLinkProgram` → replace the handle.
- **Uniform locations may change.** Cache by name, re-query after link.

Shader live-coding transforms iteration: a graphics programmer who was doing "edit-save-rebuild-relaunch" on a 45-second loop drops to 0.5 seconds. Every engine worth using has this, including ALZE R1's target libs (bgfx, sokol). Reference: Aras Pranckevičius' "Five Rendering Ideas from Frostbite / Assassin's Creed" blog series (aras-p.info, 2014–2018) documents shader hot-reload patterns, and Ben Houston / "ClayGL" docs.

### 4.3 Script hot reload — interpreted (easy)

Lua, Python, GDScript, JS. Swap the script file, reload the module, re-bind any references. Easy because scripts hold only data via interpreter globals; the engine's C++ state is untouched. Tools like Sol2 (Rapptz, sol2 C++ Lua binding) or pybind11 make this a few lines.

### 4.4 Script hot reload — managed (medium)

C# in Unity: "Assembly Reload" rebuilds the user's .dll, serializes the current editor state, unloads the old AppDomain (pre-2019) / Assembly (post-2019), reloads. Takes seconds on a medium project. Pain point: the Mono-era version forced a domain reload; Unity's "Enter Play Mode Options" (2019.3+) lets you skip the reload and accept that static state persists, dramatically faster but the user has to know what's safe.

Godot C# has similar assembly-reload semantics via .NET hot reload (AssemblyLoadContext isolation since 2022).

### 4.5 C++ hot reload (hard)

The grail. Three production approaches:

**Live++ (by Stefan Reinalter / Molecular Matters)** — replaces obj/pdb chunks in a running process. Supports MSVC x64, most recently AMD64 clang. Used by UE (as Live Coding), Remedy (Northlight), Ready At Dawn, many others. Commercial license. Handles most edits including new classes, new members with reasonable limits; cannot add virtual methods to existing classes with live instances without care.

**Recode** (by Mircea Marghidanu, successor of sorts to Runtime Compiled C++) — similar patching approach, open via commercial license.

**RuntimeCompiledC++** (Doug Binks, open source, 2010s–) — recompiles the module and loads as DLL with state migration via a serialization interface. Older, more manual; requires discipline in how state is exposed. github.com/RuntimeCompiledCPlusPlus.

**cppfront / Herb Sutter's live cppfront demos** — interesting but not production.

UE5's Live Coding (enabled by default in modern editors) is Live++ with Epic-specific gluing into the `UObject` reflection system. It handles adding new `UCLASS` members thanks to UHT regenerating metadata; it still struggles with:
- Changes to virtual tables of classes with live instances
- Header-only template instantiation changes
- Cross-module reference updates (sometimes needs full restart)

The UE5 docs explicitly say "Live Coding patches the running process without restarting; certain changes still require a full recompile." Epic CTO Tim Sweeney has publicly described C++ hot reload as "one of the hardest engineering problems at Epic" (multiple UE keynotes, 2021–2024).

For ALZE this is aspirational v3. The v1 answer is "restart the game, it's 2 seconds." ImGui + fast PIE entry keeps this pain manageable.

### Table 3 — Hot reload approaches

| Reload type | Difficulty | Runtime cost | Typical latency | Industry baseline |
|---|---|---|---|---|
| Asset / data file | Low | None | <50 ms | Universal |
| Shader | Low–medium | Negligible | 100–500 ms | Universal |
| Lua / Python / GDScript | Low | None | <100 ms | Godot, Defold, Cocos |
| C# (Unity/Godot) | Medium | Per-reload GC + state serialize | 1–10 s | Unity, Godot |
| C++ (Live++) | Very high | Small code-size overhead, debug symbols | 1–3 s per patch | UE, Remedy, Insomniac |
| C++ via DLL swap (RCC++) | Medium (you own state migration) | Small | 3–8 s | A few open projects |

---

## 5. Inspector — reflection-driven property editors

### 5.1 The inspector problem

For every component type in the engine, the editor needs to render a property panel. Manually: one `DrawInspector(MyComponent*)` per type. Maintainable at 10 types; hellish at 200.

Solution: **reflection**. The editor asks each type "what properties do you have?" and iterates.

### 5.2 Reflection options in C++17

C++ has no built-in reflection (C++26 is targeting it; static reflection TS is still in progress). Options:

**RTTR (rttr.org, by Axel Menzel, since 2014)** — runtime reflection without requiring RTTI. Register types via fluent API in a .cpp:

```cpp
RTTR_REGISTRATION {
  registration::class_<Transform>("Transform")
    .property("position", &Transform::position)
    .property("rotation", &Transform::rotation);
}
```

Pros: no codegen step, flexible. Cons: runtime registration cost, each property access is a `std::variant`-ish dispatch — slower than direct field access. Used by Godot (conceptually similar system, hand-rolled).

**refl-cpp (github.com/veselink1/refl-cpp, Veselin Karaganev, 2019–)** — compile-time reflection via a `REFL_AUTO(type(T), field(a), field(b))` macro per type. Zero runtime cost; everything iterates via template metaprogramming. Works with C++17 well; very slick. Cons: macro boilerplate per class, error messages from template depths are unreadable.

**Magic Enum (neargye)** — narrow but useful: enum → string at compile time via template-magic on `__PRETTY_FUNCTION__`. Pair with refl-cpp for enum property editing.

**Hand-rolled codegen (UHT)** — UE's UnrealHeaderTool. Scans .h files for `UCLASS`/`UPROPERTY` macros, emits `.generated.h` with type descriptor tables. Pros: full control, integrates with their serializer + GC + replication. Cons: a whole compiler frontend you now own. UHT is ~50k LOC. Reasonable for AAA; insane for indie.

**Hand-rolled descriptors (sketched in ALZE `ecs_engines.md` §11)** — each component type has a `static constexpr Property props[] = { {"pos", offsetof(T, pos), Type_Vec3}, ... }`. Works with C++17 `constexpr`. Zero runtime cost. The editor walks the table and dispatches on `Type_*`. Requires one line per field, but honest and readable. This is what Bevy-style Rust does via derive macros and what most indie C++ engines do via a helper macro:

```cpp
#define ALZE_PROPERTY(Name, Field, Type) \
  { #Name, offsetof(ClassType, Field), PropertyType::Type }

struct Transform {
  Vec3 position;
  Quat rotation;
  using ClassType = Transform;
  static constexpr Property properties[] = {
    ALZE_PROPERTY(position, position, Vec3),
    ALZE_PROPERTY(rotation, rotation, Quat),
  };
};
```

### 5.3 UE's UPROPERTY mechanics (reference)

`UPROPERTY(EditAnywhere, Category="Combat", meta=(ClampMin="0.0", ClampMax="100.0"))` compiles as expanded macro expansion into nothing in-line, but UHT parses the *source text* of the header and generates a `UClass` descriptor in `Foo.generated.h`. The descriptor feeds:
- Editor property grid (reads Category, meta hints like `ClampMin`, `UIMin`)
- Serialization (version-robust)
- Garbage collector (tracks UObject refs)
- Network replication (`Replicated`, `RepNotify`)
- Blueprint exposure (`BlueprintReadWrite`)

This single macro fans out to *five* subsystems, which is why it's both powerful and expensive. UE5's reflection is the spine of the whole engine.

ALZE v2 should mimic **only the editor-grid slice** of this, not the whole fan-out. Network replication is future, GC is unneeded (C++17 RAII), BP scripting is aspirational.

### 5.4 Property grid rendering (ImGui)

Once you have a property list, drawing it is trivial:

```cpp
for (auto& p : type.properties) {
  void* field = (char*)obj + p.offset;
  switch (p.type) {
    case PropertyType::Float: ImGui::DragFloat(p.name, (float*)field); break;
    case PropertyType::Vec3:  ImGui::DragFloat3(p.name, (float*)field); break;
    case PropertyType::Bool:  ImGui::Checkbox(p.name, (bool*)field); break;
    // ...
  }
}
```

Add: attribute flags (`ReadOnly`, `Range`, `Color`), tooltip support, category collapsing, customizer registry (per-type override for special widgets like `Curve`, `Gradient`, `Asset<T>`).

---

## 6. Viewport gizmos

### 6.1 ImGuizmo — the obvious pick

ImGuizmo (github.com/CedricGuillemet/ImGuizmo, Cédric Guillemet, since 2016) is the de facto indie translate/rotate/scale gizmo. ~2k LOC, single-file, drops into any ImGui app, renderer-agnostic (emits ImGui draw vertices).

Core API:
```cpp
ImGuizmo::SetRect(0, 0, viewport_w, viewport_h);
ImGuizmo::Manipulate(view_matrix, proj_matrix,
  ImGuizmo::TRANSLATE, ImGuizmo::LOCAL,
  transform_matrix); // modified in place
```

Supports: translate, rotate, scale, combined, bounds manipulation, view cube, snap-to-grid. Pairs with any camera (you provide the matrices). Lightweight enough to be the v1 answer for ALZE — no reason to write this.

### 6.2 Raycast picking

Select-by-click: unproject `(mouse_x, mouse_y)` to a world-space ray, intersect against scene AABB/OBB, return nearest hit. Two tiers:
- **Broad phase**: BVH / octree / uniform grid query with ray-AABB intersection.
- **Narrow phase**: ray-mesh intersection (Möller-Trumbore) for the few broad-phase candidates, or stay at AABB precision for v1.

For very-early ALZE, an even simpler pattern works: **GPU pick buffer**. Render the scene to an offscreen R32_UINT buffer writing entity-ID per fragment; on click, read back one pixel. Costs one extra framebuffer + one extra pass, no spatial DS needed. Used by Blender (Object-ID buffer), Unity (Scene view picking), Godot (Gizmo picking). Scales to whatever the scene draws already.

### 6.3 Gizmo conventions (worth copying)

- **Keyboard**: W/E/R cycling translate/rotate/scale (Maya + UE + Unity all converged here).
- **X/Y/Z axis lock** while dragging for constrained motion.
- **Shift for snap** toggle during drag.
- **Local vs world space** toggle (Q in Blender, button in UE).

Users coming from any major tool expect these; Cedric's ImGuizmo already bakes them in.

---

## 7. Asset browser + thumbnails

### 7.1 The UI problem

A grid of files with icons. Plus: folders, drag-drop into scene, drag-drop from OS, search, filters by type, in-place rename, context menu, import settings. Seems simple; everyone has dragged this feature out over months.

### 7.2 Thumbnail generation

**On-demand, cached**. When the user opens a folder, spawn background jobs that render thumbnails for visible items. Cache by (asset GUID + modified-time hash) → PNG on disk in a `.alze-cache/thumb/` folder. Popular pattern (Unity, UE, Godot all do this).

**Renderer**: spin up a tiny offscreen framebuffer, load the asset with runtime pipeline, render one frame with a fixed camera, read back, encode as PNG. For meshes: auto-frame by bounding sphere. For materials: render a sphere with the material. For textures: stretch into square.

**Cost**: 20–200 ms per thumbnail depending on asset complexity. Background job queue + priority on visible tiles. Godot 4's thumbnail system is a fair reference: github.com/godotengine/godot/tree/master/editor/plugins/*_preview_plugin.cpp.

### 7.3 Import pipeline

This is its own 300-line writeup — see r5 `dcc_asset_pipeline.md`. Briefly: source file (.fbx/.png/.wav) → importer (plugin by extension) → engine-native binary + sidecar metadata. Importer settings stored in a `.import` sidecar adjacent to the source (Unity's `.meta`, UE's embedded metadata). Re-import on source file change.

### 7.4 Unity Asset Database vs UE Content Browser

**Unity Asset Database** (docs.unity3d.com/Manual/AssetDatabase.html) — Every asset gets a GUID in a `.meta` file checked into git. Scripts query the DB via `AssetDatabase.FindAssets("t:Texture2D")`. Imports are declared via `AssetImporter` subclasses; users write custom importers by extending this. The "Library/" folder (gitignored) is a local binary cache keyed by GUID.

**UE Content Browser** (docs.unrealengine.com, Content Browser) — `.uasset` files contain both source and cooked variants, plus thumbnails, plus package-level metadata. No `.meta` sidecar — the asset *is* the metadata. Cooking strips editor-only pieces.

ALZE v1 answer: Unity-style because text metadata in git is more indie-friendly than opaque binaries requiring LFS. A `.alze-asset.json` sidecar is trivial and grep-able.

---

## 8. Level streaming in the editor

### 8.1 Why it matters in the editor, not just runtime

A 20 km² open world has ~20k assets. Loading all of it into the editor to place one rock: impossible (32 GB RAM filled). Solution: **sublevel / world-partition editing**.

### 8.2 UE5 World Partition (docs.unrealengine.com, 2021–)

Replaces UE4's manual sublevels / LevelStreamingVolumes. World is partitioned into a 2D grid (default 25600 uu per cell, runtime-configurable). Each cell is streamable. In the editor, you load only cells near your current camera. HLOD (Hierarchical LOD) provides low-detail proxies for non-loaded cells so the world looks continuous.

Companion: **One File Per Actor (OFPA)**. Historically UE actors lived inside the `.umap` file; editing the map = a merge conflict on the whole map. OFPA moves each actor to its own `.uasset` file in a sibling folder. Artists no longer stomp each other; git merges become per-actor, which is usually automatic. ALZE should steal this idea hard — a "scene" is a folder of entity files plus a manifest, not a monolithic blob.

### 8.3 HLOD

Pre-baked coarse mesh for a cell: all props in the cell collapsed into a few hundred triangles with baked textures. Rendered instead of the cell contents when the camera is far. Generated offline. ALZE v3 feature at earliest.

### 8.4 Editor camera streaming policy

Two policies in wide use:
- **Camera-proximity streaming**: cells within R meters of the editor camera are loaded. Pan the camera → cells stream in/out.
- **Explicit load**: artist picks cells from a map overview, loads them. UE4's original sublevel workflow.

Both are fine for v1; the camera-proximity flavour is more WYSIWYG.

---

## 9. Version control integration

### 9.1 The two worlds

**Perforce** — AAA default. Binary-aware, handles 100 GB+ repos, fine-grained locks (critical for binary assets where merge is impossible). Expensive (server licensing), steep ops. UE was designed around P4 (FPackage Source Control plugin is first-party P4).

**Git + Git LFS** — Indie default. LFS (Large File Storage, GitHub 2015) swaps binary blobs for pointers, storing the blob on a separate server. Works fine up to ~10 GB repo + 50 GB LFS. Unity 2018+ has GitHub integration; UE 4.21+ has Git+LFS plugin.

Other: SVN (historical), Plastic SCM (by Códice, now Unity Version Control — Unity acquired 2020), Mercurial (fading).

### 9.2 What the editor needs to do

- **Detect dirty files** and show status (A/M/D/?) in the asset browser.
- **Submit / commit dialog** with editable message + file selection + diff preview.
- **Lock file** (critical for binary assets on P4; pseudo-emulated on Git via branch protection or `git-lfs lock`).
- **Diff/merge scene files** — show only the actor-level changes, not raw bytes.

Unity's Smart Merge (UnityYAMLMerge) and UE's Diff Tool (integrated in Editor via the Content Browser's right-click → Diff Against Source Control) are the reference implementations. Both are semantic, not textual.

### 9.3 ALZE v1 answer

Git + Git LFS for binary blobs (meshes, textures, audio). Scenes in JSON, diffable natively. Editor just shows `git status` column in the asset browser; no bespoke merge tool needed in v1.

---

## 10. Plugin / extensibility

### 10.1 UE5 plugin system

A UE plugin is a folder with `.uplugin` (JSON descriptor) + one or more Modules (C++ DLLs with specific lifecycle callbacks). Modules can be:
- **Runtime**: shipped in the game
- **Editor**: only loaded in editor process
- **Developer**: test-only
- **UncookedOnly**: cooking-time only

Each module has `StartupModule()` / `ShutdownModule()`. Plugins can register new asset types, new Slate panels, new SIngInIntoNode types, new importers, new component classes, new Blueprint nodes. Archives: docs.unrealengine.com/latest/en-US/plugins-in-unreal-engine/.

Ref: "Extending the Editor with Plugins" (Epic, official docs); Alex Forsythe's YouTube series "What is a UPlugin" (2022); GDC 2019 "Plug In or Plug Out" (Mieszko Zielinski, Epic).

### 10.2 Unity package manager

Packages are NPM-ish: a folder with `package.json`, versioned, installed via manifest. Editor reloads assembly on install. First-party packages (`com.unity.render-pipelines.universal`) and Asset Store packages. Extension points: ScriptableObject-based settings, Editor-scripts via `[InitializeOnLoad]`, `[CustomEditor]` for property drawers, `[MenuItem]` for menu entries.

### 10.3 Godot EditorPlugin

Defined in GDScript or C++: extend `EditorPlugin`, register in `plugin.cfg`. API: `_enter_tree()` / `_exit_tree()` lifecycle, `add_control_to_dock()`, `add_custom_type()` for new node types, `_handles(object)` / `_edit(object)` to override the Inspector for a type. Hot-reloadable like the rest of Godot. Reference: docs.godotengine.org/en/stable/tutorials/plugins/editor/making_plugins.html.

### 10.4 Can you have editor extensibility without a script engine?

Yes — via DLLs. The engine loads `plugins/*.dll`, each DLL exports a C-ABI `RegisterPlugin(EngineAPI*)` entry point. Plugin registers new ImGui panels, importers, component types. This is how many C++-only engines work (bgfx examples, Hazel/TheCherno's teaching engine, Lumix Engine by Mikulas Florek).

Cons vs script engine: no sandbox, plugin must match ABI exactly (recompile for every engine version), crashes bring down editor. Pros: full native perf, zero binding overhead, no scripting runtime to maintain.

**ALZE v1**: probably no plugin system. Add modules as cmake subdirectories via the Godot-inspired scheme described in `unity_godot.md`. A real plugin system is v3.

---

## 11. ALZE applicability: v1 / v2 / v3

### v1 — "Ship an editor you can actually use this year"

- **Editor pattern**: A' (ImGui-in-game, Handmade-style). Toggle with F1. Zero extra process, zero extra build.
- **Scene format**: JSON (`nlohmann/json`). One file per scene in v1, one file per actor (OFPA-style) in v2.
- **Stable IDs**: uint64 monotonic, scene-local.
- **Gizmos**: ImGuizmo, drop-in.
- **Inspector**: hand-written `DrawInspector(Component*)` functions per component. Maybe 15–30 components in year one; this is fine.
- **Undo/redo**: command pattern with ImGui's `IsItemDeactivatedAfterEdit` for slider-coalescing. ~500 LOC.
- **Picking**: GPU ID-buffer, one extra framebuffer, one extra pass.
- **Asset browser**: ImGui grid view reading `assets/` filesystem. Thumbnails for textures only (meshes = generic icon).
- **Hot reload**: shaders only (watch `shaders/*.glsl` with `efsw`, re-link program on change). Scene assets hot-reload is v1.5.
- **VCS**: Git + Git LFS. No integration in editor; use `git` CLI externally.
- **Plugins**: none. Modules compiled in via CMake.

Budget: 3000–6000 LOC of editor code. One programmer-quarter to a programmer-half-year full-time.

### v2 — "Usable at a small-team scale"

- **Reflection-driven inspector**: refl-cpp or hand-macro scheme from `ecs_engines.md` §11. Now adding a component = zero editor work.
- **Scene format graduates to per-actor files** (OFPA-lite). Merges stop being painful.
- **Prefab system** with instance overrides.
- **Full hot reload**: shaders + textures + meshes + materials + scenes.
- **Asset browser**: thumbnails for all asset types, background jobs for rendering.
- **Undo/redo with transaction groups** (explicit open/close around drag operations).
- **Picking narrow-phase** (ray-triangle against selected object for precise gizmo handles).
- **Scripting**: Lua via Sol2, hot-reloadable. Gameplay not engine.
- **Editor layout persistence**: ImGui's .ini docking layout saved per workspace.
- **Basic VCS status** in asset browser.

Budget: another 4000–8000 LOC. Another programmer-year.

### v3 — "Could ship a small commercial game"

- **World Partition / streaming editor**: 2D-grid cells, camera-proximity loading, HLOD proxies.
- **C++ hot reload**: Live++ if budget allows, otherwise DLL-swap module reload.
- **Plugin system**: DLL-loaded modules with versioned C-ABI.
- **Semantic scene diff/merge tool** (custom; git's textual merge not always sufficient).
- **Multi-user collaboration** (probably aspirational; even UE's Multi-User Editor is fragile).
- **Sequencer / timeline** for cinematics.
- **Editor-only scripting** (Lua editor macros à la Unity editor scripts).

Budget: indefinite. This is where most hobby engines die.

---

## 12. Honest notes

**An editor is harder than a renderer.** This is the single most-repeated lesson from engine post-mortems. The renderer is a well-studied problem with shipped reference solutions (Filament, bgfx, Diligent). The editor is a UX problem with no consensus answer: every engine has rewritten its property grid at least twice. UE's Slate alone is 100k+ LOC; its editor module ecosystem is another several hundred k. Unity's editor is a few million LOC. Godot's editor is a Godot game of ~200k LOC. Casey Muratori's Handmade Hero editor (streamed publicly) is probably the best data point for what a *minimal* useful editor looks like, and it's still thousands of lines of careful UI work.

**Dear ImGui is the multiplier.** Without ImGui (Omar Cornut, github.com/ocornut/imgui, 2014–), an indie engine ends up writing ImGui-like code anyway, badly. With ImGui, you get docking, tables, drag-drop, theming, automatic layout, multi-viewport, and a vast knowledge base for ~15k LOC added to your build. The fact that R1 already recommended ImGui is the single most important editor decision on ALZE.

**Most of what users call "engine quality" is actually "editor quality."** When someone says "the Unity editor feels snappy" vs "the Godot editor is clunky," they're making an implicit aesthetic judgement about the editor, not the runtime. A renderer with 10% better ms/frame that ships inside a crappy editor loses to a mediocre renderer inside a polished one. Budget accordingly.

**The Handmade Hero approach still wins for year-one solo dev.** In-game debug UI, conventions over configuration, no separate editor exe, no asset pipeline more complicated than "re-read files." Casey's streams remain the most honest education available for how to make these trade-offs. Halcyon's Yuriy O'Donnell GDC/Siggraph talks cover the grown-up version: how frame graphs, task systems, and editor plugins come together in a production in-house engine.

---

## 13. Primary references

**ImGui ecosystem**
- Omar Cornut — "Dear ImGui" (github.com/ocornut/imgui, 2014–present). docs: github.com/ocornut/imgui/blob/master/docs/FAQ.md. Archive: web.archive.org/web/2024/https://github.com/ocornut/imgui.
- Omar Cornut — "IMGUI Paradigm" blog (ocornut.github.io / various, 2013–). Influenced by Casey Muratori's 2005 "Immediate Mode GUI" article (mollyrocket.com).
- Casey Muratori — "Immediate-Mode Graphical User Interfaces" (Molly Rocket, 2005). caseymuratori.com/blog_0001.
- Cédric Guillemet — "ImGuizmo" (github.com/CedricGuillemet/ImGuizmo, 2016–).

**Reflection**
- Axel Menzel — "RTTR: Run Time Type Reflection" (rttr.org, github.com/rttrorg/rttr, 2014–).
- Veselin Karaganev — "refl-cpp" (github.com/veselink1/refl-cpp, 2019–).
- Epic Games — "UnrealHeaderTool" internal docs; "Reflection in Unreal Engine" (docs.unrealengine.com, 2015–). Ben Humphrey blog post "Reflection in UE4" (wikitolearn.ubb / blogs, 2018).
- Herb Sutter — "C++ Reflection and Metaclasses: the continuing saga" (CppCon 2022, youtube).

**Hot reload**
- Stefan Reinalter — "Live++" (liveplusplus.tech, Molecular Matters, 2015–). Presentations at Digital Dragons, Remedy Open House.
- Doug Binks — "Runtime Compiled C++" (github.com/RuntimeCompiledCPlusPlus, 2012–). Also "Runtime Modifiable C++" CppCon 2018.
- Epic Games — "Live Coding" (docs.unrealengine.com, UE 4.22+, 2019–).
- Aras Pranckevičius — "Shader Hot Reload" multiple posts (aras-p.info, 2015–).

**Serialization**
- Unity Technologies — "YAML Scene Format" and "Smart Merge" (docs.unity3d.com/Manual/SmartMerge.html).
- Epic Games — "Unreal Asset Overview / Package System" (docs.unrealengine.com).
- Godot community — ".tscn file format" (docs.godotengine.org/en/stable/contributing/development/file_formats/tscn.html).
- Wouter van Oortmerssen — "FlatBuffers" (google.github.io/flatbuffers, 2014–).
- Kenton Varda — "Cap'n Proto" (capnproto.org, 2013–).

**Editor architecture talks**
- Yuriy O'Donnell — "Halcyon Rendering Architecture" GDC 2018 + "FrameGraph" GDC 2017 (gpuopen.com / GDC Vault).
- Casey Muratori — "Handmade Hero" episodes on editor-as-runtime; "Writing a Better Editor" Handmade Con 2016.
- Stefan Reinalter — "Molecule Engine" blog series (blog.molecular-matters.com, 2011–2020), especially multi-post series on reflection, tooling, hot reload.
- Mieszko Zielinski — "Plug In or Plug Out: Writing Unreal Engine Plugins" GDC 2019 (GDC Vault / gdcvault.com).
- Sebastian Lague — indie-level editor patterns across his engine-from-scratch series (YouTube, 2020–).

**Engine docs**
- Epic — Slate / UMG overview (docs.unrealengine.com); Content Browser; World Partition.
- Unity — Asset Database API; Package Manager; Editor Scripting (docs.unity3d.com).
- Godot — EditorPlugin class; Custom Resource system (docs.godotengine.org).
- Blender — "Editors and Operators" dev docs (wiki.blender.org).

**Undo/redo**
- Gang of Four — "Design Patterns" (Addison-Wesley 1994) — Command + Memento canonical reference.
- Qt Framework — QUndoCommand / QUndoStack docs (doc.qt.io) — de facto modern implementation reference.

**Version control**
- GitHub — "Git LFS" (git-lfs.com, 2015–).
- Perforce — Helix Core + UE plugin (perforce.com/unreal-engine).
- Unity — "UnityYAMLMerge" (docs.unity3d.com/Manual/SmartMerge.html).

---

## 14. Concrete v1 recommendation

**Build an editor-inside-game-exe using Dear ImGui + ImGuizmo + JSON scene files with hand-written `DrawInspector()` per component. Hot-reload shaders via `efsw`-watched GLSL files. Defer everything else to v2/v3.**

Rationale in one line: an editor is the largest engineering artefact in an engine, and the only honest path for a solo-to-tiny-team indie C++17 engine is to pick the smallest version of the editor that answers the user's question "can I place and tweak things and run the result in the same window?" and refuse to add a single feature beyond that until that bar is proven solid. UE5's Slate is 100k+ LOC because UE5 is UE5; ALZE is not, and should not pretend otherwise. The Handmade-Hero / Omar-Cornut-ImGui / Cédric-Guillemet-ImGuizmo combination has shipped more indie engines than any property-grid codegen framework ever will.
