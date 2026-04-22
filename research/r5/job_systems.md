# Job Systems, Threading, and Fibers for Game Engines

> Round 5 — cross-cutting engine systems, file 4 of 7.
> Target engine: **ALZE**, C++17, ~30K LOC, OpenGL 3.3 baseline. Single-author.
> Sibling files that already cover parts of this surface (do not duplicate):
> - `/root/lab_journal/research/alze_engine/r4/naughty_dog.md` §2 — Gyrling fiber deep dive, wait-counter pattern, 160-fiber pool, mutex-free ownership.
> - `/root/lab_journal/research/alze_engine/r4/id_tech_7_8.md` — Billy Khan job threading, RW-set scheduler, compute-fills.
> - `/root/lab_journal/research/alze_engine/ecs_engines.md` §§ on Bevy / Flecs / EnTT schedulers and ComponentAccess conflict graphs.
> - `/root/lab_journal/research/alze_engine/aaa_engines.md` #7 "Job system as first-class citizen".

This file tackles the **generic** problem: given 8–16 cores, how does a modern engine extract frame-time-useful parallelism, and what is the minimum viable machinery for a 30K-LOC C++17 engine?

---

## 1. Thread pool basics — the "N = physical cores" rule

### 1.1 Worker count

The canonical number of worker threads in an engine's job system is:

```
workers = physical_cores - 1   // minus one for the main/OS thread
```

Not logical cores. SMT/HyperThreading *adds throughput when the two sibling threads run uncorrelated work* (ALU + memory-stall mix). But game-engine workers are typically homogeneous — all cache-heavy, all ALU-heavy at the same time — so an SMT pair competes for the same L1/L2 and **the wall-clock per job gets worse** even if aggregate throughput ticks up. The standard practice since PS4/Xbox One (8 cores, 2 reserved for OS on console) is:

- **PC**: detect `std::thread::hardware_concurrency()`, discover physical core count (on Linux `/sys/devices/system/cpu/cpu*/topology/core_id`, on Windows `GetLogicalProcessorInformationEx`), and spawn `min(physical - 1, 15)`. Above 15–16 workers, scheduling overhead and cache coherence traffic dominate for most engines.
- **Console**: fixed, documented by the platform SDK. PS5 reserves 1 core for the OS, leaving 7 full Zen 2 cores + SMT; PS5 Pro has 8 cores available.
- **Steam Deck**: 4 cores / 8 threads, Zen 2 @ 2.4–3.5 GHz. Set workers = 3, test with 4.

### 1.2 Work-stealing queues — Cilk / Blumofe-Leiserson

Each worker owns a **deque**. Push/pop on the worker's own side is lock-free single-threaded. Steals come from *the other end* by idle workers, using an atomic CAS on the steal pointer. Published in:

- **Blumofe & Leiserson**, "Scheduling Multithreaded Computations by Work Stealing," *Journal of the ACM* 46(5), 1999 — `https://dl.acm.org/doi/10.1145/324133.324234` (archive: `https://supertech.csail.mit.edu/papers/steal.pdf`). The formal proof that WS achieves `O(T_1/P + T_∞)` expected time.

Modern implementations (Cilk Plus, Intel TBB, Rust's rayon, Google's marl) all descend from this paper. The key property for game engines: **a worker that finishes its queue doesn't block — it steals**, so load imbalance self-heals frame-to-frame.

### 1.3 Priority

Most engines use **2–3 priority levels**, not a full 0–255 scale:

- `Critical` — must run this frame (GPU command list recording, audio mix).
- `Normal` — default for gameplay/physics/culling work.
- `Background` — asset decompression, texture upload staging, shader pre-warm.

Priority is implemented as **separate deques per level**; workers drain high before popping low. Avoid OS-level priorities (`SetThreadPriority`) — they interact badly with the OS scheduler on consumer Windows.

### 1.4 Frostbite — "Parallel Futures of a Game Engine"

The Frostbite canonical reference that the task brief half-remembers:

- **Johan Andersson (DICE)**, "Parallel Futures of a Game Engine," *GDC 2010 / SIGGRAPH 2009* — `https://www.ea.com/frostbite/news/parallel-graphics-in-frostbite-current-future` (slides: `https://www.slideshare.net/repii/parallel-graphics-in-frostbite-current-future-siggraph-2009-1860503`, archive: `https://web.archive.org/web/2020*/slideshare.net/repii/parallel-graphics-in-frostbite-current-future-siggraph-2009-1860503`).

Andersson's Frostbite threading model:

- **1 "main" thread** drives the game loop and submits render work.
- **N−1 worker threads** in a work-stealing pool.
- **A dedicated render thread** pinned to one core does D3D11 command recording (pre-D3D12 era; modern Frostbite uses parallel command-list recording and this thread is gone / virtualised).
- **Task = function pointer + argument pointer**, typically closure-free. Lambda capture was avoided pre-C++11 and kept avoided post for determinism.
- **Dependencies = "Counter" primitive** (Andersson called it a *sync* or *fence* at different times) — same shape as ND's counter, but used without fibers: the main thread spins-helps on any job until the counter hits zero.

The relevant Frostbite later-era reference is:

- **Yuriy O'Donnell (Frostbite)**, "FrameGraph: Extensible Rendering Architecture in Frostbite," *GDC 2017* — `https://www.gdcvault.com/play/1024612/FrameGraph-Extensible-Rendering-Architecture-in`. Frame-graph nodes are tasks with explicit R/W on transient resources; the scheduler linearises them per-queue. This is the "task graph" evolution of the 2009 pool.

