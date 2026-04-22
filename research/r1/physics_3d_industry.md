# Physics 3D industry: PhysX, Havok, Bullet, Jolt

Research note for the ALZE Engine (C++17, no RTTI, no exceptions). The question is concrete: in Phase 7 we need a 3D rigid-body pipeline. Do we pull in an existing engine, or do we write our own, and if we write our own — which production engine should we pattern after?

## Overview

Four engines dominate shipped AAA 3D titles:

- **NVIDIA PhysX 5** — ubiquitous because Unity (before DOTS) and Unreal Engine 4 both embedded it by default. CPU + optional GPU pipeline. Open source under BSD-3 since PhysX 4.0 (Dec 2018), with the full GPU simulation source released in 2022 (PhysX 5). Default solver is TGS since 5.1.
- **Havok Physics** — the "premium AAA" tool for two decades. Owned by Microsoft since Oct 2015 (bought from Intel). 600+ shipped games including Halo, Dark Souls, Skyrim, Assassin's Creed, Destiny, Call of Duty. Proprietary, source available only under commercial license. Industry-gold cross-platform determinism.
- **Bullet 3** — Erwin Coumans' open-source project, Zlib license. Historically the "free Havok alternative". Last major release 2019; still maintained but not pushed hard. The pedagogical reference — the source is clean and readable, and a lot of what is public knowledge about GJK/EPA/Dbvt in game physics came out of Bullet.
- **Jolt Physics** — Jorrit Rouwe (Lead Game Tech, Guerrilla). MIT license. Started as a hobby project; shipped as the physics engine of Horizon Forbidden West (2022) and Horizon Call of the Mountain, and since Death Stranding 2. Extremely modern C++17, SIMD-first, multi-core-first. The current darling among devs building new engines.

Licensing matrix that matters for ALZE:

| Engine   | License   | Source | Cost    | Deps        |
|----------|-----------|--------|---------|-------------|
| PhysX 5  | BSD-3     | Full   | Free    | Large, CUDA for GPU |
| Havok    | Proprietary | Paid | Per-title, no royalty | Closed |
| Bullet 3 | Zlib      | Full   | Free    | Minimal     |
| Jolt     | MIT       | Full   | Free    | Minimal, no exceptions, no RTTI fits ALZE |

## PhysX 5

NVIDIA's PhysX is a full physics stack, not just rigid bodies.

- **Solvers**: PGS (Projected Gauss-Seidel) was the historical default. TGS (Temporal Gauss-Seidel) became default in 5.1 and is available on both CPU and GPU paths. TGS re-linearises constraints across sub-steps, giving much less drift on long chains and stacks at equal iteration count.
- **GPU rigid bodies** (opt-in): turning on GPU sim replaces broadphase, contact generation, shape/body management, and the constraint solver with CUDA kernels. Same API as CPU.
- **Articulations**: reduced-coordinate (Featherstone) chains for ragdolls, vehicles, robotics. Joint limits and drives.
- **Scenes**: `PxScene` is the simulation island; pruning structure (BVH) for scene queries.
- **Queries**: raycast, sweep, overlap — single-hit or multi-hit, with touching/blocking semantics for filters.
- **Shapes**: sphere, box, capsule, plane, convex hull, triangle mesh (BVH-indexed), height field, SDF (GPU path).
- **Soft/fluid/cloth (v5)**: FEM soft bodies, PBD particle system for fluids/cloth/inflatables, integrated from FLeX.
- **Broadphase**: SAP (Sweep-and-Prune, good for small-to-medium dynamic scenes), MBP (Multi-Box Pruning, scales to larger open worlds by bucketing space), GBP (GPU Broadphase, used when GPU sim is on).
- **CCD**: optional per-body, uses conservative-advancement TOI.
- **Simulation task system**: user plugs in a `PxCpuDispatcher`; PhysX issues tasks that map onto that dispatcher, compatible with most existing job systems.

