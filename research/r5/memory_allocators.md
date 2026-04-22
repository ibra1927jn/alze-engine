# Memory Allocators for Game Engines — deep dive R5

**Fecha:** 2026-04-22
**Scope:** cross-cutting engine systems, round 5, agent 5/7.
**Target repo:** `/root/repos/alze-engine` (C++17, no-RTTI, no-exceptions, SDL2+OpenGL 3.3, ~25-30K LOC).
**Motivation:** R1 touched allocators as a one-line "linear arenas per-frame + pool allocators per-type" in `_sintesis.md` §v1. This file treats memory management as a first-class engine subsystem and gives the concrete recipe for ALZE v1, v2, v3.

> Thesis: memory is the least-glamorous but highest-ROI engine subsystem. Frostbite, UE5, and Naughty Dog all spend disproportionate engineering energy here precisely because a single bad allocation at the wrong point in the frame is the difference between 60 fps and a visible hitch.

---

## 1. Why games don't use malloc

### 1.1 The frame budget arithmetic

- 60 fps = 16.666 ms per frame.
- 120 fps = 8.333 ms per frame.
- VR / competitive = 90-240 fps, 4-11 ms.

A single 100-microsecond `malloc` stall is 0.6% of a 60-fps frame — sounds small until you realize a typical game tick does tens of thousands of ops and hundreds of logical "allocations" (strings, vectors growing, temporary buffers, entities spawning). Stack 50 such stalls and you've lost 30% of the frame to the allocator.

Worse, `malloc` is **non-deterministic** in its worst case. glibc `ptmalloc`, jemalloc, tcmalloc, mimalloc all have fast-paths around 20-80 ns, but worst-case can hit several milliseconds when the allocator needs to `mmap`, coalesce, return memory to the OS, or walk long free-lists. In a 60-Hz game loop, the worst-case is what you feel as jank.

### 1.2 The four pains

1. **Fragmentation**. After a few hours of play, the heap is swiss cheese. A 4 MB texture allocation fails or forces a slow search even though the process owns 400 MB of free space. Console titles (fixed 8-16 GB RAM, no swap) literally OOM-crash to this. Consoles solve it by pre-partitioning RAM at boot; PC titles hide it with slack and hope.

2. **Latency spikes**. Generic allocators optimize for throughput and low memory overhead in the amortized case. Games need P99.9 determinism. `free()` on a long-lived object can trigger coalescing and `madvise(MADV_DONTNEED)` that costs hundreds of microseconds.

3. **TLB miss cascades**. Scattered 16-byte allocations land on random 4-KB pages. A single iteration over 10k entities might touch 10k pages, blow the TLB (1024 entries on Skylake dTLB L2), and pay ~20 cycles per re-walk. Arenas and pools put hot data in contiguous pages; huge pages cut TLB misses 512× (one 2 MB entry vs. 512 × 4 KB entries).

4. **Debugging nightmares**. Use-after-free on raw pointers is the #1 source of "random" crashes. Generic allocators return freshly-malloc'd blocks that may still hold stale data, making UAFs manifest minutes later in unrelated code.

### 1.3 Ship-title telemetry (order of magnitude)

- UE5 default projects do ~50,000-500,000 allocations per frame before tuning.
- Ship titles target **< 50 allocs per frame** in hot code paths (Hill, Gregory, Frostbite).
- Naughty Dog TLOU2 reportedly does **zero dynamic allocations** in gameplay hot code — all pre-allocated, indexed via handles. Debug builds have allocation tracking that fails CI if budgets exceeded.
- Frostbite partitions RAM at engine init into ~20 per-system budgets (Andersson, DICE); each system gets an arena and can only use its share.

---

## 2. Arena / linear allocator

### 2.1 Mechanics

```cpp
struct Arena {
    u8*   base;      // mmap'd or pre-reserved
    usize size;      // total capacity
    usize offset;    // bump pointer
};

void* arena_alloc(Arena* a, usize bytes, usize align) {
    usize p = (a->offset + (align-1)) & ~(align-1);
    if (p + bytes > a->size) return nullptr;
    a->offset = p + bytes;
    return a->base + p;
}

void arena_reset(Arena* a) { a->offset = 0; }   // O(1) free-all
```

- `alloc` is a single add + mask + compare. Typical: 2-4 ns, hits L1 every time after first page touch.
- `free` is O(1) bulk: reset offset at frame end. No per-object bookkeeping.
- No free-lists, no coalescing, no headers — zero metadata overhead.

### 2.2 When to use

- **Per-frame scratch memory**: temporary vectors, command lists, formatting strings, JSON parsing, shader uniform staging, visibility lists, animation intermediate poses, debug draw buffers.
- **Per-level scratch**: load-time parsing, asset cooking in-memory, scene graph construction.
- **Per-task scratch** (inside jobs): small stack-like bump arena given to each worker thread.

### 2.3 Typical sizes

- Per-frame arena: 16-64 MB. UE5 `FMemStack` (per-thread frame allocator) defaults around this order.
- Per-level arena: 128-512 MB (streaming-heavy open worlds).
- Per-task arena: 64 KB - 2 MB (fit in L2, stack-ish).

### 2.4 Variants worth knowing

- **Stack allocator**: arena + explicit `marker` (save offset, restore offset). Lets nested scopes free only what they allocated; unlike a flat arena, parents keep their allocations.
- **Double-buffered arena**: frame N writes into arena A, frame N+1 into arena B. Needed when GPU is still reading frame-N uniforms while CPU builds frame N+1.
- **Ring buffer**: for streaming command data where age = position. Use a free-fence (GPU timeline value) to know when the oldest region is safe to reuse.

### 2.5 Pitfalls