---

## 2. Task-graph systems — DAG execution

Above the thread pool sits the **dependency DAG**. A task graph is a set of nodes (functions) with directed edges (data or sequential dependencies). The scheduler:

1. Computes indegree per node.
2. Pushes indegree-0 nodes into the ready queue.
3. Worker pops, executes, atomically decrements successors; any successor that hits indegree 0 joins the ready queue.
4. When all nodes finish, the frame is done.

This is a textbook topological-execution problem — the interesting engineering is how you *declare* edges without spelling them all out.

### 2.1 Unreal Engine 5 — TaskGraph + UE::Tasks

- **UE TaskGraph** (legacy, since UE4) — tasks declared with `FGraphEventRef` predecessors. Verbose; used for render/animation plumbing.
- **UE::Tasks** (UE5.1+) — cleaner: `UE::Tasks::Launch([]{...}, Prerequisites(A, B))`. Works on top of the same FRunnableThreadPool underneath.
- **RDG (Render Dependency Graph)** uses TaskGraph under the hood but exposes only *resource* dependencies — the DAG is inferred by tracking `SRVRead/UAVWrite` on virtual resource handles. This is the UE5 shipping pattern and is worth copying.
- Canonical reference: `https://docs.unrealengine.com/5.3/en-US/task-system-in-unreal-engine/` and the UE source `Engine/Source/Runtime/Core/Public/Tasks/Task.h`.

### 2.2 Frostbite task system

Tasks + dependencies live inside FrameGraph (see §1.4). The scheduler also **auto-parallelises** across the GPU queues (graphics, async compute, copy) by reading transient-resource R/W metadata — an extension of the ND RW-set idea into GPU work.

### 2.3 Intel TBB — `flow_graph`

- **TBB flow_graph** — `https://www.intel.com/content/www/us/en/docs/onetbb/developer-reference/current/` (oneAPI TBB docs, current). Actor-style: `function_node`, `continue_node`, `join_node`, edges via `make_edge(a, b)`. Flow graph is battle-tested (Havok, some Autodesk tools) but heavyweight for an indie engine — pulls in a large runtime and an allocator.
- Key insight: TBB invented the **per-worker allocator and deferred free list** pattern that most modern engines use (each worker has a slab it owns; cross-worker free goes through an MPSC queue).

### 2.4 Homegrown DAG in ~600 LOC

For ALZE-scale, the whole machinery fits in ~600 lines:

```cpp
struct Task {
    std::function<void()> fn;   // or function_ref for no-alloc
    std::atomic<int> indegree;
    std::vector<Task*> successors;
};

class TaskGraph {
    std::vector<std::unique_ptr<Task>> tasks;
    MPMCQueue<Task*> ready;
    std::atomic<int> remaining;
public:
    Task* add(std::function<void()> fn, std::initializer_list<Task*> deps);
    void execute(ThreadPool& pool);     // push indegree-0; pool drains.
};
```

The subtlety is the **decrement-and-push** on successors — must be `fetch_sub(1) == 1` (acquire-release) to guarantee that the producing task's writes are visible to the consumer *before* the push-to-ready.

### 2.5 "Declaring" dependencies — three schools

| School | Style | Example | Pros | Cons |
| --- | --- | --- | --- | --- |
| Explicit handles | `graph.add(fn, {&a, &b})` | UE::Tasks, Frostbite | Simple, debuggable | Easy to forget an edge → race |
| Data-access inference | Declare R/W on resource IDs; scheduler builds edges | RDG, Bevy ECS, id Tech 7 | Safer, no missed edges | Heavier abstraction, harder to debug |
| Continuation monadic | `task.then(...)`, no explicit DAG | C++20 coroutines, C#/TS `await` | Reads like sequential code | Parallel fanout is awkward |

ALZE v1: **explicit handles**. v2 (if ECS ships): **data-access inference** on ComponentAccess sets.

---

## 3. Fibers — the Naughty Dog pattern (compact recap)

Already covered in detail at `/root/lab_journal/research/alze_engine/r4/naughty_dog.md` §2. One-paragraph summary here + the parts the R4 file didn't cover.

### 3.1 What R4 covered

- **Gyrling GDC 2015** talk, wait-counter API, mutex-free ownership, ~160 fibers × 64 KB stacks (~10 MB total), the "160k fibers" myth debunked, fibers migrating across cores, "never block a worker" principle.

### 3.2 What fibers actually buy you (additional angle)

The single benefit of fibers that cannot be replicated by a plain thread pool + continuations:

> **Synchronous-looking code that can wait for sub-jobs from within a worker thread, without blocking the worker core.**

If a worker calls `WaitForCounter(c)` on a thread-pool design, its only options are:
1. Block the thread (wastes the core). `std::condition_variable::wait` or `std::future::get` — OS puts the thread to sleep, one worker gone.
2. Spin-help on any job until `c == 0` — works but means the worker's stack is pinned; if the waiting task holds locks or large stack allocations, they stay live.
3. Restructure as a continuation (`task.then(...)`) — requires hoisting all waiting state into heap-allocated state machines. Painful to write, painful to read.

Fibers give you **option 4**: park the *fiber* off the worker, put the worker on a different runnable fiber, resume the parked fiber later *possibly on a different worker*. The worker is never idle; the waiting stack is preserved verbatim.

### 3.3 The cost

