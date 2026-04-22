# Bevy & ECS-Native Engine Architectures

Research note for ALZE Engine (C++17, no-RTTI, no-exceptions, SDL2+GL3.3, ~25-30K LOC).
Target: give Phase 7 "ECS" a concrete architectural direction by studying Bevy, Flecs,
EnTT, Legion and Amethyst. Focus is on ECS mechanics, not full-engine alternatives.

## Overview

An Entity-Component-System (ECS) splits game state into three orthogonal concerns:

- **Entity** — an opaque id (usually `u32`/`u64` with a generation counter).
- **Component** — plain data attached to an entity (`Position`, `Velocity`, `Health`).
- **System** — free function that queries "all entities with components A & B" and
  transforms their data every tick.
- **Resource** — singleton global data (time, asset registry, input state) that is
  *not* attached to an entity but lives in the same world.

ECS is the practical incarnation of Data-Oriented Design (Richard Fabian's 2018 book
*Data-Oriented Design*, free online). The thesis is that modern CPUs are bottlenecked
on memory latency, so laying components out as Structure-of-Arrays (SoA) in contiguous
buffers lets iteration hit L1/L2 cache and auto-vectorise. OOP's virtual dispatch and
pointer-chased object graphs are the opposite pattern.

Two storage schools exist:

- **Archetype / table** (Bevy default, Flecs, Unity DOTS) — every unique *set of
  component types* gets its own SoA table. All entities sharing that set live in the
  same table, contiguously. Iteration is a tight loop over dense arrays. Adding or
  removing a component moves the entity to a different archetype (costly bulk copy).
- **Sparse-set** (EnTT default, Bevy opt-in) — each component type owns a sparse→dense
  index pair. Add/remove is O(1) with no data movement; iteration follows indirection
  through the sparse array and is less cache-friendly for multi-component queries.

Historical milestones that shaped modern ECS: Scott Bilas's 2002 GDC talk on Dungeon
Siege's "objects as aggregations"; Adam Martin's 2007 blog series that coined the
name; Overwatch's 2017 GDC talks by Timothy Ford ("Gameplay Architecture and Netcode")
and Dan Reed ("Networking Scripted Weapons"), which turned ECS into an industry
default by showing deterministic netcode fell out of the pattern for free; Unity's
2018 DOTS release; and Bevy 0.1 in August 2020 by Carter Anderson.

## Bevy architecture

Bevy is a Rust engine where the ECS is the *entire* programming model. Render, audio,
UI, input — all expressed as systems.

- **App + Plugin builder.** `App::new().add_plugins(DefaultPlugins).add_systems(Update, move_player).run()`. A `Plugin` is a trait with `fn build(&self, app: &mut App)` that registers resources, systems and schedules. `DefaultPlugins` is a tuple of ~15 plugins (window, input, asset, render, UI, audio).
- **Systems are plain functions.** Their parameters declare access: `fn move_player(mut q: Query<(&mut Transform, &Velocity), With<Player>>, time: Res<Time>)`. The scheduler inspects parameter types at registration to build a *conflict graph*.
- **Query<T, F>.** `T` is a tuple of component accesses (`&C` read, `&mut C` write, `Entity`, `Option<&C>`, `Ref<C>` for change-detection). `F` is a zero-sized *filter* (`With<T>`, `Without<T>`, `Changed<T>`, `Added<T>`, `Or<...>`).
- **Resources vs Components.** `Res<T>` / `ResMut<T>` for world-global singletons; components live on entities. Resources participate in the same read/write conflict analysis.
- **Events.** `EventWriter<E>` / `EventReader<E>`: a double-buffered queue, drained each frame, read via iterator with a per-system cursor so no event is missed or duplicated.
- **World = Entities + Archetypes + Tables + SparseSets.** The `World` contains: `Entities` (a slotmap of `EntityMeta`), `Archetypes` (every unique component set), `Tables` (the SoA columns — `BlobVec` type-erased contiguous storage), and `ComponentSparseSets` for components tagged `#[component(storage = "SparseSet")]`. Multiple archetypes can share a table if they only differ in sparse components.
- **Change detection.** Each component stores two ticks (`added`, `changed`). Queries filtered by `Changed<T>` only yield entities whose tick is newer than the system's last run. Nearly free (just an integer compare).
- **Commands.** `Commands` is a per-system buffer; `commands.spawn(...)`, `.entity(e).insert(...)`, `.despawn()` are *queued*. `ApplyDeferred` (automatically inserted between system sets with conflicting structural access) flushes them to the `World`. This is what makes parallel systems structurally safe.
- **Reflect.** `#[derive(Reflect)]` gives runtime type info (field names, types) → used by the editor, scene serialisation (RON), and inspector plugins.
- **wgpu render graph.** The renderer is *also* an ECS world (`RenderApp`), with its own systems each frame that extract from the main world, prepare GPU resources, and record render graph nodes. The graph is a DAG of passes (e.g. `main_opaque_pass_3d → main_transparent_pass_3d → bloom → tonemapping → ui`) where edges encode texture/buffer dependencies.