- **No destructors run** on reset. This is a feature for POD, a trap for non-POD. Arenas should be used with trivially destructible types. If you need destructors, build a linked list of `{dtor_fn, obj_ptr}` records and walk it at reset — "typed arena" pattern.
- **Out-of-memory mid-frame** ruins your day. Fix: make the arena a `reserve(1 GB)` + `commit on demand` region (see §8) so OOM means "this frame used more scratch than expected", not "process crash".
- **Cross-frame references**. If anything holds a pointer into the arena past reset, you have UAF. Solution: handle-based access, not pointers (see §5).

### 2.6 Primary refs
- Jason Gregory, *Game Engine Architecture* 3rd ed., §6.2.1 "Stack-Based Allocators", CRC Press 2018. https://www.gameenginebook.com/
- Niklas Frykholm, "Allocators and their memory budgets" — Bitsquid blog 2010-11. https://bitsquid.blogspot.com/2010/09/custom-memory-allocation-in-c.html (archive: https://web.archive.org/web/2024/https://bitsquid.blogspot.com/2010/09/custom-memory-allocation-in-c.html)
- RandyGaul, "Linear Allocator Implementation" — blog 2014. https://www.randygaul.net/2015/08/01/linear-allocator/
- Ryan Fleury, "Untangling Lifetimes: The Arena Allocator", Mr. 4th Programming blog 2023. https://www.rfleury.com/p/untangling-lifetimes-the-arena-allocator

---

## 3. Pool allocator

### 3.1 Mechanics

Pool = array of fixed-size slots + intrusive free-list.

```cpp
struct Pool {
    u8*   base;
    usize slot_size;    // sizeof(T) rounded to align
    usize slot_count;
    u32   free_head;    // index into free list, embedded in freed slots
};

void* pool_alloc(Pool* p) {
    if (p->free_head == U32_MAX) return nullptr;
    u32 idx = p->free_head;
    p->free_head = *(u32*)(p->base + idx * p->slot_size);
    return p->base + idx * p->slot_size;
}

void pool_free(Pool* p, void* ptr) {
    u32 idx = ((u8*)ptr - p->base) / p->slot_size;
    *(u32*)(p->base + idx * p->slot_size) = p->free_head;
    p->free_head = idx;
}
```

Both ops: O(1), ~5-15 ns, one cache-line touch (the freed slot).

### 3.2 When to use

One pool per type, especially per component type in an ECS. Each archetype chunk is effectively a pool. Examples:

- `Pool<Transform>` — 1024-65536 entries, heavily reused.
- `Pool<Particle>` — 100k-1M entries; allocate and free every frame.
- `Pool<AudioVoice>` — 128-512 entries, pool of hardware-capped resources.
- `Pool<PhysicsBody>` — 1000-50000, one per rigid body.
- `Pool<Entity>` — master entity pool; hand out handles.

### 3.3 Advantages

- **No fragmentation** — every slot is the same size, interchangeable.
- **Cache-coherent iteration** — array traversal at SIMD-friendly strides.
- **Predictable** — capacity is known at init; you can pre-touch pages.
- **Debug-friendly** — guard bytes, poison freed slots, watch-write protection per-slot possible.

### 3.4 Typical sizes

Rule of thumb: `slot_size × max_count` rounded up to the next page multiple. Most component types fit in:
- Tiny (16-32 B): Transform, Vel, Health — pool 1-4 MB.
- Small (64-256 B): MeshRenderer, Collider — pool 4-16 MB.
- Medium (1-4 KB): SkeletonPose, AudioBuffer — pool 16-64 MB.
- Large (> 16 KB): Texture payloads — pool per-mip level.

### 3.5 Parallel pool allocation

Single-threaded pool → lock contention if many workers allocate. Options:

- **Per-thread pools** (like tcmalloc/mimalloc thread caches). Each thread has its own pool; stealing at pool-exhaustion.
- **Lock-free free-list** using `CAS` on `free_head`. ABA-safe with versioned pointers (packed 48-bit idx + 16-bit gen in a 64-bit word).
- **Batch-alloc / batch-free**: grab 64 slots at once under one CAS, amortize contention 64×.

### 3.6 Primary refs
- Jason Gregory, *Game Engine Architecture*, §6.2.2 "Pool Allocators". Same book.
- Dmitry Vyukov, "Bounded MPMC queue" — used as template for lock-free pool freelists. http://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue
- Niklas Frykholm, "Building a Data-Oriented Entity System part 3: The Transform Component" — Bitsquid blog 2014-10. https://bitsquid.blogspot.com/2014/10/building-data-oriented-entity-system.html

---

## 4. Slab allocator

### 4.1 Idea

Slab = multiple buckets, each bucket is a pool of a fixed power-of-two size. Routes an `alloc(n)` request to the smallest bucket that fits. This is the general-purpose allocator pattern adapted from kernels (Bonwick, Solaris 1994) and reinvented by every modern malloc.

Typical buckets: 16 / 32 / 64 / 128 / 256 / 512 / 1024 / 2048 / 4096 B. Requests > 4 KB fall through to page-level allocator.

### 4.2 vs general-purpose malloc

jemalloc, tcmalloc, mimalloc are all slab-based under the hood, layered with thread-local caches, radix trees for pointer→size metadata, and background threads for decommit. Writing your own is rarely worth it — but **using one of them as the engine's backend replacement for libc malloc** is table stakes.

Mimalloc (Leijen 2019) is particularly game-friendly:
- Sharded free-lists for low contention.
- "Free delayed" — doesn't coalesce immediately; keeps recently-freed blocks hot.
- Small binary (~80 KB), single-header integration via `LD_PRELOAD` or linker override.
- Measured ~10-20% faster than jemalloc on typical C++ workloads.

### 4.3 When to use a custom slab

Rarely. Only when:
- You need deterministic layout (fixed-location testing, replay systems).
- You need to allocate from a specific memory region (VRAM scratchpad, NUMA node, locked pages).
- You want to instrument every alloc/free with zero overhead to the rest of the app.