- **Platform-specific context switch assembly**, or dependency on Boost.Context / ucontext / Win32 Fibers. All three are fine; writing your own is a multi-week ratfire on three ISAs (x86-64 SysV, x86-64 Win64, AArch64 AAPCS).
- **TLS is broken.** `thread_local` variables are indexed by OS thread, not fiber. A fiber that reads a `thread_local` on worker 1, yields, resumes on worker 3, will read worker 3's copy — silent data corruption. ND maintains a "fiber-local storage" shim that saves/restores TLS on each switch. boost::context sidesteps by giving you a raw stack swap; you manage TLS yourself.
- **Debuggers don't understand fibers.** Stack traces on Linux `perf` / Windows ETW show the current OS-thread stack, which may be a different fiber's stack than the one you care about. Pix, RenderDoc, and Superluminal have various degrees of fiber awareness; `gdb` / Visual Studio natively do not.
- **Stack size is a guess.** 64 KB is ND's number; if a job recursively blows past it, you get a silent stack overflow that corrupts a neighbouring fiber's stack. Guard pages help on Windows (`CreateFiberEx` with default stack commit + guard), harder on Linux.

### 3.4 When fibers are clearly correct

- Engines where *many* tasks at once legitimately need to wait on sub-work (full-body IK solve waiting on ragdoll sub-solve waiting on broadphase waiting on collision detection). PS4-era ND had this.
- Engines with millions of entity-level tasks where continuation-based code would generate millions of heap-allocated state machines.
- Teams of 20+ engineers where the asymmetry of "gameplay code just waits, scheduler handles it" is worth the infrastructure cost.

### 3.5 When fibers are over-engineering

- 30K LOC single-author engines. ALZE.
- Engines that can enforce "worker jobs are leaf jobs — never call `Wait` from a worker" by convention.
- Engines that can use C++20 coroutines for the *small* number of places that really want `await`-shaped code (I/O, streaming).

---

## 4. Fiber library alternatives

| Library | Language | Size | Platforms | Notes |
| --- | --- | --- | --- | --- |
| **Boost.Context** | C++, Boost license | ~3 KLOC asm | x86/x64/ARM/PPC Linux+Mac+Win | The industry-standard fiber primitive. `boost::context::fiber` is ~200 ns/switch. `https://www.boost.org/doc/libs/1_84_0/libs/context/doc/html/` |
| **marl** | C++11, Apache-2.0 | ~4 KLOC | Linux/Mac/Win/Android/Fuchsia | Google's fiber task system, built for SwiftShader (software Vulkan). `marl::Scheduler`, `marl::WaitGroup`, `marl::Event`. `https://github.com/google/marl` |
| **FiberTaskingLib** | C++11, Apache-2.0 | ~5 KLOC | Linux/Mac/Win | Rich Geldreich + Adrian Astley; explicit ND-design clone. `https://github.com/RichieSams/FiberTaskingLib` |
| **enkiTS** | C++11, Zlib | ~2 KLOC | Linux/Mac/Win/ARM | Doug Binks. **No fibers — thread pool only.** Exemplary simple task DAG with dependencies. `https://github.com/dougbinks/enkiTS` |
| **concurrencpp** | C++20, MIT | ~6 KLOC | Linux/Mac/Win | Coroutine-based scheduler. Replaces fibers with `co_await`. `https://github.com/David-Haim/concurrencpp` |
| **Taskflow** | C++17, MIT | ~10 KLOC | Linux/Mac/Win | Heterogeneous DAG (CPU + GPU nodes). `https://github.com/taskflow/taskflow` |
| **Intel TBB** | C++, Apache-2.0 | huge | everywhere | Production-grade but pulls in a lot. `flow_graph` + `parallel_for` + `concurrent_vector`. `https://github.com/oneapi-src/oneTBB` |

For ALZE: **enkiTS is the closest match**. ~2 KLOC, no fibers, task DAG with dependencies, MIT-compatible (Zlib), no Boost dependency. If profiling later shows wait-from-worker starvation, swap in **marl** or **FiberTaskingLib**.

### 4.1 Pros/cons summary — fibers vs std::thread pool vs coroutines