## Flecs architecture

Flecs (Sander Mertens, C with C++/Rust/C# bindings) is the most *feature-rich* ECS,
used in games and also sold as a runtime for behaviour graphs.

- **Relationships as first-class.** A component on an entity is really a pair `(Relation, Target)`. Unary components are the degenerate `(Component, 0)`. `child.add<ChildOf>(parent)` creates the hierarchy; `inventory.set<Item>(Coins, {30})` attaches *the same component type* to multiple targets. Built-in relations: `ChildOf`, `IsA` (prefab inheritance), `Identifier`, `Exclusive`.
- **Query DSL with terms.** A query is a list of terms; each term can be a component, a pair, a wildcard (`(DockedTo, *)` matches any target), or a variable (`SpaceShip, (DockedTo, $Planet), Planet($Planet)` — multi-hop join, essentially a datalog query).
- **Observers.** Callbacks keyed on `OnAdd`, `OnRemove`, `OnSet`, or custom events. Think "ECS reactive triggers".
- **Tags vs components.** A *tag* is an empty component (no data payload) used as a marker; a *component* carries data. Both go through the same storage machinery.
- **Pipeline phases.** Systems are assigned a phase tag: `OnLoad → PostLoad → PreUpdate → OnUpdate → OnValidate → PostUpdate → PreStore → OnStore`. The pipeline scans the phase order and emits a run list; custom phases can be inserted.
- **Modules.** A module is a function that registers its components, systems and prefabs under a named scope (stored as `ChildOf` children of the module entity). Gives library-level encapsulation.
- **REST explorer.** `flecs-hub/explorer` connects over HTTP to a running app (or runs in-browser via WASM) and lets you inspect entities, run queries live, edit components — essentially a free debug editor.

## EnTT architecture

EnTT (Michele Caini / "skypjack") is a header-only C++17 library, famously used by
Mojang for Minecraft Bedrock. Design goals: drop into any existing C++ codebase, no
macros, no engine opinion.

- **Sparse-set storage.** Each component type owns a `basic_sparse_set<Entity>` — a packed dense array + a sparse array indexed by entity id. O(1) add/remove, no data movement.
- **Views.** `registry.view<Position, Velocity>()` is a *non-owning* lightweight iterator: it picks the shortest of the component pools as the driver and filters by lookup into the others. Creation is essentially free; iteration eats the pointer chase.
- **Groups.** `registry.group<Position, Velocity>()` is *owning*: it keeps the pools sorted so that the N entities with all requested components sit contiguously at the front. Iteration becomes a true SoA scan (competitive with archetype ECS) — but a component can belong to only one owning group at a time, and group membership is maintained via signal handlers on add/remove.
- **Signals / sinks.** `sigh<void(Entity)>` is the signal type; `sink` the subscription interface. `registry.on_construct<T>()` / `on_update<T>()` / `on_destroy<T>()` return sinks. Groups and observers are built on top.
- **Runtime views.** `runtime_view` takes a list of component pool pointers chosen at runtime (not compile time), handy for scripting bridges and editor tooling.
- **Template-metaprogramming heavy.** The entire API is a cathedral of `std::tuple`, variadic templates, CRTP, and SFINAE. Compile times are real.
- **Header-only.** One `#include <entt/entt.hpp>` and you're done; no build-system integration needed.

## Archetype vs Sparse-set storage

Staffordshire University's CGVC 2025 paper *Run-time Performance Comparison of
Sparse-set and Archetype Entity-Component Systems* (StaffsUniGames/cgvc25-ecs-comparison)
quantifies the trade-off Mertens and Caini have debated for years:

| | Archetype (Bevy/Flecs) | Sparse-set (EnTT default) |
|-|-|-|
| Iterating multi-component query | Best (SoA, no indirection) | Indirection through sparse; 2-5× slower at scale |
| Adding / removing a component | Full row move between archetypes | O(1), no data movement |
| Entity spawn cost | Moderate (find/create archetype) | Cheap (append to pools) |
| Fragmentation on volatile components | Archetype explosion (many sparse tables) | None |
| Memory overhead per component | Low | Sparse array (`num_entities` slots) per type |

Rule of thumb: if your hot loop is "iterate 50k enemies with `(Transform, Velocity,
Health)` every frame" → archetype wins. If your pattern is "add/remove a `Stunned`
tag dozens of times per frame across many entities" → sparse-set wins. Bevy mitigates
by letting you tag individual components with `storage = "SparseSet"`; Flecs keeps
everything in archetypes but deduplicates via its table graph.

## System scheduling

- **Bevy — automatic parallel.** Each system declares a `FilteredAccessSet<ComponentId>` at registration (read set + write set + "with/without" filters). The scheduler builds a conflict graph: two systems conflict iff their write set intersects the other's read-or-write set *on the same archetype*. A worker pool executes non-conflicting systems concurrently. Ordering hints (`.before()`, `.after()`, `.chain()`, `SystemSet`) only constrain, never invent, parallelism. `ApplyDeferred` points flush command buffers between sets.
- **Flecs — phase pipeline + per-phase parallelism.** The pipeline sorts systems by phase. Within a phase, Flecs can run systems in parallel using its worker threads (`ecs_set_threads`) if their component access doesn't conflict. The conflict analysis is coarser than Bevy's because C lacks Rust's read/write type distinction — you declare access explicitly with `.inout()` / `.in()` / `.out()` flags on query terms.
- **EnTT — hand-rolled.** No scheduler. You write a `main_loop()` that calls systems in the order you want, and parallelism is whatever you wire with `<execution>` / TBB / your own thread pool. Clean and honest; the engine integrates, not dictates.

## En qué es bueno

- **Bevy** — Type safety and free parallelism. The Rust borrow checker extended into queries means "two systems writing the same component" is a compile-time error, and the scheduler gets parallelism without any programmer input. Plugin ecosystem is explosive (rapier, egui-inspector, leafwing-input, etc.).
- **Flecs** — Relationships and raw C performance. First-class `(Relation, Target)` pairs let you model inventory, faction, ownership, spatial containment, prefab inheritance without bespoke data structures. The REST explorer is a genuine differentiator (a free runtime editor). C core fits any language.
- **EnTT** — Drop-in modern C++. Header-only, opinion-free, integrates into existing engines (Minecraft Bedrock, many in-house engines). No macros, no engine framework to adopt — just a library.

## En qué falla

- **Bevy** — Rust learning cliff; lifetime errors in queries are notorious. The renderer and asset system churn API every release. Archetype fragmentation hurts if you thrash marker components (`Dead`, `Selected`, `Hovered`) — the mitigation is sparse-set storage, but then you lose SoA iteration speed.
- **Flecs** — The C macro API (`ECS_COMPONENT`, `ECS_SYSTEM`) feels clunky from modern languages; the C++ wrapper is heavier than EnTT. Behaviour can be hard to debug because so much happens through relationship propagation and observer chains.
- **EnTT** — Compile times blow up as you add view/group instantiations; error messages are template-explosion novellas. Cache locality on many-component queries is worse than archetype ECS at 100k+ entity scale. No built-in scheduler, no change detection, no events primitive — you build the engine around it.

## Qué podríamos copiar para ALZE Engine

Concrete, C++17-compatible (no RTTI, no exceptions) recipes for ALZE's Phase 7 ECS:

1. **Archetype SoA storage à la Bevy/Flecs, not per-component sparse sets.** ALZE's hot loops (render culling, transform update, particle tick) are iteration-heavy over stable component sets — archetype wins. Key structures: `ComponentId` (stable u32), `ArchetypeId`, `Archetype { ComponentId[] types; Column[] columns; Entity[] entities; }`, `Column { void* data; size_t stride; size_t len; size_t cap; }` (manual BlobVec). Add/remove re-homes the row via memcpy between archetypes.
2. **System access descriptor + automatic parallel scheduler.** Each system declares `ComponentAccess { std::vector<ComponentId> reads, writes; }` at registration. Build a conflict DAG at `world.buildSchedule()`; walk it with a lock-free work-stealing pool (ALZE already has no exceptions, so `std::thread` + `std::atomic` is fine). Two systems parallelise if `writes(A) ∩ (reads(B) ∪ writes(B)) == ∅`.
3. **Relationships as first-class pairs (Flecs idea).** Represent `(relation, target)` with a 64-bit packed id (`u32 relation | u32 target`) treated as a component. Gives parent-child, owned-by, docked-to, equipped-by without bespoke pointer fields in every struct. Implement `ChildOf` specifically to replace the scene-graph tree ALZE probably has.
4. **Change-detection tick per component column per entity.** Two global ticks per `World` (`current_tick`, `last_run`); each column stores a parallel `u32[]` of `changed_tick`. Systems consume a `world.changedSince(last_run)` filter. Cost: one u32 per component per entity, one compare per iteration. Enables incremental render/physics updates.
5. **Deferred command buffer applied at frame boundary.** `CommandBuffer` records `Spawn{archetype, data}`, `Despawn{entity}`, `Insert{entity, cid, data}`, `Remove{entity, cid}`. Each worker thread owns one; the scheduler flushes them sequentially between stages. Avoids iterator invalidation and archetype reshuffling mid-system — the source of 80% of ECS bugs.
6. **Resources as typed singletons separate from entity components.** `world.insertResource<RenderCtx>(...)`, `world.getResource<Input>()`. Stored in a `std::unordered_map<TypeId, std::unique_ptr<void, void(*)(void*)>>` (manual dtor since no RTTI). Cleanly separates "global engine state" from entity data — avoids the common anti-pattern of putting input/time on a singleton entity.
7. **Stages/phases (Flecs pipeline idea).** `PreUpdate → Update → PostUpdate → Render`. Systems register into a phase; command buffers flush at phase boundaries. Gives ALZE deterministic ordering without Bevy's full dependency-graph complexity.

## Qué NO copiar

- **Bevy's full borrow-checker ergonomics.** Impossible in C++17 without compile-time lifetime analysis. Don't try to reinvent `Query<&mut T>` safety at compile time — just document that handing out mutable refs from a query during a parallel stage is UB, and rely on the scheduler to forbid it.
- **Flecs's C macro mill (`ECS_COMPONENT`, `ECS_SYSTEM`, `ECS_TAG`).** Ugly from C++ and hides semantics behind preprocessor soup. Prefer template registration + constexpr ids.
- **EnTT's template-metaprogramming depth.** Compile-time `tuple`/SFINAE cathedrals inflate ALZE build times and wreck error messages. Use plain function pointers + type-erased void* columns, not variadic `view<Ts...>` cascades.
- **Flecs observers as core dispatch mechanism.** Reactive on-add/on-remove callbacks are powerful but make control flow non-local and hard to debug. Prefer explicit systems reading `Added<T>` / `Changed<T>` filters.
- **Bevy's sub-world (RenderApp) duality.** Elegant in Rust with move semantics, but in C++17 the lifetime management of two parallel `World` objects that extract from each other is a footgun. ALZE should keep render as systems *in the same world*.

## Fuentes consultadas

- https://bevyengine.org/learn/ — Bevy official learn book
- https://bevy-cheatbook.github.io/programming/ecs-intro.html — ECS intro
- https://bevy-cheatbook.github.io/patterns/component-storage.html — Table vs SparseSet
- https://bevy-cheatbook.github.io/programming/change-detection.html — Change detection
- https://bevy-cheatbook.github.io/programming/commands.html — Commands
- https://bevy-cheatbook.github.io/programming/system-order.html — System ordering
- https://bevy-cheatbook.github.io/gpu/intro.html — Render architecture
- https://deepwiki.com/bevyengine/bevy/2-entity-component-system-(ecs) — ECS deepwiki
- https://deepwiki.com/bevyengine/bevy/2.2-components-and-storage — Storage internals
- https://docs.rs/bevy/latest/bevy/ecs/archetype/index.html — Archetype module
- https://github.com/bevyengine/bevy/blob/main/crates/bevy_ecs/src/archetype.rs — Source
- https://taintedcoders.com/bevy/ecs — Bevy ECS reference
- https://taintedcoders.com/bevy/archetypes — Bevy archetypes
- https://github.com/SanderMertens/flecs — Flecs repo
- https://github.com/SanderMertens/flecs/blob/master/docs/Queries.md — Flecs queries
- https://github.com/SanderMertens/flecs/blob/master/docs/Quickstart.md — Flecs quickstart
- https://www.flecs.dev/flecs/md_docs_2ObserversManual.html — Flecs observers
- https://ajmmertens.medium.com/a-roadmap-to-entity-relationships-5b1d11ebb4eb — Relationships roadmap
- https://ajmmertens.medium.com/building-games-in-ecs-with-entity-relationships-657275ba2c6c — Relationships in games
- https://ajmmertens.medium.com/building-an-ecs-storage-in-pictures-642b8bfd6e04 — ECS storage visualised
- https://github.com/flecs-hub/explorer — Flecs REST explorer
- https://github.com/skypjack/entt — EnTT repo
- https://github.com/skypjack/entt/wiki/Entity-Component-System — EnTT wiki
- https://skypjack.github.io/2019-03-07-ecs-baf-part-2/ — ECS back and forth part 2
- https://skypjack.github.io/2020-08-02-ecs-baf-part-9/ — Sparse sets and EnTT
- https://github.com/amethyst/legion — Legion ECS repo
- https://amethyst.rs/posts/legion-ecs-v0.3/ — Legion 0.3 release
- https://csherratt.github.io/blog/posts/specs-and-legion/ — Specs vs Legion
- https://github.com/StaffsUniGames/cgvc25-ecs-comparison — Archetype vs sparse-set benchmark
- https://diglib.eg.org/items/6e291ae6-e32c-4c21-a89b-021fd9986ede — Staffs CGVC 2025 paper
- https://moonside.games/posts/archetypal-ecs-considered-harmful/ — Archetype critique
- https://www.dataorienteddesign.com/dodbook/ — Richard Fabian, DOD book
- https://www.gdcvault.com/play/1024001/-Overwatch-Gameplay-Architecture-and — Overwatch GDC 2017 Ford
- https://gdcvault.com/play/1024653/Networking-Scripted-Weapons-and-Abilities — Overwatch netcode talk