## Havok Physics

Havok is the closed-source, battle-hardened benchmark.

- **Solver**: iterative constraint solver tuned over two decades for stability of stacks, vehicles, ragdolls at large scale. Full algorithm specifics are proprietary.
- **Determinism**: Havok guarantees bitwise-identical output across all supported platforms when the same stateful world is simulated; this is the key differentiator used in networked games with deterministic lockstep (RTS, fighting games, replay systems). Note: it is *stateful* determinism — a copy of the world only matches the original if all internal caches are also copied.
- **Convex decomposition**: supplied via V-HACD (voxel-based Hierarchical Approximate Convex Decomposition), the de facto tool for turning concave meshes into arrays of convex hulls usable in real-time sim. (V-HACD itself is now end-of-life; the replacement is CoACD.)
- **Vehicles, cloth, destruction**: separate Havok modules (Havok Cloth, Havok Destruction).
- **Licensing**: no royalty, no per-seat — priced per title, negotiated. Works for large budgets, not for indie/hobby engines.
- **No source** unless you sign an enterprise agreement.

For ALZE: Havok is off the table on licensing alone. We mention it because its *behaviours* (what a "good" vehicle feels like, what networked determinism feels like) are the target to hit.

## Bullet 3

Erwin Coumans' Bullet is the academic/OSS reference and was for years the default choice when PhysX was not available.

- **Broadphase**: two options in production — `btAxisSweep3` (incremental sweep-and-prune with fixed world bounds) and `btDbvtBroadphase` (dynamic AABB bounding-volume tree, two trees: one for static, one for dynamic). Dbvt is the one most people copy because it has no world-bounds assumption and is benchmark-competitive.
- **Narrowphase**: GJK (Gilbert-Johnson-Keerthi) for convex-convex distance and closest-points, plus Bullet's own EPA (Expanding Polytope Algorithm) to recover penetration depth and contact normal once GJK reports overlap. The combined GJK+EPA pipeline returns normal, penetration depth, and two contact points.
- **Solver**: sequential-impulse style PGS with successive over-relaxation, warm-started from previous frame's impulses. Bullet 3 added an OpenCL GPU-rigid-body branch (Erwin's work at NVIDIA/AMD) but it never fully displaced the CPU path in shipped games.
- **Collision shapes**: sphere, box, capsule, cylinder, cone, convex hull, compound (tree of child shapes with local transforms), triangle mesh with BVH (`btBvhTriangleMeshShape`), heightfield.
- **Serialization**: `.bullet` binary format, cross-platform (32/64-bit, little/big endian), via `btDefaultSerializer` / `btBulletWorldImporter`. You can ship a pre-built physics world.
- **Why it still matters**: the source is readable, the algorithms are named and separated cleanly, and every academic paper on game physics since 2005 cites Bullet's implementations. For ALZE this is the textbook.
- **Weakness**: last major release was 2019. Development has slowed; Jolt has largely eaten its mindshare for new engines.

## Jolt Physics

Jorrit Rouwe's engine, used in production at Guerrilla, open-sourced under MIT in 2021.