### 4.4 ALZE recommendation

**Use mimalloc (or tcmalloc) as the default global allocator replacement**. Let arenas and pools cover the hot paths. Slab is the fallback for everything that doesn't fit the special cases.

### 4.5 Primary refs
- Jeff Bonwick, "The Slab Allocator: An Object-Caching Kernel Memory Allocator", USENIX Summer 1994. https://www.usenix.org/legacy/publications/library/proceedings/bos94/full_papers/bonwick.pdf
- Jason Evans, "A Scalable Concurrent malloc(3) Implementation for FreeBSD" (jemalloc), BSDCan 2006. https://www.bsdcan.org/2006/papers/jemalloc.pdf — docs at https://jemalloc.net/
- Daan Leijen, Benno Zorn, Leonardo de Moura, "Mimalloc: Free List Sharding in Action", Microsoft Research TR MSR-TR-2019-18, June 2019. https://www.microsoft.com/en-us/research/publication/mimalloc-free-list-sharding-in-action/
- Sanjay Ghemawat, Paul Menage, "TCMalloc : Thread-Caching Malloc", Google internal 2005, public docs https://google.github.io/tcmalloc/overview.html
- Andrei Alexandrescu, "std::allocator is to allocation what std::vector is to vexation", CppCon 2015. https://www.youtube.com/watch?v=LIb3L4vKZ7U — this talk is the spiritual origin of "stop using `operator new` in performance-critical C++".

---

## 5. Handle-based systems

### 5.1 The Handle<T>

```cpp
template<typename T>
struct Handle {
    u32 index;       // slot index into a pool
    u32 generation;  // increments on free/reuse

    bool valid(const Pool<T>& p) const {
        return p.generation[index] == generation;
    }
};
```

- Size: 8 bytes (can pack into 64 bits).
- Dereference: `pool.slots[h.index]` + generation check in debug.
- **UAF is impossible** if you check generation on every deref: the stale handle's gen won't match the slot's current gen.

### 5.2 Variants and packing

- **32-bit handle**: 16-20 bits index + 12-16 bits generation. Supports 64k-1M entities, generation wraps at 4k-64k reuses per slot. Wrap is a bug if not handled — solve by "retiring" (skip) exhausted slots.
- **64-bit handle**: 32/32 split. 4B entities, 4B generations. Industry-standard (Bitsquid / Stingray / Our Machinery pattern).
- **Typed vs. untyped**: `Handle<Mesh>` prevents cross-type use at compile time; a single `Handle` with a `type_id` field supports polymorphic containers.

### 5.3 UE5 FWeakObjectPtr

Close cousin:
- `FWeakObjectPtr` = `ObjectIndex (32 bit) + ObjectSerialNumber (32 bit)`.
- `GUObjectArray` (the global object table) maintains per-slot serial numbers.
- When an `UObject` is GC'd, the slot's serial is incremented. Every `IsValid()` call compares.
- Performance: single indirect read + one compare. ~2-3 ns in hot code.

### 5.4 Bitsquid / Stingray design

Niklas Frykholm documented this most clearly. Key insight: **handles are the unit of external reference; raw pointers exist only inside a subsystem that owns the memory**. This makes subsystems relocatable (defrag, serialize, stream) transparently.

Pattern:
```cpp
struct Manager {
    Array<T>  dense;     // actual data, packed
    Array<u32> sparse;   // handle.index -> dense index
    Array<u32> gen;      // handle.index -> generation
    Array<u32> free_list;
};
```
- `dense` is iterated for systems (cache-friendly).
- `sparse` maps handles to dense indices (allows pack-on-remove / swap-and-pop).
- `gen` detects stale handles.
- "swap-and-pop" free: move the last dense element into the freed slot, update its sparse mapping; keeps `dense` packed always.

This is exactly the sparse-set pattern used by EnTT and Flecs at the world level.

### 5.5 Why not shared_ptr

- 16 B per pointer (plus 16 B control block, plus atomic refcount writes on every copy).
- Non-trivial copy ctor fights SoA iteration.
- No UAF detection — dangling `weak_ptr` becomes a branch-predictable path rather than a data-driven invariant.
- Refcount contention across threads is a real hotspot (Aaltonen has ranted about this).

Handles are 8 B, trivially copyable, safe-by-default, and amenable to relocation.

### 5.6 Primary refs
- Niklas Frykholm, "Managing Decoupling Part 4: The ID Lookup Table", Bitsquid blog 2011-09. https://bitsquid.blogspot.com/2011/09/managing-decoupling-part-4-id-lookup.html (archive https://web.archive.org/web/2024/https://bitsquid.blogspot.com/2011/09/managing-decoupling-part-4-id-lookup.html)
- Niklas Frykholm, "A Handle-Based Memory Allocator for Small Objects", Bitsquid blog 2012-08. https://bitsquid.blogspot.com/2012/08/a-handle-based-memory-allocator-for.html
- Niklas Frykholm, "Managing Data Relationships", Bitsquid blog 2010-12. https://bitsquid.blogspot.com/2010/12/managing-data-relationships.html
- Epic Games, "FWeakObjectPtr" — UE5 source `Runtime/CoreUObject/Public/UObject/WeakObjectPtr.h`, https://github.com/EpicGames/UnrealEngine (private repo, or docs https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/CoreUObject/UObject/FWeakObjectPtr )
- Sander Mertens (Flecs author), "Building an ECS: Archetypes and Vectorization", 2019-2024 blog series. https://ajmmertens.medium.com/building-an-ecs-1-types-hierarchies-and-archetypes-21d6c9b08c48
- Intel Game Developer Zone, "Handle-based Data Structures for ECS", 2018. https://www.intel.com/content/www/us/en/developer/articles/technical/get-started-with-the-unity-entity-component-system-ecs-c-sharp-job-system-and-burst-compiler.html

---

## 6. Streaming allocators