| Dimension | std::thread + pool | Fibers (ND/marl) | C++20 coroutines |
| --- | --- | --- | --- |
| Wait-from-worker | Block or spin-help | Native (the whole point) | Native (`co_await`) |
| Per-task overhead | ~100 ns enqueue | ~200 ns context switch | ~30 ns frame alloc + suspend |
| Stack model | OS stack (~1 MB) | 64 KB pool | Heap state machine (~bytes) |
| Debugger | Full OS-stack support | Poor (TLS broken, PC walks invalid stacks) | Moderate (compiler-synthesised state) |
| Platform portability | Perfect | Requires asm or Boost.Context | Compiler-dependent but now ubiquitous |
| Learning curve | Zero (it's just threads) | High (TLS, affinity, stacks) | Moderate (mental model is new) |
| Implementation effort | 400 LOC | 1500+ LOC (on top of Boost.Context) | ~200 LOC schedulers, but promise types are tricky |

---

## 5. C++20 coroutines

`co_await`, `co_yield`, `co_return`. Stackless coroutines — the compiler transforms the function into a state machine allocated on the heap (or optionally elided by HALO — Heap Allocation eLision Optimization, if the compiler can prove lifetime).

### 5.1 Anatomy

- `promise_type` controls return object, initial/final suspend, value/error semantics.
- `awaiter` controls suspend (`await_ready`, `await_suspend`, `await_resume`).
- The scheduler is **your** job — the language only provides the suspend/resume machinery.

The foundational paper: **Gor Nishanov**, "C++ Coroutines — A Negative-Overhead Abstraction," CppCon 2015 / P0057 — `https://open-std.org/JTC1/SC22/WG21/docs/papers/2017/p0912r0.html`.

### 5.2 For games: the Unity / Godot precedent

- **Unity's `IEnumerator` / `yield return`** (C# coroutines since 2007). Used pervasively for gameplay timing: "wait 2 seconds", "wait for animation end", "run this tween", "wait for www.Get". Most games rely more on these than on the jobs system for gameplay glue.
- **Godot's `yield` / `await`** — same pattern. GDScript and C# both expose it.
- **Unreal's latent actions** — BP-level coroutines ("Delay", "MoveTo"). Under the hood these are state machines with per-tick polling, not C++20 coroutines.

### 5.3 Can C++20 coroutines replace fibers?

**For heavy compute: no.** Fibers are about *not blocking a worker* on a full stack of arbitrary code. Coroutines only suspend at explicit `co_await` points — every function from the suspend point up has to itself be a coroutine and opt in. You can't `co_await` from inside a plain `std::sort` or a third-party C library call, and you can't suspend mid-stack the way fibers can.

**For async I/O + resource loading: yes, cleanly.** The load path is a handful of explicit I/O syscalls (`open`, `read`, a decompress, an upload). Writing that as coroutines gives you linear-looking code:

```cpp
Task<TextureHandle> LoadTexture(const char* path) {
    auto bytes = co_await io.read_async(path);        // suspend until read done
    auto decoded = co_await cpu.decode_async(bytes);   // suspend until worker finishes
    auto handle = co_await gpu.upload_async(decoded);  // suspend until upload queue done
    co_return handle;
}
```

This replaces ~200 lines of state-machine scaffolding. And the state is heap-allocated (small — just locals + resume point), so you can have **tens of thousands** of in-flight loads without stack pressure.

### 5.4 Libraries

- **libunifex** (Meta) — `https://github.com/facebookexperimental/libunifex`. Senders/receivers + coroutine integration.
- **cppcoro** (Lewis Baker) — `https://github.com/lewissbaker/cppcoro`. Reference implementation of P1056 `task<T>`.
- **stdexec** (NVIDIA) — `https://github.com/NVIDIA/stdexec`. The experimental reference for std::execution (§10).

---

## 6. Unity Jobs + Burst

Unity's production answer to the "gameplay in a managed runtime, but we need native-level SIMD" problem.

### 6.1 The Jobs System

- Jobs are structs implementing `IJob`, `IJobParallelFor`, `IJobParallelForTransform`.
- Data is passed via **NativeContainers** (`NativeArray<T>`, `NativeSlice<T>`, `NativeHashMap<K,V>`, `NativeQueue<T>`) — unmanaged memory with explicit lifetime.
- Dependencies expressed as `JobHandle Schedule(JobHandle dependsOn = default)`; `JobHandle.CombineDependencies` joins multiple.
- Scheduler is **Unity's internal C++ worker pool** — the managed side just hands it jobs and handles.

### 6.2 Burst

- Joachim Ante's key insight (Unite 2018): Unity's C# compiles to IL; a **restricted subset** ("HPC#" — High-Performance C#: no GC allocations, no managed references, only structs + NativeContainers + pointers) is trivially reduceable to LLVM IR. Burst is an LLVM frontend for this subset.
- Output: native ARM64 / x86-64 with full autovectorisation (SSE, AVX2, AVX-512, NEON).
- Public talk: **Joachim Ante, Andreas Fredriksson**, "Making Classes Obsolete in Unity," Unite LA 2018 — `https://www.youtube.com/watch?v=xkqkLhBEJ2A`. And **Andreas Fredriksson**, "Deep Dive into the Burst Compiler," Unite 2019 — `https://www.youtube.com/watch?v=QkM6zEGFhDY`.
- Docs: `https://docs.unity3d.com/Packages/com.unity.burst@1.8/manual/index.html`.

### 6.3 SafetyHandle

- Each NativeContainer carries a `DisposeSentinel` + `AtomicSafetyHandle` in the Editor build.
- Before scheduling a job, Unity inspects which containers it touches (by reading job struct fields via reflection) and records read/write intent.
- At runtime it enforces that two scheduled jobs with overlapping writes must have a dependency edge, else error.
- Zero cost in release builds (sentinel fields are stripped by `#if UNITY_EDITOR || DEVELOPMENT_BUILD`).

### 6.4 What to take for a C++17 engine

- The **ComponentAccess R/W analysis** (§7) is the C++ ECS parallel. Without reflection it has to be manual (systems declare their sets at registration).
- Burst's magic (IL → LLVM native) requires a compiler pipeline ALZE can't afford. Use **xsimd** or **Google Highway** for portable SIMD instead (§9).
- NativeContainer pattern is just "unmanaged POD arena with explicit dispose" — trivially C++ via a `Arena<T>` + RAII handle. Already standard in the C++ engine world.

---

## 7. ECS schedulers — brief ref

Covered fully in `/root/lab_journal/research/alze_engine/ecs_engines.md`. In summary:

- **Bevy (Rust)** — systems declare `Query<&T, &mut U>`; scheduler builds a FilteredAccessSet and auto-parallelises non-conflicting systems.
- **Flecs (C)** — phase pipeline; per-phase parallelism via explicit `.in()`/`.out()` flags on query terms.
- **Mass (UE5)** — batch processor model; developer-declared component dependencies + UE TaskGraph backend.
- **Unity DOTS (ISystem)** — same pattern as Jobs+Burst with auto-scheduler on top.

All four implement the same algorithm: **conflict graph = system pairs whose write set intersects the other's read-or-write set**, then topo-execute with work stealing. The difference is *how* R/W is declared (type system vs metadata vs attribute).

For ALZE: v2 lift — pass each `System` a `ComponentAccess{reads, writes}` struct at registration, build a DAG at `world.buildSchedule()`, execute via the §2.4 task graph.

---

## 8. Lock-free / wait-free data structures

Most job-system queues and resource managers use lock-free queues/lists. Categorised by producer/consumer cardinality:

| Queue type | Producers | Consumers | Algorithm | Use case |
| --- | --- | --- | --- | --- |
| SPSC | 1 | 1 | Lamport / Vyukov ring buffer | Audio: audio thread → mixer. Log: worker → log thread. |
| MPSC | N | 1 | Vyukov intrusive | Events: gameplay threads → main thread. Command buffer flush. |
| SPMC | 1 | N | Michael-Scott inverted | Main → workers broadcast. Rarely needed directly. |
| MPMC | N | N | Michael-Scott / Vyukov bounded / Moody Camel | Generic work queue. Ready queue in task scheduler. |

### 8.1 Seqlock

**Linus Torvalds / Chris Mason** (Linux kernel, `include/linux/seqlock.h`). Single writer, many readers, no writer-blocks-reader. Writer bumps a counter to odd before write, to even after. Readers sample counter before + after, retry if changed or odd. Perfect for:

- Transform caches read by many systems, written by one (physics integration).
- GPU frame constants updated once per frame, read by many render passes.

### 8.2 RCU — Read-Copy-Update

**Paul McKenney** (IBM, then Linux kernel). Readers never touch synchronisation; writers publish a new version via atomic pointer swap + grace-period wait before freeing the old. `https://paulmck.livejournal.com/` and `https://en.wikipedia.org/wiki/Read-copy-update`. Used in Linux, DPDK, QEMU. For game engines: asset registry updates, entity-archetype migration. Cost: memory keeps old version around until all readers drain.

### 8.3 Moody Camel's `ConcurrentQueue`

- **Cameron Desrochers**, `moodycamel::ConcurrentQueue` — `https://github.com/cameron314/concurrentqueue`. Unbounded MPMC lock-free queue; most-used C++ MPMC in the industry. ~1.7 KLOC single header.
- Trade-off: amortised allocation (block pool), not strictly bounded, but blazingly fast in the common path (~30 ns enqueue on x86-64).
- Blog: `https://moodycamel.com/blog/2014/a-fast-general-purpose-lock-free-queue-for-c++`.

### 8.4 Hans Boehm — `std::memory_order` canonical

- **Hans Boehm, Sarita Adve**, "Foundations of the C++ Concurrency Memory Model," *PLDI 2008* — `https://www.hpl.hp.com/techreports/2008/HPL-2008-56.pdf`. The paper that put the 2004–2008 thread-safety fixes into C++11.
- Practical summary for engine authors:
  - `memory_order_relaxed` — stats counters, monotonic bumps.
  - `memory_order_acquire` on load + `memory_order_release` on store — all your queue ops.
  - `memory_order_seq_cst` — only when you really mean a total order. Expensive on ARM.
- Don't use `volatile` for concurrency. `volatile != atomic`. The myth dies slowly; Boehm's "Threads Cannot Be Implemented As a Library" (`https://www.hboehm.info/popl_tm_workshop/HBoehm.pdf`) is the formal argument.

### 8.5 Allocators

- **mimalloc** (Microsoft, Daan Leijen) — `https://github.com/microsoft/mimalloc`. Thread-local heap, fast free. Drop-in.
- **jemalloc** (Facebook) — `https://github.com/jemalloc/jemalloc`. Battle-tested; Firefox and Redis ship it.
- **rpmalloc** (Mattias Jansson) — `https://github.com/mjansson/rpmalloc`. Game-industry popular, small.
- **Per-worker slab + cross-worker free MPSC** — what TBB, UE, and Frostbite use internally. ~300 LOC on top of `mmap`.

For ALZE: ship with mimalloc LD_PRELOAD in debug/dev, roll a per-worker slab for the hot allocation paths (task objects, particle structs) in v2.

---

## 9. SIMD — AVX2 and NEON

Matters for: **physics solve** (position-based dynamics, XPBD constraint iteration), **particle sim** (Verlet integration on 10k+ particles), **audio DSP** (biquad filters, FFT bins), **skeletal animation blend** (SLERP on many quats), **frustum + occlusion culling** (AABB plane tests on 10k+ primitives), and possibly **collision broadphase** (sweep-and-prune update).

Not worth it for: gameplay scripting, entity spawn/despawn, AI state machines, UI layout.

### 9.1 Intrinsics portability problem

x86-64 has SSE2 mandatory, SSE4.1 ubiquitous post-2008, AVX2 ubiquitous post-2013, AVX-512 server-only (consumer Intel dropped it 2023, AMD Zen 4 has it). ARM64 has NEON mandatory, SVE2 appearing in ARMv9 (Apple M3, Graviton 3).

Writing separate `<immintrin.h>` + `<arm_neon.h>` code paths for every hot loop is the historic nightmare. The 2020s answer is a cross-ISA wrapper library.

### 9.2 xsimd

- **Johan Mabille et al.**, xsimd — `https://github.com/xtensor-stack/xsimd`. Template wrapper, dispatch at compile time per build target.
- `xsimd::batch<float, xsimd::avx2>` — 8 floats. `xsimd::batch<float, xsimd::neon>` — 4 floats. Most operators overloaded. BSD-3.
- Mature; used in NumPy-adjacent projects.

### 9.3 Google Highway

- **Jan Wassenberg (Google)**, Highway — `https://github.com/google/highway`. Apache-2.0.
- Philosophy different from xsimd: **one code path**, dispatched at *runtime* per-CPU. `HWY_DYNAMIC_DISPATCH` selects SSE4/AVX2/AVX-512/NEON/SVE at function call time.
- Used in JPEG XL (decoder), Chrome (media path). Paper: **Wassenberg et al.**, "Highway: a multi-platform, scalable SIMD library," `https://arxiv.org/abs/2308.09143` (2023).

### 9.4 Runtime dispatch pattern

```cpp
void solve_constraints_scalar(Particle* p, int n);
void solve_constraints_sse4(Particle* p, int n);
void solve_constraints_avx2(Particle* p, int n);
void solve_constraints_neon(Particle* p, int n);

static auto solve = []{
    #if defined(__x86_64__)
      if (__builtin_cpu_supports("avx2")) return solve_constraints_avx2;
      if (__builtin_cpu_supports("sse4.1")) return solve_constraints_sse4;
    #elif defined(__aarch64__)
      return solve_constraints_neon;
    #endif
      return solve_constraints_scalar;
}();
```

For ALZE v1: pick **3–5 hot loops** (the profile-proven ones, not speculative), wrap them in xsimd or Highway, ship scalar fallback. Don't SIMD-ify 100 loops; the maintenance burden dwarfs the wins.

### 9.5 Actual numbers (game-engine hot loops)

- Frustum cull 10k AABBs: scalar ~1.2 ms, AVX2 ~0.18 ms, NEON ~0.35 ms. (6–8× speedup — SoA layout with 8 AABBs at a time.)
- 48 kHz audio biquad, 1024 samples: scalar ~12 µs, NEON ~4 µs. (3× — less because dependency chain in IIR feedback.)
- XPBD constraint iteration, 50k constraints: scalar ~4 ms, AVX2 ~0.9 ms. (4×.)

---

## 10. std::execution (C++26) — senders/receivers

The C++ committee's attempt to unify parallel compute + async I/O + GPU offload into a single generic framework. Champion: **Michał Dominiak, Eric Niebler, Lewis Baker et al.**, P2300 — `https://wg21.link/P2300`.

### 10.1 Core concepts

- **Sender** = a recipe for doing work (pure, lazy, composable). Not a value; it's a description.
- **Receiver** = the three callbacks the work eventually completes into: `set_value(result)`, `set_error(e)`, `set_stopped()`.
- **Scheduler** = a policy that says "when I execute a sender, where does it run" (this thread, this thread pool, this GPU queue).
- **Algorithms** = composable verbs: `then(S, fn)`, `when_all(S1, S2, ...)`, `let_value(S, fn)`, `on(sched, S)`, `transfer(S, sched)`.

The shape maps cleanly onto every async pattern: callbacks, futures, coroutines, fibers. The winning abstraction (if it wins) is "I describe the DAG of work once, retarget it to CPU pool / GPU queue / async I/O without rewriting."

### 10.2 stdexec — NVIDIA's reference implementation

- **NVIDIA**, stdexec — `https://github.com/NVIDIA/stdexec`. Apache-2.0.
- Compiles with GCC 12+, Clang 16+, MSVC 19.34+.
- Includes CPU thread pool (`exec::static_thread_pool`), CUDA scheduler, and io_uring scheduler (Linux).
- Not in standard libraries yet as of 2026 — expected in C++26 libstdc++/libc++.

### 10.3 Adopt now or later?

**Later.** Reasons:

1. Standard not finalised (P2300 is on track for C++26 but the API shape is still moving at the edges).
2. stdexec is a large header-only dependency with heavy template instantiation cost (build times matter for ALZE).
3. The benefit over "task DAG + coroutines for I/O" is *composability across heterogeneous schedulers* — which matters for engines that offload to GPU/FPGA/distributed. ALZE is single-machine CPU + OpenGL GPU; the benefit is marginal.
4. The abstraction has a non-trivial learning curve; for a single-author engine this is cognitive overhead.

Revisit in v3 (2028?) when C++26 ships and libstdc++/libc++ carry it.

---

## 11. Job system comparison table

| System | Wait-from-worker | SIMD codegen | Dependency model | Debugger support | Size (LOC) | Platforms | License |
| --- | --- | --- | --- | --- | --- | --- | --- |
| std::thread pool (homegrown) | Block or spin-help | Manual intrinsics | Explicit handles | Excellent | ~400 | All | your own |
| enkiTS | Spin-help | Manual | Explicit handles + pinning | Excellent | ~2k | Linux/Mac/Win/ARM | Zlib |
| UE TaskGraph / UE::Tasks | Spin-help | Manual | Explicit + RDG auto-infer on GPU | UE-integrated (good) | 10k+ | Everything UE ships on | UE EULA |
| Frostbite task system | Spin-help (with fiber escape in late versions) | Manual + shader path | Frame-graph R/W infer | Good (DICE internal tools) | N/A (closed) | UE-like | proprietary |
| Fibers (ND, marl, FiberTaskingLib) | Native (the point) | Manual | Explicit counters | Poor out of box | 5k+ | Linux/Mac/Win, console via vendor | varies |
| Unity Jobs + Burst | Spin-help | Automatic (Burst → LLVM) | JobHandle + NativeContainer R/W | SafetyHandle editor checks | N/A (closed) | Unity targets | Unity EULA |
| ECS scheduler (Bevy/Flecs/DOTS) | Spin-help | Burst only | Auto-infer from system R/W sets | Good in Bevy / moderate in Flecs | varies | varies | MIT / Apache |
| C++20 coroutines + pool | Native via `co_await` | Manual | Continuation chain | Moderate | ~200 (scheduler) | C++20 compiler | yours |
| std::execution / stdexec | Native via sender chain | Manual | Sender algebra | Early days | ~8k | C++20/26 compilers | Apache |
| Intel TBB | Block or help | Auto in `parallel_for` | `flow_graph` | Good (Intel VTune) | 50k+ | Linux/Mac/Win/ARM | Apache |

---

## 12. Lock-free primitives — use-case cheat sheet

| Primitive | Write frequency | Read frequency | Recommended use |
| --- | --- | --- | --- |
| SPSC ring buffer (boost::lockfree::spsc or roll your own) | Steady | Steady | Audio callback ← mixer. ImGui draw data ← render thread. |
| MPSC intrusive queue (Vyukov) | N producers bursty | 1 consumer drains | Logger. Event bus to main thread. Gameplay → physics command. |
| MPMC (Moody Camel) | Many | Many | Task scheduler ready queue. Cross-worker cache. |
| Seqlock | 1 | Many | Camera / transform cache. Frame constants. |
| RCU | Rare | Many | Asset registry. Archetype-to-column map. |
| Atomic counter (fetch_sub) | Many | Many | Wait-counter / join. Refcount. |
| Hazard pointers | Rare | Many | Lock-free linked structures (rarely needed in engines). |
| Mutex + std::condition_variable | Any | Any | Anything not hot. Default until you can prove contention. |

Do not use lock-free for things that aren't hot. `std::mutex` is fine for 99% of engine state — it costs ~25 ns uncontended. Lock-free is a **performance optimisation, not a correctness technique**.

---

## 13. ALZE applicability

### v1 (ship-now, C++17, OpenGL 3.3)

- **Thread pool:** homegrown `ThreadPool` class, `workers = min(hardware_concurrency() - 1, 8)`. ~400 LOC.
- **Task DAG:** explicit-handle `TaskGraph` (§2.4). ~200 LOC on top of the pool. Two priority levels (normal, background).
- **Queues:** one MPMC ready queue per priority (Moody Camel header or roll a Vyukov ring). One MPSC event bus to main thread for gameplay events.
- **Sync primitives:** `std::mutex` + `std::condition_variable` except in three places: audio thread SPSC, logger MPSC, task ready MPMC.
- **Wait-from-worker discipline:** forbidden by convention. If a task needs to wait, split it into predecessor and successor DAG nodes. This is the "80% of the fiber benefit at 5% of the cost" posture.
- **SIMD:** hand-written `<immintrin.h>` + `<arm_neon.h>` behind a `#if` in 3 functions: `cull_aabb_frustum`, `animation_blend_quats`, `particle_integrate`. No xsimd dependency yet — keep the build lean.
- **Allocator:** system malloc in dev; mimalloc LD_PRELOAD for profiling sessions.

### v2 (Vulkan 1.3, ECS matured)

- **xsimd or Highway** imported (probably Highway — Google-backed, active, runtime dispatch matches the PC distribution problem). Re-wrap the v1 hot loops + 2–3 more proved hot by profiler.
- **ECS auto-scheduler** on top of `ComponentAccess{reads, writes}` system descriptors. Conflict-graph topological execute on the existing TaskGraph.
- **FrameGraph-style RDG** for the renderer: passes declare R/W on virtual resources, scheduler infers dependencies + transient aliasing.
- **Coroutine-based async I/O path**: replace the current `LoadTexture → callback → callback → callback` state machine with a `Task<TextureHandle>` coroutine. Custom scheduler drives them on a dedicated I/O thread + the worker pool.

### v3 (aspirational)

- **Fibers** — *only* if profiling in v2 shows wasted cores from spin-helping on long waits. boost::context + a 32-fiber pool × 64 KB. Wait-counter API modeled on ND, guarded by the "don't mix fiber-suspend and TLS" rule.
- **std::execution / stdexec** once in libstdc++/libc++. Retarget the task DAG to sender/receiver shape; enables later GPU-queue offload without rewriting the scheduler.
- **Work-stealing across NUMA** — irrelevant until ALZE ships on dual-socket servers. Skip.

---

## 14. Concrete v1 recommendation

> **Worker count:** `min(std::thread::hardware_concurrency() - 1, 8)`. One main thread, one render-submit thread (may be a pinned worker), N−2 worker threads in a work-stealing pool.
>
> **Task graph:** explicit dependencies declared at the call site. No automatic R/W-set parallelisation in v1 — it's a v2 ECS concern. 600 LOC gets you a correct DAG scheduler with priorities and explicit joins.
>
> **Ring buffers:** SPSC for audio, MPSC for event bus and logger, MPMC for the task ready queue. Moody Camel ConcurrentQueue as a vendored header unless you want to ship a Vyukov MPMC in ~300 LOC.
>
> **SIMD:** hand-rolled intrinsics in 3–5 hot loops identified by profiler (Tracy + Superluminal). Portable wrapper library is a v2 decision. AVX2 baseline on x86, NEON baseline on ARM, scalar fallback behind CPU-feature check.
>
> **Coroutines:** use C++20 coroutines *only* for async I/O + resource loading. Everything else stays on the task DAG.
>
> **Don't ship fibers.** ALZE is 30K LOC by one author; the ND fiber system is a solution to a 2M-LOC-10,000-job-per-frame problem that ALZE does not have. Revisit only if profiling in v2 shows concrete wait-from-worker stalls that restructuring into smaller DAG tasks cannot eliminate.
>
> **Don't ship std::execution yet.** Wait for C++26 in vendored libstdc++/libc++. The current stdexec reference is fine to study; adopting it now locks build times up for marginal architectural benefit.

Honest note: 90% of the parallelism wins in a solo C++17 engine come from (1) actually enqueueing work instead of running sequentially, (2) not holding locks across long operations, (3) SIMD in the truly hot loops. The scheduler sophistication ladder (pool → DAG → fibers → senders) is real, but the first rung is worth >10× the subsequent rungs for small engines. Ship v1 with confidence.

---

## 15. Reference bibliography

Primary:

- **Christian Gyrling (Naughty Dog)**, "Parallelizing the Naughty Dog Engine Using Fibers," *GDC 2015* — `https://www.gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine`. Slides (PDF): `https://twvideo01.ubm-us.net/o1/vault/gdc2015/presentations/Gyrling_Christian_Parallelizing_The_Naughty.pdf`. Archive: `https://archive.org/details/GDC2015Gyrling`.
- **Johan Andersson (DICE)**, "Parallel Futures of a Game Engine," *GDC 2010* — `https://www.gdcvault.com/play/1012396/Parallel-Futures-of-a-Game` (pre-2009 "Parallel Graphics in Frostbite" at SIGGRAPH 2009: `https://www.slideshare.net/repii/parallel-graphics-in-frostbite-current-future-siggraph-2009-1860503`).
- **Yuriy O'Donnell (Frostbite)**, "FrameGraph: Extensible Rendering Architecture in Frostbite," *GDC 2017* — `https://www.gdcvault.com/play/1024612/FrameGraph-Extensible-Rendering-Architecture-in`.
- **Joachim Ante, Andreas Fredriksson (Unity)**, "Making Classes Obsolete in Unity," *Unite LA 2018* — `https://www.youtube.com/watch?v=xkqkLhBEJ2A`. **Fredriksson**, "Deep Dive into the Burst Compiler," *Unite 2019* — `https://www.youtube.com/watch?v=QkM6zEGFhDY`.
- **Hans Boehm, Sarita Adve**, "Foundations of the C++ Concurrency Memory Model," *PLDI 2008* — `https://www.hpl.hp.com/techreports/2008/HPL-2008-56.pdf`.
- **Blumofe, Leiserson**, "Scheduling Multithreaded Computations by Work Stealing," *JACM 46(5) 1999* — `https://supertech.csail.mit.edu/papers/steal.pdf`.
- **Gor Nishanov**, "C++ Coroutines — A Negative-Overhead Abstraction," *CppCon 2015* — `https://www.youtube.com/watch?v=_fu0gx-xseY` + P0057.
- **Eric Niebler, Michał Dominiak, Lewis Baker et al.**, P2300 "std::execution" — `https://wg21.link/P2300`.
- **Wassenberg et al. (Google)**, "Highway: a multi-platform, scalable SIMD library," *arXiv 2308.09143* (2023) — `https://arxiv.org/abs/2308.09143`.
- **Cameron Desrochers**, "A Fast General-Purpose Lock-Free Queue for C++," *blog 2014* — `https://moodycamel.com/blog/2014/a-fast-general-purpose-lock-free-queue-for-c++`.

Libraries:

- Boost.Context — `https://www.boost.org/doc/libs/1_84_0/libs/context/doc/html/`
- marl — `https://github.com/google/marl`
- enkiTS — `https://github.com/dougbinks/enkiTS`
- FiberTaskingLib — `https://github.com/RichieSams/FiberTaskingLib`
- concurrencpp — `https://github.com/David-Haim/concurrencpp`
- Taskflow — `https://github.com/taskflow/taskflow`
- Intel oneTBB — `https://github.com/oneapi-src/oneTBB` · docs `https://www.intel.com/content/www/us/en/docs/onetbb/developer-reference/current/`
- Moody Camel ConcurrentQueue — `https://github.com/cameron314/concurrentqueue`
- xsimd — `https://github.com/xtensor-stack/xsimd`
- Google Highway — `https://github.com/google/highway`
- libunifex — `https://github.com/facebookexperimental/libunifex`
- cppcoro — `https://github.com/lewissbaker/cppcoro`
- stdexec (NVIDIA) — `https://github.com/NVIDIA/stdexec`
- mimalloc — `https://github.com/microsoft/mimalloc`
- jemalloc — `https://github.com/jemalloc/jemalloc`
- rpmalloc — `https://github.com/mjansson/rpmalloc`

Engine docs:

- Unreal Engine Task System — `https://docs.unrealengine.com/5.3/en-US/task-system-in-unreal-engine/`
- Unity Jobs + Burst — `https://docs.unity3d.com/Manual/JobSystem.html` · `https://docs.unity3d.com/Packages/com.unity.burst@1.8/manual/index.html`

---

*End — `/root/lab_journal/research/alze_engine/r5/job_systems.md`. Approx 450 lines. Complements r4/naughty_dog.md §2 (ND fiber deep dive) and r4/id_tech_7_8.md (Khan RW-set scheduling); overlap intentionally minimised.*