- **Shipped titles**: Horizon Forbidden West, Horizon Call of the Mountain, Death Stranding 2. Not yet at Bullet's game-count, but already at higher prestige-per-title.
- **Broadphase**: custom lock-free quad-tree with 4 children per node chosen specifically for SIMD width. Background tree rebuild happens concurrently with the current step. Multiple "broad-phase layers" let you bucket bodies (e.g. static/dynamic/debris) for faster pair culling.
- **Narrowphase**: GJK + EPA, with `TransformedShape` snapshot objects to keep query concurrency safe.
- **Solver**: sequential-impulse (in the Catto tradition), warm-started, with support for multiple sub-steps per frame (e.g. 120 Hz collision at 60 Hz render). Symplectic Euler integrator.
- **Contact caching**: persistent contact cache with `OnContactAdded/Persisted/Removed` callbacks. The cache drives warm-starting and stable stacks.
- **Determinism**: deterministic when APIs are called in identical order. `CROSS_PLATFORM_DETERMINISTIC` compile flag enables controlled FP ordering for bit-identical results across platforms. A `Check Determinism` mode in the Samples app rewinds and re-runs every step and diffs state.
- **Job system**: simulation step splits into jobs submitted to a `JobSystem` interface; ALZE can plug in its own. Lock-free islands + lock-free broadphase are the two load-bearing tricks that let it scale near-linearly to many cores.
- **Shapes**: sphere, box, capsule, cylinder, convex hull, triangle, plane, mesh, heightfield, compound (static + mutable), plus decorator shapes (scaled, rotated-translated, offset-center-of-mass).
- **Constraints**: fixed, distance, hinge, point, slider, swing-twist, six-DOF, path. Motorised.
- **Ragdolls and vehicles**: first-class support; vehicle is Guerrilla-tested.
- **Soft bodies**: added relatively recently (particle-based with skinning to joints).
- **Fits ALZE**: can be compiled with exceptions off and RTTI off. This is unusual and matters — PhysX and Bullet don't play as cleanly with `-fno-exceptions -fno-rtti`.

## Solver families explained

- **PGS (Projected Gauss-Seidel)** — classic iterative constraint solver. Solve each constraint independently, iterate. Robust. Quality and drift scale with iteration count; more iterations = more CPU. Used historically by Bullet, early PhysX, most textbook engines. Erin Catto's *sequential impulses* is PGS-in-velocity-space with warm-starting and accumulated-impulse clamping — that is what Box2D, Bullet, Jolt all use.
- **TGS (Temporal Gauss-Seidel)** — PhysX 3+ default. Instead of solving velocities once per step then stepping positions, TGS re-linearises constraints across multiple small sub-steps inside the one outer step. Large savings on drift in long chains and tall stacks at equal wall-time. Costs: more complex bookkeeping, slightly more state.
- **PBD (Position-Based Dynamics)** — Matthias Müller et al. 2007. Operate directly on positions, integrate velocities at the end. Very stable, trivial to make unconditionally stable. **XPBD** (Macklin-Müller-Chentanez 2016) fixes PBD's iteration-count dependence by making stiffness physically meaningful via a compliance term (inverse stiffness). XPBD-style solvers are used in cloth/fluid/soft-body paths of PhysX 5 (`PxPBDParticleSystem`), and in detailed-rigid-body research (Müller 2020).
- **Featherstone's algorithm** — reduced-coordinate articulated-body method. Each body has coordinates relative to its parent only along the unlocked DOF. O(N) for a chain of N links. Used for ragdolls and robots in PhysX and Unity's `ArticulationBody`. The alternative is the Lagrange-multiplier/maximal-coordinate approach (normal constraints) which is O(N³) worst-case but simpler to code.
- **LCP (Linear Complementarity Problem)** — the formal math underneath contact + friction + joint constraints. Dantzig, Lemke, PGS and sequential-impulse are all different ways to (approximately) solve the LCP at each step.

## Broadphase

- **Sweep-and-prune (SAP)** — incremental sort of AABB endpoints on each axis. Cheap when the set of objects is small and does not move between large buckets. Used by PhysX (SAP mode) and historically by Bullet (`btAxisSweep3`).
- **Multi-box pruning (MBP)** — SAP inside hand-specified regions. Used by PhysX for large open worlds to avoid SAP's quadratic blow-up at scale.
- **Dynamic AABB tree (Dbvt)** — dynamic BVH with rotation-based rebalancing. Bullet's `btDbvtBroadphase` pioneered this in game physics; Jolt uses a 4-ary variant for SIMD. No world-bounds required. The recommended choice for new engines.
- **Spatial hashing** — grid of fixed-size cells, insert each AABB into every overlapping cell. Great for uniform-sized bodies; bad when sizes vary by orders of magnitude.
- **GPU parallel broadphase** — PhysX's GBP and Bullet 3's OpenCL path. Parallel radix sort + bin-based pair generation. Complexity is less algorithmic and more about latency hiding.