### 6.1 Problem

Open-world games stream gigabytes of data per minute: textures, meshes, audio, animation, script chunks, navmesh. You cannot pause the game to `malloc` a 64 MB texture and `memcpy` it in — that's a 10-20 ms stall.

Solution pattern: **double-buffered / ring-buffered pinned memory regions with GPU fences**.

### 6.2 Mechanics

```
[ CPU writer ] -> [ ring-buffer in pinned system RAM ] -> [ GPU DMA ] -> [ VRAM residency pool ]
                      ^                                        ^
                  producer head                            consumer tail (fence-gated)
```

- Ring is a contiguous region (typ. 64-512 MB) pre-reserved at engine init.
- Head advances as the streamer's I/O thread writes freshly-read bytes (from disk via `io_uring` / `DirectStorage`).
- Tail advances as the GPU signals "done with this region" via a timeline semaphore.
- **Never allocate** during stream — just advance head; if head would overtake tail, the streamer back-pressures (throttles).

### 6.3 DirectStorage / PS5 Kraken-adjacent path

Modern PS5 / Xbox Series / Windows DirectStorage allow the SSD to feed GPU VRAM directly, with optional on-GPU or on-IO-block decompression:

- PS5 Kraken hw decompressor: ~8-9 GB/s decompressed, zero CPU. Game issues an "I/O request for compressed blob → target VRAM region".
- Windows DirectStorage 1.2+ with GDeflate: ~20 GB/s on NVMe + RTX 40-series GPUs (GPU-side decompression).
- UE5 Nanite + VT: streams clusters and texture tiles via this exact path.

Implication for allocators: the **target VRAM regions** must be pre-allocated as "streaming residency pools" managed as ring / sparse-arrays. No per-request VRAM alloc.

### 6.4 Pinned (non-pageable) system memory

- `cudaMallocHost` / `VirtualAlloc(MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE|PAGE_NOCACHE)` / `mmap(..., MAP_LOCKED)`.
- Locked into RAM, so DMA engines can read without the OS page-faulting.
- Max recommended: few hundred MB total (OS gets grumpy past that).

### 6.5 Typical layout

- **Upload ring**: 64-256 MB pinned RAM. CPU writes, GPU DMAs. Frame N+3 safe to overwrite frame N entries.
- **Residency pool**: VRAM, sized to GPU capacity minus framebuffer minus shader code. Sub-allocated into slabs per resource type (texture tiles, mesh pages, BVH nodes).
- **Promotion/demotion**: LRU eviction from VRAM back to RAM, then to SSD. Hierarchical memory just like CPU caches.

### 6.6 Primary refs
- NVIDIA, "DirectStorage on GPU decompression with GDeflate", GDC 2023. https://developer.nvidia.com/blog/gpu-decompression-directstorage/
- Microsoft, "DirectStorage 1.2 release notes", 2023. https://devblogs.microsoft.com/directx/directstorage-1-2-available-now/
- Sebastian Aaltonen, "Optimizing Tick Performance: Virtual Texturing and Streaming", Twitter/X threads + talks 2020-2024. https://twitter.com/SebAaltonen (selected in meshoptimizer docs https://github.com/zeux/meshoptimizer)
- Andrew Lauritzen, "GPU-Driven Rendering and Streaming in UE5", SIGGRAPH 2021. https://advances.realtimerendering.com/s2021/index.html
- Sony, "The Road to the PlayStation 5" (Cerny), 2020 presentation. https://www.youtube.com/watch?v=ph8LyNIT9sg

---

## 7. Large pages + HugeTLB

### 7.1 Why

Default page = 4 KB on x86, 16 KB on Apple Silicon. A TLB covers ~1024 entries → ~4 MB (x86) / ~16 MB (M-series) of hot memory without misses. Modern games have working sets of 100s of MB; TLB thrashing is a measurable hit (easily 5-15% across data-heavy code).

Large pages:
- x86: **2 MB** (normal large) or **1 GB** (gigantic).
- Apple Silicon: **16 KB** is already large for small configurations; **2 MB** as a hint.
- ARM server: **2 MB**, **32 MB**, **1 GB**.

One 2-MB page entry in TLB covers 512× more RAM than a 4-KB entry.

### 7.2 Setup cost

- **Linux explicit**: `madvise(addr, len, MADV_HUGEPAGE)` requests Transparent Huge Pages (THP). Opportunistic — kernel decides.
- **Linux hugetlbfs**: reserve N hugepages at boot (`hugepages=512` kernel param), `mmap(... MAP_HUGETLB ...)`. Guaranteed but requires sysadmin.
- **Windows**: `SE_LOCK_MEMORY_PRIVILEGE` + `VirtualAlloc(..., MEM_LARGE_PAGES)`. User account needs "Lock pages in memory" privilege. Pages are pinned.
- **macOS**: `VM_FLAGS_SUPERPAGE_SIZE_2MB` with `mach_vm_allocate`. Works with caveats.

### 7.3 Typical impact

- Pointer-chasing workloads (scene graph walks, BVH traversal): **5-15% uplift** on large scenes.
- Streaming large buffers (textures, geometry): **marginal** (0-3%), since the iteration is sequential and hw prefetchers help.
- Hot inner loops with random access over big tables (hash maps, handle pools): **biggest wins**, 10-20%.

### 7.4 When to use

- Arena backing storage: if frame-arena is ≥ 8 MB, back it with 2-MB pages. Free.
- Pool backing: for pools ≥ 16 MB.
- Streaming rings: yes, if pinned.
- Small scratch: don't bother, waste of reserve.

### 7.5 Pitfalls

- **Fragmentation of huge page pool** on Linux: after a few hours of heavy allocation, the kernel may fail to allocate new 2-MB pages. Solution: reserve at boot.
- **Lock memory privilege** on Windows is an admin action; shipping games often include an installer step to grant it.
- **Debugging friction**: `/proc/self/smaps` tells you whether THP actually materialized; "transparent" is misleading, you need to verify.