## Narrowphase

- **GJK (Gilbert-Johnson-Keerthi)** — iterative closest-point between two convex shapes using Minkowski differences. Returns distance and the two closest points. If the origin is inside the Minkowski difference, the shapes overlap but GJK cannot by itself report penetration depth.
- **EPA (Expanding Polytope Algorithm)** — continues where GJK leaves off when overlap is detected. Builds a polytope around the origin in the Minkowski difference and expands it to find the closest face, giving penetration depth and contact normal. GJK + EPA together give (normal, depth, 2 contact points).
- **MPR (Minkowski Portal Refinement)** — alternative to GJK+EPA using a portal in Minkowski space. Simpler termination and sometimes more robust in degenerate cases.
- **SAT (Separating Axis Theorem)** — for polyhedra with known face normals (especially boxes), test projections along each face normal and each edge-edge cross product. Early-out on the first separating axis. Much faster than GJK for box-box; the recommended fast path for OBB-OBB.
- **CCD (Continuous Collision Detection)** — two families: conservative advancement (repeatedly compute distance via GJK, step the body the safe amount, iterate until TOI converges) and root-finding on the TOI equation. Bilateral advancement extends CA to handle rotational motion. Erin Catto's 2013 GDC talk is the canonical reference.

## Determinism

- **PhysX** — deterministic if the same scene runs with identical call order on the same hardware. Not cross-platform deterministic by default.
- **Havok** — bitwise cross-platform deterministic for stateful worlds. This is Havok's moat.
- **Bullet** — deterministic on a single platform in single-threaded mode; multi-threaded mode is not deterministic without extra work.
- **Jolt** — deterministic when API calls are made in the same order; `CROSS_PLATFORM_DETERMINISTIC` compile flag enables controlled FP ordering for cross-platform bit-identical runs. Documented path for lockstep networking.
- **Hardware vs software determinism** — same `x + y + z` can differ across CPUs if the compiler reorders additions (FP is non-associative). Software determinism means you control op order (e.g. always `((x+y)+z)` in a fixed iteration order over a sorted container). Some studios go further and use fixed-point arithmetic (Q32.32) — expensive and painful but truly bitwise-portable.

## En qué es bueno cada uno

- **PhysX 5** — best all-round feature set. Only engine with production GPU rigid bodies, PBD fluids and FEM soft bodies integrated. Unity/UE ecosystems.
- **Havok** — best determinism, best vehicles, best stability for giant open-world stacks. Proven for 20 years.
- **Bullet 3** — best as a reference implementation. Readable, well-documented, every technique clearly separated.
- **Jolt** — best multi-core scaling, best modern C++ code quality, cleanest API surface, MIT, ships in AAA. Near-linear scaling on many cores per the multicore-scaling paper.

## En qué falla cada uno

- **PhysX 5** — GPU setup is heavy (CUDA toolchain, per-platform quirks). The SDK is a large dependency and its internal allocators assume a lot. Some GPU paths are still NVIDIA-GPU-preferred. Overkill for a 2D-plus-limited-3D engine.
- **Havok** — licensing and closed source kill it for any non-enterprise project. No way to debug deeply without source.
- **Bullet 3** — development has stalled since 2019. API shows its age. Multi-threading is bolted on rather than architectural. Some ugly corners (`btAlignedObjectArray`, `btScalar` type dance).
- **Jolt** — fewer shipped titles than Bullet; smaller community, fewer Stack Overflow hits. Some features (soft bodies, SDF collisions) are newer and less battle-tested. Documentation is improving but still thinner than PhysX's.

## Qué podríamos copiar para ALZE Engine

Patterns to lift directly when writing the ALZE physics module:

- **Jolt's lock-free broadphase + island builder** — the two tricks that let physics scale with cores. Without these, a job system just contends on mutexes. Copy the architecture, not the code.
- **Jolt's contact cache + warm-starting lifecycle** (`added/persisted/removed`) — stable stacks, cheap per-step cost after first contact. Persistent contacts are the single biggest stability win.
- **Bullet's `btDbvtBroadphase` (dynamic AABB tree)** — readable, no world-bounds assumption, benchmark-competitive. The right broadphase for ALZE's first cut. Upgrade to Jolt-style 4-ary SIMD tree later if we need the cores.
- **TGS solver (PhysX)** — consider it over PGS if we want less drift and fewer iterations. More work to implement correctly but scales better for the same quality target. For v1, start with sequential-impulse PGS (Catto/Jolt style) and upgrade later.
- **GJK + EPA** for convex-convex (textbook: Christer Ericson, *Real-Time Collision Detection* ch. 9). SAT fast path for box/box and box/capsule — do not route everything through GJK when a specialised pair is 3-5x faster.
- **Shape library priority**: box, sphere, capsule, convex hull, triangle mesh (BVH inside), compound — in that order. Heightfield if the game needs terrain. Do not implement SDF, cloth, or FEM in v1.
- **Job-system-aware stepping** — split `broadphase → find-pairs → narrowphase → build-islands → solve → integrate` into phases with explicit sync points. Jolt-style. Every phase is parallelisable inside; sync points are between phases.
- **Deterministic mode** with fixed FP ordering (stable sort by body id, single-threaded solver pass as an optional mode), and `CROSS_PLATFORM_DETERMINISTIC`-style compile flag. Read Jolt's deterministic-sim docs and copy that contract into ALZE's API.
- **Persistent serialization** — Bullet's `.bullet` idea: let us bake a physics world once, load it at runtime. Nice to have, not v1.

## Qué NO copiar

- **PhysX's monolithic pedigree** — even now that it is BSD-3, the SDK is enormous, has its own allocator layer, its own task system, its own scene graph. Copying PhysX's architecture wholesale would dominate ALZE's code size.
- **Havok's licensing model** — irrelevant direction. Also do not try to hide implementations the way Havok does; ALZE benefits from open code.
- **Full Featherstone articulation in v1** — too heavy for a small engine's first physics pass. Start with joint constraints (hinge, point, slider, fixed) on regular rigid bodies. Add reduced-coordinate articulation only when a shipping game needs it (robots, exoskeletons, complex vehicles).
- **GPU rigid bodies in v1** — PhysX does it well, but the complexity tax is enormous for a team that does not already have CUDA/Compute infrastructure. Defer.
- **Soft bodies / FEM / PBD fluids in v1** — absorb into a later phase. Scope creep kills physics modules.
- **Bullet's ageing multi-threading bolt-ons** — if we are building threading in, architect it like Jolt, not like late-era Bullet.

## Recomendación práctica para ALZE

**Pattern after Jolt, use Bullet as the pedagogical reference.**

Concretely:

1. Do not take Jolt as a dependency on day one — it's MIT so we legally could, but we lose the opportunity to understand our own physics stack, and Jolt pulls in its own allocator, job system, and profiler conventions that will fight with ALZE's.
2. Write our own, *architected* like Jolt:
   - Quad-tree-or-Dbvt broadphase, lock-free.
   - GJK + EPA narrowphase with SAT fast paths for primitives.
   - Sequential-impulse PGS solver (Catto) with warm-starting and persistent contacts. TGS as a later optimisation.
   - Job-phased stepping with explicit sync points, plugged into ALZE's existing job system.
   - Deterministic mode behind a compile flag.