### 7.6 Primary refs
- Linux kernel documentation, "Transparent Hugepage Support". https://www.kernel.org/doc/html/latest/admin-guide/mm/transhuge.html
- Linux kernel documentation, "HugeTLB Pages". https://www.kernel.org/doc/html/latest/admin-guide/mm/hugetlbpage.html
- Microsoft, "Large-Page Support" MSDN. https://learn.microsoft.com/en-us/windows/win32/memory/large-page-support
- Wenisch et al., "A Large-Pages Analysis on Modern Server CPUs", ISCA research context 2020.
- Aaron Cao, "How huge pages save TLB misses in game engines" — informal but cited, 2021. https://engineering.fb.com/2014/06/19/ios/2mb-pages/

---

## 8. Virtual memory tricks

The POSIX / Win32 virtual memory system gives games superpowers that people forget exist. Key primitive: **reserve** (no physical backing, just address-space reservation) vs **commit** (pages get physical RAM).

### 8.1 Reserve-then-commit pattern

```cpp
void* huge = VirtualAlloc(nullptr, 1TB, MEM_RESERVE, PAGE_READWRITE);
// nothing is allocated yet; 1 TB of address space is just "ours"
VirtualAlloc(huge, 2MB, MEM_COMMIT, PAGE_READWRITE);
// now 2 MB of physical RAM is backing the first 2 MB of the reserved region
```

POSIX equivalent:
```cpp
void* huge = mmap(nullptr, 1TB, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE, -1, 0);
mprotect(huge, 2MB, PROT_READ|PROT_WRITE);   // commit on first touch (overcommit)
```

### 8.2 Use cases in games

- **Arenas that never move**. Reserve 1 GB, commit as you grow. No `realloc`, no pointer invalidation. Vectors that never reallocate are a performance superpower.
- **SparseArray / dense handle tables**. Reserve enough address space to index every possible handle, commit pages only where data lives. Classic Windows server pattern (Mark Russinovich has written about this for SQL Server), applied in UE5's sparse arrays, Flecs sparse sets, Our Machinery's "The Truth" database.
- **Infinite streaming worlds**. A region grid of 256×256 cells, each cell a 64 MB reserved block; commit/decommit cells as player roams. No realloc, cell pointers are stable for the lifetime of the world.
- **Guard pages**. Reserve a page with `PAGE_NOACCESS` at the end of each stack or arena; SEH / signal handler catches overflow before corruption.

### 8.3 Limits

- 64-bit address space is 48-bit effective on most CPUs (256 TB user). You can reserve TBs without cost — reservations cost only kernel bookkeeping, not RAM.
- Commit is limited by physical RAM + swap. On consoles (no swap), commit is essentially "give me RAM now" — so over-commitment behaviour differs from PC.
- `mmap` on some Linuxes requires `vm.overcommit_memory` tuning for multi-GB reservations.

### 8.4 Guard-pages for stacks

Classic pattern:
```
| guard (PROT_NONE) | stack (PROT_READ|WRITE) | guard (PROT_NONE) |
```
Stack overflow writes to guard → SIGSEGV → handler dumps backtrace. Cheap, detects 100% of stack overflows.

### 8.5 Primary refs
- Mark Russinovich, David Solomon, *Windows Internals 7th ed.*, chapter 5 "Memory Management", Microsoft Press 2017. https://learn.microsoft.com/en-us/sysinternals/resources/windows-internals
- Ulrich Drepper, "What Every Programmer Should Know About Memory", Red Hat 2007, §§2-3. https://people.freebsd.org/~lstewart/articles/cpumemory.pdf
- Casey Muratori, "The Thirty Million Line Problem", 2015 — contains the "reserve 1 TB" framing. https://caseymuratori.com/blog_0015
- Niall Douglas, "LLFIO and Reserved Address Space", CppCon 2019 talk.

---

## 9. Stack allocation + alloca

### 9.1 `alloca` / VLAs

`alloca(n)` bumps the stack pointer by n bytes, returns pointer, auto-freed at function return. Nearly free (one add), but:
- **Not portable** in the C++ standard. MSVC, gcc, clang all support it; but size is limited to remaining stack (usually 1 MB default, 8 MB for main).
- **Dangerous with exceptions**. If the function unwinds before the stack frame is torn down, the memory can leak or confuse tooling.
- **Not inlined consistently** — the compiler may or may not honor it across optimization levels.

VLAs (`int arr[n];` with runtime-n) are C99, banned in C++ standard, but supported as extension. Same tradeoffs.

### 9.2 When to use

- Small temporary arrays of known max size: "<=256 elements, use stack; else fallback to arena".
- Inner-loop scratch that fits in L1 (≤ 32 KB).

### 9.3 When not to

- Anywhere an exception can be thrown.
- Variable-size where n could be >10 KB.
- Hot paths with deep call stacks — eating stack here can cause overflow elsewhere.

### 9.4 ALZE note
Project is `-fno-exceptions`, so exception-unwind concern is moot. Still, prefer a **small-buffer-optimized arena**: stack-allocated 4 KB buffer, falls back to heap if n > 4 KB. Folly `SmallVector`, Abseil `InlinedVector`, LLVM `SmallVector` all implement this.

### 9.5 Primary refs
- ISO/IEC 14882 (C++17), [expr.delete] — no alloca, use VLA extension or manual.
- gcc docs, "Other Built-in Functions: `__builtin_alloca`". https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
- LLVM `SmallVector` source. https://llvm.org/doxygen/classllvm_1_1SmallVector.html

---

## 10. Memory debugging

### 10.1 Tooling matrix

| Tool | Overhead | Catches | Use case |
|---|---|---|---|
| Valgrind Memcheck | 20-50× slowdown | UAF, leaks, uninitialized reads | offline unit tests only |
| ASan (clang/gcc) | 2-3× slowdown | UAF, heap overflow, stack overflow | CI + dev builds |
| UBSan | 1.1-1.5× | UB (signed overflow, misaligned) | CI |
| MSan | 3× | uninitialized reads | specialized CI |
| TSan | 5-10× | data races | threading CI |
| heaptrack | 1.2× | alloc counts + sizes + backtraces | profiling sessions |
| Windows Application Verifier | minor | UAF, handle leaks | QA builds |
| custom tracking malloc | 1.05× | allocation budgets per subsystem | ship builds with `-DTRACK_ALLOCS` |

### 10.2 Engine-internal tracking

Every AAA engine builds its own:
- **UE5 FMemory**: wraps every alloc; per-thread stats; `stat memory` shows per-subsystem. Switchable backend: ANSI / TBB / Binned2 / Binned3 / Mimalloc.
- **UE5 MallocBinned2 / Binned3**: UE's own slab-based allocator, designed to outperform generic mallocs on game workloads. Binned3 (UE5.2+) added smaller size classes, lower fragmentation.
- **Frostbite FbMemory**: partition per subsystem + per-arena counts; a subsystem exceeding its budget fails the build.
- **Naughty Dog**: alloc tracking tied to asset IDs; every byte knows its "owner".

### 10.3 Per-frame allocation budgets

Tag each allocation with a subsystem ID and a category; aggregate per-frame counts:
- Target in ship code: **< 50 allocs/frame**.
- Target in dev builds: < 5000 allocs/frame (logs, hot reload, etc.).
- A frame with > 100 allocs in ship code triggers a warning and logs the backtrace of the top-5.

### 10.4 Bisecting allocations

When you have "frame 47 has a leak of 200 KB that was not there on frame 46", tools like UE5 `MemoryProfiler2` and Frostbite's allocation diffing let you snapshot between frames, diff by callstack, and pinpoint the offending line. Gregory's book has a chapter on this.

### 10.5 Primary refs
- Google Sanitizers docs. https://github.com/google/sanitizers
- Julian Seward, "Valgrind" user manual. https://valgrind.org/docs/manual/manual.html
- Epic Games, UE5 `FMalloc` / `FMallocBinned2` — `Runtime/Core/Private/HAL/MallocBinned2.cpp` in UE source; docs https://dev.epicgames.com/documentation/en-us/unreal-engine/memory-management-in-unreal-engine
- Jason Gregory, *Game Engine Architecture* 3rd ed., §5 "Engineering for Data: Memory". https://www.gameenginebook.com/
- Milian Wolff, "heaptrack — a better heap memory profiler for Linux". https://github.com/KDE/heaptrack

---

## 11. Case studies

### 11.1 Frostbite (DICE / EA)

- Engine partitions RAM at init into ~20 named arenas: *AnimationPool*, *PhysicsPool*, *AudioStreamBuffer*, *GPUUploadRing*, *ScratchFrame*, *ScratchLevel*, etc.
- Each subsystem only touches its arena; exceeding budget is a build-break.
- Per-frame arena per worker thread (~32 MB) — `FrameAllocator`.
- Public talks: Johan Andersson DICE, "Parallel Graphics in Frostbite" (GDC 2009); Charles de Rousiers, "Moving Frostbite to PBR" (SIGGRAPH 2014) mentions memory budgets in passing.

### 11.2 Naughty Dog

- Handle-first architecture: TLOU2 has very few raw pointers in gameplay code. Everything is `Handle<T>` with generation.
- Fibers share thread-local stack arenas (see sibling r5/job_systems.md).
- Asset memory fully pre-allocated and pinned by level; streaming is "swap cell A for cell B in this fixed pool".
- Christian Gyrling, "Parallelizing the Naughty Dog Engine Using Fibers", GDC 2015. https://www.gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
- Jason Gregory (Naughty Dog), *Game Engine Architecture* 3rd ed.

### 11.3 UE5

- `FMalloc` is an interface; backends are `MallocBinned2`, `MallocBinned3`, `MallocTBB`, `MallocMimalloc`, `MallocAnsi`.
- Runtime default on desktop (UE 5.3+): MallocBinned3 with optional Mimalloc override via `-mallocmimalloc`.
- Per-thread frame allocator: `FMemStack` (linear arena), 64 KB chunks, auto-freed at frame end.
- `TSparseArray` uses reserve-and-commit for stable-pointer semantics.
- `FWeakObjectPtr` for safe external references (see §5.3).
- Docs: https://dev.epicgames.com/documentation/en-us/unreal-engine/memory-management-in-unreal-engine

### 11.4 Flecs (Nacktar / Sander Mertens)

- Sparse set is the core storage: `entity_id → table + row`.
- Each component type has its own storage array in each archetype table.
- Handles are `entity_id = (gen:32 | index:32)` packed into 64 bits.
- Allocator strategy: per-archetype arena + chunked tables (16384 entities per chunk) → pagefault-friendly.
- Docs https://www.flecs.dev/ and blog https://ajmmertens.medium.com/

### 11.5 id Tech 7 / 8

- Single megachunk of RAM allocated at startup (hundreds of MB-to-GB).
- Per-system linear allocators sliced from the megachunk.
- Doom Eternal famously has < 200 allocs per frame in ship code; 60 fps baseline on PS4 base.
- Axel Gneiting (id), "The Rendering of Doom Eternal", Digital Dragons 2020. https://advances.realtimerendering.com/ (talk videos)

---

## 12. Allocator comparison table