3. Keep Bullet's source open in a second tab as we write — it is the best-documented implementation of every algorithm we will need, and Erwin's code is clear enough to lift patterns from without copying it verbatim.
4. Keep Jolt's GDC talk and multicore-scaling paper as the architectural north star.
5. If at any point ALZE scope needs GPU rigid bodies, PBD fluids, or FEM soft bodies before we have bandwidth to write our own — fall back to Jolt as a dependency for that subsystem only. It is the cleanest MIT library on the market.

Reality check: a credible v1 ALZE rigid-body pipeline is 8-12 weeks of one engineer. The four engines above are each 5-15 years of many engineers. We are not competing on feature count; we are competing on fitting ALZE's style (no RTTI, no exceptions, job-system-native, deterministic). That target is reachable.

## Fuentes consultadas

- Jorrit Rouwe — *Architecting Jolt Physics for Horizon Forbidden West*, GDC 2022. <https://www.guerrilla-games.com/read/architecting-jolt-physics-for-horizon-forbidden-west> and slides at <https://jrouwe.nl/architectingjolt/>.
- Jolt Physics architecture docs — <https://jrouwe.github.io/JoltPhysics/>.
- Jolt Physics repo (README, license, scaling doc) — <https://github.com/jrouwe/JoltPhysics>.
- Jolt Multicore Scaling paper — <https://jrouwe.nl/jolt/JoltPhysicsMulticoreScaling.pdf>.
- NVIDIA PhysX 5 docs — rigid body dynamics, GPU rigid bodies, scene queries, articulations, soft bodies. <https://nvidia-omniverse.github.io/PhysX/physx/5.4.1/>.
- NVIDIA PhysX SDK announcement (BSD-3, 2018) — <https://developer.nvidia.com/blog/announcing-physx-sdk-4-0-an-open-source-physics-engine/>.
- PhysX 5 open-source announcement (2022) — <https://www.cgchannel.com/2022/11/nvidia-open-sources-physx-5/>.
- Havok Physics product page — <https://www.havok.com/havok-physics/>.
- Havok Physics for Unity FAQ (determinism discussion) — <https://docs.unity3d.com/Packages/com.havok.physics@0.1/manual/faq.html>.
- Microsoft acquires Havok (2015) — <https://blogs.microsoft.com/blog/2015/10/02/havok-to-join-microsoft/>.
- Bullet 2.80/2.82 User Manual — <https://www.cs.kent.edu/~ruttan/GameEngines/lectures/Bullet_User_Manual>.
- Bullet 3 repo — <https://github.com/bulletphysics/bullet3>.
- Erin Catto, *Fast and Simple Physics using Sequential Impulses*, GDC 2006 — <https://box2d.org/files/ErinCatto_SequentialImpulses_GDC2006.pdf>.
- Erin Catto, *Modeling and Solving Constraints*, GDC 2009 — <https://box2d.org/files/ErinCatto_ModelingAndSolvingConstraints_GDC2009.pdf>.
- Erin Catto, *Continuous Collision*, GDC 2013 — <https://box2d.org/files/ErinCatto_ContinuousCollision_GDC2013.pdf>.
- Erin Catto, *Understanding Constraints*, GDC 2014 — <https://box2d.org/files/ErinCatto_UnderstandingConstraints_GDC2014.pdf>.
- Featherstone's algorithm (Wikipedia + PhysX docs) — <https://en.wikipedia.org/wiki/Featherstone's_algorithm>.
- Müller et al., *Position Based Dynamics*, 2007.
- Macklin, Müller, Chentanez, *XPBD: Position-based Simulation of Compliant Constrained Dynamics*, MIG 2016 — <http://mmacklin.com/xpbd.pdf>.
- Christer Ericson, *Real-Time Collision Detection* (Morgan Kaufmann, 2005), chapters 5, 9, 12 — GJK/EPA, BVH, CCD.
- Gaffer On Games, *Deterministic Lockstep* — <https://gafferongames.com/post/deterministic_lockstep/>.
- V-HACD repo — <https://github.com/kmammou/v-hacd>.