| Allocator | Alloc speed | Free speed | Fragmentation | Debug difficulty | Use case |
|---|---|---|---|---|---|
| malloc / new | 20-80 ns typical, ms worst | 20-80 ns typical | HIGH long-term | HIGH (UAF time bombs) | avoid in hot paths; fine for init |
| Linear arena | **2-4 ns** | **O(1) bulk reset** | NONE (by reset) | LOW (no free = no UAF) | per-frame scratch, per-task scratch |
| Stack allocator | 2-4 ns | O(1) marker rewind | NONE | LOW | nested scope scratch |
| Pool (fixed-size) | 5-15 ns | 5-15 ns | NONE (equal slots) | LOW (guard + poison) | per-type component storage |
| Slab (multi-bucket) | 10-30 ns | 10-30 ns | LOW (bucketed) | MEDIUM | general-purpose allocator |
| Handle system | 5-15 ns (pool) + deref | 5-15 ns + gen++ | NONE | **LOWEST (UAF detected)** | external references, ECS entities |
| Streaming ring | O(1) head bump | O(1) fence-gated tail | NONE | MEDIUM (timeline bugs subtle) | async IO, uploads, large assets |
| VirtualAlloc reserve | µs-ish (page setup) | µs-ish decommit | NONE | MEDIUM | never-moving big tables, sparse |
| alloca / VLA | **1 ns** (stack bump) | **0 ns** (ret) | NONE | LOW (but stack-overflow risk) | small fixed-bound temps |
| mimalloc / tcmalloc | 15-40 ns | 15-40 ns | LOW | MEDIUM | libc drop-in replacement |

---

## 13. ALZE applicability

ALZE is C++17, no-RTTI, no-exceptions, single-threaded-ish with ambitions (job system is in r5 sibling). Target: 60 fps on mid-range laptop, scenes of 1-5k entities, 5-20 MB textures.

### 13.1 v1 (ship-this-month, minimum viable)

Goal: no malloc in hot paths, handles for safety, predictable frame.

- **Per-frame scratch arena**: 32 MB, reset at `World::frame_end()`. Used by: command recording, culling lists, ImGui strings, JSON parsing at dev-time, debug draw.
- **Per-type pools for components**:
  - `Pool<Transform>` — 4096 slots, ~192 KB.
  - `Pool<MeshRenderer>` — 2048 slots, ~256 KB.
  - `Pool<RigidBody>` — 1024 slots, ~1 MB.
  - `Pool<AudioVoice>` — 128 slots, ~8 KB.
  - `Pool<Entity>` — 16384 slots, 64 KB metadata.
- **Handle<T, Gen32>**: 64-bit packed `(index:32 | gen:32)`. Every external reference is a handle. Debug builds assert gen on deref.
- **Global allocator**: leave default libc malloc; replace with mimalloc if measurements show need (trivial swap).
- **No custom slab in v1** — not worth the engineering.

Expected wins: 99.9% of frame allocation traffic becomes O(1) arena bumps; GC hitches disappear; UAFs become compile-time-like via handles.

### 13.2 v2 (add streaming)

When ALZE grows beyond "assets fit in RAM at load".

- **Streaming ring**: 64 MB pinned system RAM, producer = IO thread (`std::async` / `io_uring`), consumer = upload queue to GL.
- **VRAM residency pool**: slab over GL `glBufferStorage` mapped persistent-coherent buffer (GL 4.4+). Or sub-alloc textures from a big 2D array texture if binds are a concern.
- **Per-asset handle**: `AssetHandle` (64-bit, distinct namespace from `Handle<T>`). Level streaming swaps concrete data under stable handle.
- **Add huge pages** on arenas ≥ 16 MB (THP madvise on Linux, MEM_LARGE_PAGES on Windows).

### 13.3 v3 (infinite-ish worlds)

If ALZE ever targets persistent open worlds.

- **Reserved-then-committed region grid**: reserve 64 GB address space; commit 64-256 MB per loaded cell. Cells are stable-pointer arenas.
- **Sparse handle tables** reserved at 1 TB, committed as entities populate slots.
- **Decompression pipeline** reading compressed mesh/texture deltas directly into streaming ring (DirectStorage-like on Windows; fallback CPU decompress on other platforms).
- **Per-level arena snapshots** for save/load — serialize arena contents as one blob.

---

## 14. Concrete v1 recommendation

**Do this, now, for ALZE:**

1. **One global `FrameArena` of 32 MB** (reserve 64 MB, commit 32 MB initial, grow in 4-MB steps). Reset at end-of-frame. Used via a thin `FrameAlloc(size, align)` function and RAII `FrameMarker` for nested scopes.
2. **One `Pool<T>` per component type**, sized conservatively (powers of two, at least 2× expected peak). Each component's free-list is a u32 intrusive list in the freed slot.
3. **`Handle<T, Gen32>` 64-bit** as the unit of external reference. Every subsystem hands out handles; never exposes raw pointers. `handle.get(World&)` returns `T*` after gen check (nullptr if stale in release, assert in debug).
4. **No custom slab, no huge pages, no streaming allocator** — not worth the engineering in v1.
5. **No `new` / `delete` in any file under `src/core/` hot-loops.** Lint rule: grep for `new ` / `operator new` in PR CI; fail if found outside approved init code paths.
6. **mimalloc as the libc malloc replacement** once the hot paths are migrated — trivial link-time swap, free 10-20% on the non-hot allocs.
7. **ASan in all dev builds.** Guard and poison on pool slots in debug. Per-frame alloc counter logged (target < 100 in debug, < 10 in ship).

### Honest note

Memory is the least glamorous and highest ROI subsystem of a game engine. Frostbite, UE5, and Naughty Dog all spend disproportionate engineering energy on it not because it's fashionable but because **every other optimization is fighting a ceiling imposed by the allocator**. A cache-friendly ECS iterator that pulls from a fragmented heap is still fragmented-heap-bound. A beautiful render graph that triggers `malloc` mid-frame is still a stutter factory. Get allocators right first, and everything else becomes easier to measure and optimize.

A rough rule: **if you can't answer "how many allocations did frame 1000 make?" in < 5 minutes**, your allocator architecture is not good enough yet. The v1 above gets you to an answer inside an hour, for free.

---

## 15. References — consolidated

Author / year / venue / primary URL (archive fallback where flaky).

- Bonwick, Jeff. 1994. "The Slab Allocator: An Object-Caching Kernel Memory Allocator". USENIX Summer Tech 1994. https://www.usenix.org/legacy/publications/library/proceedings/bos94/full_papers/bonwick.pdf
- Evans, Jason. 2006. "A Scalable Concurrent malloc(3) Implementation for FreeBSD". BSDCan 2006. https://www.bsdcan.org/2006/papers/jemalloc.pdf
- Leijen, Daan; Zorn, Benno; de Moura, Leonardo. 2019. "Mimalloc: Free List Sharding in Action". MSR-TR-2019-18. https://www.microsoft.com/en-us/research/publication/mimalloc-free-list-sharding-in-action/
- Alexandrescu, Andrei. 2015. "std::allocator is to allocation what std::vector is to vexation". CppCon 2015. https://www.youtube.com/watch?v=LIb3L4vKZ7U
- Frykholm, Niklas. 2010-2014. Bitsquid blog posts. https://bitsquid.blogspot.com/ (arch https://web.archive.org/web/2024*/bitsquid.blogspot.com)
  - "Managing Data Relationships" 2010-12.
  - "Custom Memory Allocation in C" 2010-09.
  - "Managing Decoupling Part 4: The ID Lookup Table" 2011-09.
  - "A Handle-Based Memory Allocator for Small Objects" 2012-08.
  - "Building a Data-Oriented Entity System (4-part)" 2014.
- Mertens, Sander. 2019-2024. Flecs blog, "Building an ECS" series. https://ajmmertens.medium.com/
- Gregory, Jason. 2018. *Game Engine Architecture 3rd ed*. CRC Press. §5 Memory, §6 Allocators. https://www.gameenginebook.com/
- Gyrling, Christian. 2015. "Parallelizing the Naughty Dog Engine Using Fibers". GDC 2015. https://www.gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
- Andersson, Johan. 2009. "Parallel Graphics in Frostbite — Current and Future". GDC 2009. https://www.ea.com/frostbite/news (archive https://web.archive.org/web/2020*/publications.dice.se)
- Epic Games. UE5 documentation. "Memory Management in Unreal Engine". https://dev.epicgames.com/documentation/en-us/unreal-engine/memory-management-in-unreal-engine
- Epic Games. UE5 source, `FWeakObjectPtr`, `FMallocBinned2`, `FMemStack`. https://github.com/EpicGames/UnrealEngine (auth required).
- Ghemawat, Sanjay; Menage, Paul. 2005+. TCMalloc docs. https://google.github.io/tcmalloc/overview.html
- Aaltonen, Sebastian. 2020-2024. Twitter threads + meshoptimizer notes on tiled memory, async IO, frame allocators. https://twitter.com/SebAaltonen ; https://github.com/zeux/meshoptimizer
- Kapoulkine, Arseny. 2018-2024. "The Performance of RAII" + meshoptimizer blog series. https://zeux.io/
- Intel Developer Zone. 2018. "Handle-based Data Structures for ECS". https://www.intel.com/content/www/us/en/developer/articles/
- Muratori, Casey. 2015. "The Thirty Million Line Problem". https://caseymuratori.com/blog_0015
- Linux kernel docs. "Transparent Hugepage Support" + "HugeTLB Pages". https://www.kernel.org/doc/html/latest/admin-guide/mm/
- Microsoft. "Large-Page Support", "VirtualAlloc". MSDN. https://learn.microsoft.com/en-us/windows/win32/memory/
- Russinovich, Mark; Solomon, David. 2017. *Windows Internals 7th ed*. Microsoft Press. Ch 5 "Memory Management".
- Drepper, Ulrich. 2007. "What Every Programmer Should Know About Memory". Red Hat. https://people.freebsd.org/~lstewart/articles/cpumemory.pdf
- NVIDIA. 2023. "GPU Decompression with DirectStorage / GDeflate". https://developer.nvidia.com/blog/gpu-decompression-directstorage/
- Microsoft DirectX team. 2023. DirectStorage 1.2 blog post. https://devblogs.microsoft.com/directx/
- Cerny, Mark. 2020. "The Road to the PlayStation 5". https://www.youtube.com/watch?v=ph8LyNIT9sg
- Google Sanitizers (ASan, UBSan, MSan, TSan). https://github.com/google/sanitizers
- Wolff, Milian. 2014-2024. heaptrack. https://github.com/KDE/heaptrack
- Gneiting, Axel (id Software). 2020. "The Rendering of Doom Eternal". Digital Dragons. https://advances.realtimerendering.com/
- Vyukov, Dmitry. 2010s. 1024cores.net — lock-free queues / freelists. http://www.1024cores.net/
- Fleury, Ryan. 2023. "Untangling Lifetimes: The Arena Allocator". https://www.rfleury.com/p/untangling-lifetimes-the-arena-allocator
- Gaul, Randy. 2015. "Linear Allocator Implementation". https://www.randygaul.net/2015/08/01/linear-allocator/

---

## 16. Cross-refs to other R5 agents

- `job_systems.md` — per-worker frame arenas; lock-free pools; fiber-stack allocators.
- `animation.md` — per-pose arena for skinning; pose cache pool.
- `audio.md` — AudioVoice pool; streaming ring for music and dialogue; decompression into pinned buffers.
- `networking.md` — packet pool; rollback snapshot arenas; deterministic-build allocation tracking.
- `editor_architecture.md` — undo/redo snapshot arena; hot-reload object pool swap.
- `dcc_asset_pipeline.md` — import-time arena; DDC blob storage via reserve-then-commit.

Memory choices made here are the soil every other subsystem grows in. Get them right.
