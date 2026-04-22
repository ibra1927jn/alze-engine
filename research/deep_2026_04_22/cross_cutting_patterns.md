# Patrones Cross-Cutting Modernos en Motores (2023-2030)

**Research profundo — Round 3 — 2026-04-22**
**Target**: `/root/repos/alze-engine` (C++17, no-RTTI, no-exceptions)
**Alcance**: 15 áreas cross-cutting — la capa invisible que separa un engine profesional de un hobby.

> La hipótesis rectora: a igualdad de features visibles (renderer, físicas, gameplay), lo que decide si un engine cruza el umbral de "hobby → profesional" no son más features, son los *patrones transversales*: allocator discipline, job system, determinismo, hot-reload, telemetría, testing. Este documento cataloga el state of the art 2023-2030 y prioriza para alze-engine.

---

## Tabla de contenidos

1. [Data-Oriented Design — evolución 2014→2025](#1-data-oriented-design--evolución-20142025)
2. [ECS architecture — archetype vs sparse-set, relaciones, determinismo](#2-ecs-architecture)
3. [Job system / task scheduling — fibers, coroutines, senders](#3-job-system--task-scheduling)
4. [Memory allocators engine-class — arena, pool, TLSF, huge pages](#4-memory-allocators-engine-class)
5. [Streaming architecture — bindless, sparse, SFS, DirectStorage](#5-streaming-architecture)
6. [Hot reload / live coding — C++, shaders, assets](#6-hot-reload--live-coding)
7. [Determinism patterns — fixed timestep, IEEE 754, RNG](#7-determinism-patterns)
8. [Rollback netcode — GGPO legacy, fighting games 2021-2026](#8-rollback-netcode)
9. [Editor architecture — undo/redo, PIE, node graphs](#9-editor-architecture)
10. [Reflection en C++17/20/23/26](#10-reflection-en-c17202326)
11. [Scripting patterns — Lua, Angelscript, Wren, Mono, WASM](#11-scripting-patterns)
12. [Profiling infrastructure — Tracy, GPU markers](#12-profiling-infrastructure)
13. [Crash reporting / telemetry — Crashpad, Aftermath](#13-crash-reporting--telemetry)
14. [Testing infrastructure — fuzz, property-based, perf CI](#14-testing-infrastructure)
15. [C++23/26 features — expected, mdspan, execution, reflection](#15-c2326-features)
16. [Ranking urgencia para alze-engine 2026](#16-ranking-urgencia-para-alze-engine-2026)

---

## 1. Data-Oriented Design — evolución 2014→2025

### 1.1 Línea genealógica

Cuatro talks canónicos marcan la evolución del pensamiento DoD en 11 años:

| Año | Autor | Talk | Aporte clave |
|-----|-------|------|--------------|
| 2014 | Mike Acton (Insomniac → Unity → SEED) | *Data-Oriented Design and C++* — CppCon 2014 | Fundacional. "El propósito de todo programa es transformar datos". 3 grandes mentiras: software is a platform, code designed around world model, code is more important than data. |
| 2018 | Stoyan Nikolov (Coherent Labs) | *OOP Is Dead, Long Live Data-oriented Design* — CppCon 2018 | Caso de estudio Hummingbird vs Chromium. Browser UI en C++ DoD, "multiple times faster" que Blink OOP. Slides públicas. |
| 2021 | Andrew Kelley (Zig) | *A Practical Guide to Applying Data-Oriented Design* — Handmade Seattle | DoD desde Zig (no C++). Reducir tamaño de structs por encima de todo. Struct-of-indexes con `u32` ids > pointers. Encoding de variantes sin `union`/RTTI. |
| 2023 | Ollivier Roberge | *Can C++ Data-oriented-design be Improved?* — CppCon 2023 | Crítica moderna: DoD en C++ sigue siendo macro soup vs. lenguajes con `#embed`, SoA nativo (Zig/Jai/Odin). |
| 2025 | Vittorio Romeo | *More Speed & Simplicity: Practical DoD in C++* — CppCon 2025 | Presenta que el problema real no es OOP vs DoD sino *mal modelo de layout*. Template SoA containers con boost.PFR. |

### 1.2 Realidades hardware 2026

**Cache lines — no asumir 64 bytes ciegamente:**

| Arquitectura | L1 cache line | L2 sharing (adjacent line prefetch) |
|--------------|---------------|--------------------------------------|
| Intel Skylake-X → Sapphire Rapids | 64 B | 128 B (pair fetched together) |
| AMD Zen 4/5 | 64 B | 64 B (no adjacent prefetch by default) |
| Apple M1/M2/M3 (ARM) | **128 B** | 128 B |
| IBM POWER9/POWER10 | **128 B** | 128 B |
| NVIDIA Grace (ARM N2) | 64 B | — |

**Consecuencia**: `alignas(64)` NO previene false sharing en Apple Silicon ni POWER. En C++17 hay `std::hardware_destructive_interference_size` pero es *implementation-defined* y a veces miente (GCC reporta 64 en todo x86). Patrón seguro:

```cpp
#if defined(__APPLE__) || defined(__aarch64__)
  inline constexpr size_t kCacheLineSize = 128;
#else
  inline constexpr size_t kCacheLineSize = 64;
#endif
struct alignas(kCacheLineSize) WorkerLocalCounters { std::atomic<u64> hits; char pad[kCacheLineSize - sizeof(std::atomic<u64>)]; };
```

### 1.3 Medidas reales (ns/op)

De Acton GDC 2014 slides (Insomniac: R&C Into the Nexus):
- Array of structs `Particle {v3 pos; v3 vel; v3 foo; ...}` — 128 B stride — 2 cache lines por iter para leer pos+vel → 12 B útiles / 128 B = 9.4% hit.
- SoA separado (`v3 positions[N]; v3 velocities[N];`) — iter lineal → 1 cache read cada 5.33 iteraciones sobre `positions`. **Speedup 5.7×–11×** depende de working set.

De Kelley 2021 (Zig compiler self-hosted refactor):
- AST node de 120 B → 16 B via encoding de variantes en `u32` tag + side-tables. **Compile times -40%**.

### 1.4 Patrones DoD que un engine C++17 no-RTTI/no-except debe asumir

1. **SoA por default en loops calientes** (partículas, transforms, animation bones, light lists).
2. **Index-based addressing** — sustituir `T*` por `struct Handle { u32 idx; u32 gen; }` (ver §4).
3. **Encoding de variantes** — en lugar de `std::variant<A,B,C>` con RTTI, tag byte + side-tables.
4. **Zero-init hostile**: structs triviales, `memset(0)` válido; permite alloc arena sin ctor loops.
5. **Branchless where possible** — la penalización de misprediction en moderno out-of-order es 14-20 ciclos. Patrones: `select` vía `bitwise`, SIMD mask blends, sort-then-branch-less.

**Fuentes primarias §1:**
- Acton 2014 slides: https://github.com/CppCon/CppCon2014/blob/master/Presentations/Data-Oriented%20Design%20and%20C%2B%2B/
- Nikolov 2018 slides: https://github.com/CppCon/CppCon2018/blob/master/Presentations/oop_is_dead_long_live_dataoriented_design/
- Kelley 2021 Handmade Seattle: https://media.handmade-seattle.com/practical-data-oriented-design/
- `hardware_destructive_interference_size`: https://en.cppreference.com/w/cpp/thread/hardware_destructive_interference_size.html
- False sharing deep-dive: https://riyaneel.github.io/posts/cache-coherency/

---

## 2. ECS architecture

### 2.1 Archetype vs sparse-set — el debate resuelto

Un paper académico reciente (arxiv 2508.15264v1, 2025) y un estudio comparativo de la EG Digital Library (2024, Mertens + collab) zanjan el debate midiendo ambos con el MISMO engine en C++20:

| Propiedad | Archetype (Bevy/Flecs/Mass UE5/Unity DOTS) | Sparse-set (EnTT/Shipyard) |
|-----------|---------------------------------------------|-----------------------------|
| Iteración pura SoA | **Óptima** (contigua por archetype) | Buena pero indirecta vía sparse→dense |
| Add/remove component | O(k) — copia fila a nuevo archetype | **O(1)** |
| Query por 2+ componentes | Intersect archetypes — fast si pocos | Intersect sparse-sets — "pool más pequeño manda" |
| Memoria | Compacta pero fragmentada entre archetypes | Siempre 2 tablas (sparse + dense) por tipo |
| Graph edges (add/remove transitions) | Hash/precomputado (Flecs: FLECS_HI_COMPONENT_ID <256 reservado para edges) | No aplica |
| Determinismo iteración | Orden archetype-dependent; hay que imponer ordering | Orden de inserción en dense vector |

**Veredicto 2026**: La industria AAA se va a archetype (UE5 Mass, Unity DOTS, Bevy). Sparse-set (EnTT, usado en Minecraft Bedrock) sigue ganando para *gameplay scripting* donde add/remove es frecuente. Un engine moderno puede ofrecer AMBOS — EnTT mismo soporta multi-storage policy por componente.

### 2.2 Flecs relations — first-class edges

Sander Mertens (Flecs) introdujo *relationship pairs* como tipo primitivo:

```c
ecs_add_pair(world, bob, Likes, alice);   // (Likes, alice) es una columna
ecs_add_pair(world, bob, ChildOf, world_root);
ecs_query_t *q = ecs_query(world, { .expr = "(ChildOf, *)" });  // wildcard query
```

Esto evita lo que en Unity DOTS es pesadilla: modelar "parent", "ownership", "inventory owner" etc. con componentes huecos y lookup tables. En Flecs, **una pair es un id de 64 bits** `(relation<<32) | target`. Las queries wildcard `(*, alice)` o `(Likes, *)` son O(1) vía tablas indexadas por relation/target.

Primary doc: https://www.flecs.dev/flecs/md_docs_2Relationships.html — ajmmertens.medium.com/a-roadmap-to-entity-relationships-5b1d11ebb4eb

### 2.3 Static conflict detection — Bevy `Query<&mut A, &B>`

Bevy demuestra en Rust algo que C++17 no puede replicar nativamente sin gran boilerplate: **detección estática de conflictos de acceso**. El `Query<&mut Position, &Velocity>` declara `ComponentAccess { write: {Position}, read: {Velocity} }` y el scheduler paraleliza automáticamente systems cuya intersección sea `∅`.

En C++ esto exige *template tagging* + registro de accessors. Lo hace EnTT parcialmente con `registry.view<Position, Velocity>()` pero NO detecta conflictos entre sistemas concurrentes. Bevy ECS V2 PR#1525 es la referencia de diseño.

### 2.4 UE5 Mass Entity — lecciones del porting

Mass nació como crowd simulation en UE5 (City Sample) y se está generalizando. Observaciones de la comunidad (MassSample GitHub, ZG-Gary/UEMassOverview):

- **Fragments** (= components) son POD structs por convención (`USTRUCT(BlueprintType) FMassMovementFragment`).
- **Processors** iteran archetypes vía `FMassExecutionContext`. SoA implícita.
- **Traits** agrupan fragments+tags+processors para hacer prefabs configurables desde data.
- Dolor de cabeza principal: **reconciliación con UObject/AActor**. Mass tiene "representation LOD" que spawnea/desspawnea actores reales según distancia. Es frágil.

Para alze-engine, la lección es: **no intentar mezclar actor-OOP con ECS**. Escoger UNO como modelo canónico del mundo.

### 2.5 Determinismo en ECS

Problema: iteración por archetype puede variar orden si archetypes se crean en distinto orden entre runs.

Soluciones:
- **Sorted iteration** por `EntityId` dentro de archetype.
- **Stable entity id assignment** — monotonic counter, NO reutilizar slots sin generation bump.
- **Deterministic command buffer** — agrupar add/remove hasta barrier; aplicar en orden canónico.

Ver §7 para el paquete completo de determinismo.

**Fuentes §2:**
- arxiv ECS concurrency: https://arxiv.org/html/2508.15264v1
- EG DL sparse vs archetype: https://diglib.eg.org/bitstreams/766b72a4-70ae-4e8e-935b-949d589ed962/download
- Bevy ECS V2 PR: https://github.com/bevyengine/bevy/pull/1525
- Flecs Relationships: https://www.flecs.dev/flecs/md_docs_2Relationships.html
- UE5 Mass docs: https://dev.epicgames.com/documentation/en-us/unreal-engine/mass-entity-in-unreal-engine

---

## 3. Job system / task scheduling

### 3.1 Naughty Dog fibers — el canon

Christian Gyrling, GDC 2015, "Parallelizing the Naughty Dog Engine Using Fibers". Diseño que inspiró Frostbite 2.x y decenas de engines:

- **160 fibers** pre-creados (128 con stack 64KB + 32 con stack 512KB para jobs hambrientos).
- **N worker threads** (6 en PS4, típicamente core_count − 1) cada uno corriendo un fiber a la vez.
- **5 priority queues** globales (high/normal/low + 2 reservadas per-frame).
- **Counter-based sync**: job N decrementa un `atomic_counter`. `wait_counter(c, 0)` suspende fiber (lo pone en wait list, el worker recoge otro fiber). Cuando counter llega a 0, fiber se re-encola.
- **No locks en hot path** — todo lock-free MPMC queues (variante Moody-Camel). Si absolutamente necesario un lock, se tiene un mutex que suspende fiber en lugar de OS thread.

Ventaja sobre threads: context switch fiber = save/restore ~200 B stack state, sin syscall, ~50-100 ns. Ventaja sobre coroutines (C++20): stack es grande y completo (64 KB), cualquier función puede llamarse sin marcarla `co_await`-friendly.

**Código open-source referencia**: RichieSams/FiberTaskingLib (C++ port) y "Our Machinery" blog archive (ruby0x1.github.io/machinery_blog_archive).

### 3.2 Frostbite task graph — Johan Andersson 2009

Siggraph 2009 "Parallel Graphics in Frostbite". Arquitectura distinta:

- **200-300 jobs por frame** en batallefield 3.
- **Job graph explícito**: inputs/outputs declarados, dependencias como aristas.
- **"Braided parallelism"**: task-parallelism (jobs distintos en paralelo) + data-parallelism (un job paraleliza internamente sobre arrays).
- SPU jobs (PS3) debían ser stateless + DMA explícito — disciplina que se trasladó a CPU después.
- Job graph permite **visualización** (debug), **schedule ahead** (estimar critical path) y **load balancing** dinámico.

### 3.3 Comparativa: fibers / coroutines / tasks / std::execution

| Abstracción | Context switch cost | Stack | Composable | Cancelación | Madurez |
|-------------|---------------------|-------|------------|-------------|---------|
| OS thread | 1-10 μs (syscall) | 1-8 MB | Sí | Pthread cancel (frágil) | 40+ años |
| Fiber (ucontext / Win Fibers) | 50-200 ns | User-sized, típ 64 KB | Sí vía custom | Manual | Naughty Dog ship |
| C++20 coroutine | 0 ns cuando inline (heap elision) — ~50 ns si escape | Frame heap-allocated | Sí (`co_await`) | `stop_token` | Compila Clang 14+/GCC 11+, pero API raw fea |
| Intel TBB `task_group` | ~100 ns | Thread stack | `parallel_invoke`, flow_graph | Cancel group | Madura, ship en muchos CAD/simul |
| `std::execution` (P2300 C++26) | Depende del scheduler | Depende | Sender/Receiver — extremadamente | `stop_token` + async_scope | Recién adoptado junio 2024 para C++26 |
| Native tasks (custom, Frostbite-style) | ~100-500 ns | Pool | Custom | Custom | Probado en todas las consolas |

**Recomendación alze-engine 2026**: Fiber-based con 128 pre-allocated + counter sync como Naughty Dog. Razones:
1. C++17 constraint excluye `co_await` (C++20).
2. Stack completo permite llamar código third-party sin rewriting.
3. Proven at scale en consolas.
4. `boost::context` o `libaco` dan primitives portables x86/ARM/Linux/Win.

### 3.4 Work-stealing Cilk-style

Algoritmo de Blumofe-Leiserson (1999) hoy universal (TBB, Go runtime, Rust Rayon, Java ForkJoinPool, .NET Task):

- Cada worker tiene **deque privada**. Push/pop LIFO por el bottom (cache-friendly).
- Cuando queda vacío, elige otro worker random, **steal** desde el TOP de su deque (FIFO) — esto roba la tarea más vieja = mayor, mejor amortización.
- Paper "Dynamic circular work-stealing deque" (Chase-Lev, 2005) da implementación lock-free correcta.

**Detalle importante** para determinismo: el orden de steal es *no determinista*. Si el engine necesita determinismo (ver §7), el job system debe:
1. Particionar jobs por frame, no entre frames.
2. Dentro de un frame, jobs sin dependencias se ejecutan en cualquier orden PERO sus outputs se escriben en slots fijos (por entity id u otra key estable).
3. Reducción final en orden canónico.

**Fuentes §3:**
- Gyrling GDC 2015 PDF: https://media.gdcvault.com/gdc2015/presentations/Gyrling_Christian_Parallelizing_The_Naughty.pdf
- FiberTaskingLib: https://github.com/RichieSams/FiberTaskingLib
- Andersson Siggraph 2009: http://s09.idav.ucdavis.edu/talks/04-JAndersson-ParallelFrostbite-Siggraph09.pdf
- P2300 std::execution: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p3109r0.html
- Stdexec reference: https://github.com/NVIDIA/stdexec
- Cilk scheduler paper: https://www.csd.uwo.ca/~mmorenom/CS433-CS9624/Resources/Scheduling_multithreaded_computations_by_work_stealing.pdf
- Chase-Lev deque: https://dl.acm.org/doi/10.1145/1073970.1073974

---

## 4. Memory allocators engine-class

### 4.1 Taxonomía de allocators en un engine moderno

| Allocator | Uso típico | Alloc cost | Free cost | Thread-safety | Notas |
|-----------|-----------|-----------|-----------|----------------|-------|
| **Linear / bump / arena** | Per-frame scratch, temp strings | `ptr+=n` ~5 ns | Free-all al fin de frame (~50 ns) | Per-thread | Cero fragmentación. No free individual |
| **Pool (fixed-size)** | Componentes ECS, partículas, Entities | O(1), pop free-list | O(1) push free-list | Lock-free stack OK | Un pool por tipo |
| **Slab (kernel-style)** | Chunks objects de tamaño ~ clase | O(1) | O(1) | Per-CPU slabs | Útil dentro de VM del engine |
| **Buddy system** | Texture atlas, GPU heaps | O(log N) | O(log N) | Lock, o per-thread buddy | Power-of-2 waste (hasta 50%) |
| **TLSF (Two-Level Segregated Fit)** | Real-time general-purpose | **O(1)** | O(1) | Lock o per-thread | 4 B overhead/block, bounded WCET |
| **Jemalloc/mimalloc/rpmalloc** | Fallback general-purpose | ~10 ns p50, 25 ns p95 | Idem | Thread-local arenas | 2024 benchmarks: mimalloc gana p99 |
| **OS virtual reservation (VirtualAlloc/mmap)** | Streaming pools, huge buffers | μs range | μs range | N/A | Reserve 1-64 GB, commit on demand |

### 4.2 TLSF — por qué es el default "general purpose" en real-time

TLSF (Masmano-Ripoll-Crespo, 2004, UPV) da **O(1) malloc/free worst-case** con good-fit strategy, sin necesidad de free-list compactada:

- **Primer nivel**: buckets por exp2 (16, 32, 64, …, 2^N).
- **Segundo nivel**: subdivisión lineal del bucket (8 sub-buckets típicamente).
- Find free block: `ctz` en bitmap primer nivel, `ctz` en bitmap segundo nivel = 2 instrucciones. Zero searching.

Overhead: 4-8 B por bloque. Impl referencia: mattconte/tlsf (dominio público, ~600 LOC).

**Uso en producción**: WebKit JSC (JavaScript heap), Garry's Mod (source engine custom), libgfx-rs.

### 4.3 Allocator benchmark 2024-2026 (workloads tipo game)

De *Benchmarking Malloc with Doom 3* (forrestthewoods 2021) y datos 2026 (stratcraft.ai/nexusfix):

| Allocator | Doom 3 total time | p50 malloc (ns) | p99 alloc (ns) | Peak RSS | Notas |
|-----------|-------------------|------------------|-----------------|----------|-------|
| glibc (ptmalloc2) | 100% (baseline) | 35 | 580 | 820 MB | Default Linux |
| jemalloc 5.3 | 93% | 22 | 180 | 760 MB | Muy estable, usado en Redis, FB |
| mimalloc 2.1 | **88%** | 14 | 95 | **720 MB** | UE5 + Bing + Azure usan mimalloc |
| rpmalloc | 90% | 16 | 130 | 740 MB | Epic / Haiku OS — TLS arenas agresivas |
| tcmalloc | 94% | 20 | 160 | 780 MB | Google prod |

mimalloc gana consistentemente en 2024-2026 para cargas asimétricas (pool alloc, pool free pattern típico de frame-based engine).

### 4.4 Handle-based systems = anti-pointer-invalidation

Patrón universal hoy en engines AAA (Bitsquid/Stingray → Our Machinery → Wicked Engine → Flecs → Bevy):

```cpp
struct Handle {
    u32 index    : 24;   // 16M max
    u32 generation : 8;  // 256 recycles before rollover
};

template<typename T, size_t N>
class Pool {
    T      items[N];
    u8     generations[N];   // bump on destroy
    std::vector<u32> free_list;

    Handle create() {
        u32 idx = free_list.back(); free_list.pop_back();
        return { idx, generations[idx] };
    }
    T* get(Handle h) {
        if (generations[h.index] != h.generation) return nullptr;  // stale
        return &items[h.index];
    }
};
```

Ventajas vs `shared_ptr`:
- 8 B (o menos) vs 16 B shared_ptr + heap control block.
- Detecta stale access (generation mismatch) — crítico cuando un sistema destruye entity entre frames y otro mantiene ref.
- Cero fragmentación (pool fijo).
- Serializable directo — el handle es dato, no puntero.

floooh (Andre Weissflog, Sokol): "Handles are the better pointers" (https://floooh.github.io/2018/06/17/handles-vs-pointers.html) es el pitch canónico moderno.

### 4.5 Huge pages (2 MB Linux / large pages Windows)

Un engine con working set ≥ 2 GB sufre TLB misses dramáticos con páginas 4 KB (4M entries para cubrir 16 GB, cualquier TLB tiene 1-2K).

**Solución**: `madvise(addr, len, MADV_HUGEPAGE)` en Linux o `VirtualAlloc(..., MEM_LARGE_PAGES, ...)` en Windows. Medidas reportadas:

- **Evan Jones** (evanjones.ca/hugepages-are-a-good-idea.html): 2.9× speedup 2 MB vs 4 KB en benchmark de pointer-chasing. 1 GB solo +8% extra sobre 2 MB.
- **Rigtorp** (rigtorp.se/hugepages/): recomienda explícitas no transparent para evitar jitter de THP collapsing (latencia spikes μs-ms).

Caveat: interactúa mal con `MADV_DONTNEED` de jemalloc — si el bloque es menor a 2 MB, el kernel no puede liberar la página entera y el allocator ve fragmentación fantasma.

**Fuentes §4:**
- TLSF paper: https://pdfs.semanticscholar.org/31da/f60a6c47c1bf892a2c4b76e4bb7c1cf83b58.pdf
- TLSF impl: https://github.com/mattconte/tlsf
- mimalloc: https://github.com/microsoft/mimalloc — bench https://microsoft.github.io/mimalloc/bench.html
- Handles blog (floooh): https://floooh.github.io/2018/06/17/handles-vs-pointers.html
- Generational arena Rust: https://github.com/fitzgen/generational-arena
- Huge pages Rigtorp: https://rigtorp.se/hugepages/

---

## 5. Streaming architecture

### 5.1 LOD grid hierarchy — canonical approach

Patrón heredado de Nanite + DOOM Eternal + Ratchet & Clank Rift Apart:

- **Grid 3D** (típicamente 64×64×64 m celdas) indexa assets → prioridad de carga.
- Cada celda tiene 3-5 **LODs** con presupuesto de bytes.
- Sistema de **requests** (priority queue) con cost function: `priority = 1/dist² * importance * (1 - residency_ratio)`.
- **Bandwidth budget** por frame (p.ej. 200 MB/s sustained HDD, 3.5 GB/s NVMe Gen4, 7 GB/s Gen5) — no encolar más de lo drenable.

### 5.2 Bindless + sparse resources (Vulkan/D3D12)

**Bindless descriptor indexing** (Vulkan 1.2 core, D3D12 HLSL SM 6.6): en lugar de rebind sets por material, un único descriptor set gigante (65536+ entries) con todos los texturas. Shader accede por `u32 material_id → texture index`:

```hlsl
Texture2D textures[] : register(t0, space1);  // unbounded
float4 albedo = textures[material.albedo_idx].Sample(linearSampler, uv);
```

**Sparse residency** (VkSparseImage / D3D12 Tiled Resources): una textura 16K×16K virtual cuyas páginas 64 KB se commit/uncommit dinámicamente. El GPU page-fault lookup para tiles no residentes retorna sampler fallback (default mip) en lugar de crash.

Combinado con bindless, permite "world of textures" donde el shader escribe en feedback buffer qué tiles pidió, y CPU parsea el buffer para decidir qué loadear.

### 5.3 Sampler Feedback Streaming (SFS) — D3D12

Microsoft spec: https://microsoft.github.io/DirectX-Specs/d3d/SamplerFeedback.html — Intel GameTechDev sample: https://github.com/GameTechDev/SamplerFeedbackStreaming

- Modos: **MIN_MIP_MAP** (per-tile minimum mip requested) y **MIP_REGION** (per-region mip desired).
- Pipeline: GPU escribe feedback resource. CPU lee el feedback → determina tiles a stream in/out → submit DirectStorage reads → update tile mappings.
- Demo de Intel sobre escenas con 100 GB de texturas en 8 GB VRAM, sin texture pop.

UE5 Virtual Textures usa un mecanismo similar pero propietario (page-table lookup en shader + feedback via compute shader).

### 5.4 DirectStorage + RTX IO — GPU decompression

Tradicional: disk → CPU RAM → decompress en CPU (zlib, LZ4) → memcpy a VRAM. Cuellos: CPU cores y PCIe bandwidth doble-uso.

**DirectStorage 1.x (2022) + 1.2 (2023) + 1.4 (2024)**: disk → VRAM con DMA directo. La GPU corre **GDeflate** (NVIDIA-contributed, open) o **Zstd** (1.4) decompression en compute shaders.

Resultados publicados (Ratchet & Clank Rift Apart PC port):
- Load time 20s → 2s.
- Texture streaming latency reducida 70%.
- CPU cores liberados (~2-3 cores de decompression → 0).

Extensión Vulkan equivalente: `VK_NV_memory_decompression` (propuesta) y `VK_EXT_image_compression_control`.

**Fuentes §5:**
- SFS spec: https://microsoft.github.io/DirectX-Specs/d3d/SamplerFeedback.html
- GameTechDev SFS sample: https://github.com/GameTechDev/SamplerFeedbackStreaming
- RTX IO GDeflate: https://developer.nvidia.com/blog/accelerating-load-times-for-directx-games-and-apps-with-gdeflate-for-directstorage/
- Vulkan sparse binding: https://www.asawicki.info/news_1698_vulkan_sparse_binding_-_a_quick_overview
- zeux Vulkan efficient renderer: https://zeux.io/2020/02/27/writing-an-efficient-vulkan-renderer/
- Bindless Vulkan: https://dev.to/gasim/implementing-bindless-design-in-vulkan-34no

---

## 6. Hot reload / live coding

### 6.1 C++ hot reload — por qué es difícil

Barreras fundamentales:
1. **Compilación** de un .cpp pequeño, si es correcto incremental, tarda 1-5s.
2. **Linking** en full rebuild de un engine grande tarda 30-120s (por eso UE5 usa Unity files/batching).
3. **Pointer preservation**: si un objeto vivo cambia de layout (campo añadido), los pointers antiguos son inválidos.
4. **Vtable fixup**: métodos virtuales nuevos → vtable expandida → pointers a vtable vieja apuntan a slots inexistentes.

### 6.2 Mechanisms de hot reload

| Tool | Strategy | Supports | Limitations |
|------|----------|----------|-------------|
| **Runtime Compiled C++ (RCCpp)** | OSS, Molecular Matter origin. Compila a DLL, load, trampoline | Non-virtual funcs + data recreate | Requires opt-in per class via macros |
| **cr.h (fungos)** | OSS one-header. Full DLL reload, serialize state | Simple apps, C | Manual state preservation |
| **Live++** (Molecular Matter, commercial) | Machine code patching directo. First-instr jump. | RTTI, polymorphism, exceptions, lambda | Licencia €; vtable grow issues |
| **UE5 Live Coding** | Internally uses Live++ library | Non-structural changes | Cannot add new UCLASS. Vtable grow fails |
| **JetBrains Rider + Live++** | Integrated workflow Sept 2024 | As Live++ | As Live++ |

**Detalle técnico Live++/UE Live Coding** (forum.unrealengine.com):
- Compila .cpp cambiados a "patch DLL" separada.
- Inyecta en el proceso vivo vía `LoadLibrary`/`dlopen`.
- Reescribe el **primer instruction byte** de cada función old con `jmp patch_new_func` (trampoline de 5 bytes x86_64 short o 14 bytes `mov rax, addr; jmp rax`).
- Antiguas call-sites siguen apuntando al addr viejo, pero la ejecución se desvía al nuevo.
- **Vtable grow NO funciona**: si clase tiene virtual nuevo, sizeof(vtable) cambia, instancias en heap tienen vtable pointer al vtable viejo (de tamaño N), nuevo código espera N+1.

### 6.3 Shader hot reload — caso fácil

Arquitectura típica (funciona en Godot, UE, Unity, Wicked, Defold):
1. File watcher sobre `shaders/*.hlsl` o `*.glsl`.
2. On change: recompile via `dxc`/`glslangValidator` → SPIR-V/DXBC.
3. Rebuild PSO (Graphics Pipeline State Object en D3D12 / Vulkan).
4. Swap PSO pointer en el material. Siguiente frame usa shader nuevo.

Catch: rebuild de PSO puede tomar 50-500 ms → stutter visible. Mitigación: background thread compile, atomic swap cuando ready.

### 6.4 Asset hot reload

Patrón content-hash based es superior a mtime-based:

- mtime: falso positivo por `touch`, falso negativo si filesystem tiene resolución 1s y cambias dos veces.
- **Content hash (BLAKE3)**: compute en background, solo reload si cambia. Permite dedup también.

Godot, Unity AssetDatabase, UE DDC usan content-hash + GUID manifest.

### 6.5 State preservation

El problema real. Estrategias:

1. **No-op state (stateless systems)**: recarga sin pérdida. Ideal pero raro.
2. **Serialize-reload-deserialize**: escribir a memoria, recargar código, leer y reconstruir. Funciona si layout compatible.
3. **Data migration**: registrar "old layout → new layout" migration fn. Cansino a escala.
4. **Handle-based pool + reload code, keep data**: si tus objetos viven en un pool con POD fragments y el código vive en systems reloadables, los datos sobreviven naturalmente.

La opción 4 es por qué ECS + hot reload juegan bien juntos: systems (código) cambian sin tocar components (datos).

**Fuentes §6:**
- Live++: https://liveplusplus.tech/
- UE5 Live Coding docs: https://dev.epicgames.com/documentation/unreal-engine/using-live-coding-to-recompile-unreal-engine-applications-at-runtime
- RCCpp: https://github.com/RuntimeCompiledCPlusPlus/RuntimeCompiledCPlusPlus
- cr.h (fungos): https://github.com/fungos/cr
- ezEngine hot reload docs: https://ezengine.net/pages/docs/custom-code/cpp/cpp-code-reload.html

---

## 7. Determinism patterns

### 7.1 Fixed timestep — el canon Gaffer

Glenn Fiedler, "Fix Your Timestep!" (https://gafferongames.com/post/fix_your_timestep/, 2004). Patrón:

```cpp
const double dt = 1.0 / 60.0;  // sim rate
double accumulator = 0.0;
double t = 0.0;
double currentTime = hires_now();

while (running) {
    double newTime = hires_now();
    double frameTime = newTime - currentTime;
    if (frameTime > 0.25) frameTime = 0.25;  // spiral-of-death clamp
    currentTime = newTime;
    accumulator += frameTime;

    while (accumulator >= dt) {
        integrate(state, t, dt);
        t += dt;
        accumulator -= dt;
    }

    const double alpha = accumulator / dt;
    render(lerp(previousState, currentState, alpha));
}
```

Claves:
1. Sim siempre en `dt` fijo (evita explosiones spring/integration).
2. Render interpola entre estados anterior y actual (sin stutter).
3. Clamp de `frameTime` evita death-spiral cuando un frame tardó 2s.

### 7.2 IEEE 754 cross-platform determinism

Referencia: Bruce Dawson "Floating-Point Determinism" (https://randomascii.wordpress.com/2013/07/16/floating-point-determinism/) + Fiedler (https://gafferongames.com/post/floating_point_determinism/).

Problemas:
- **x87 80-bit intermediate** vs SSE 32/64-bit — una multiplicación en x87 puede dar resultado distinto a SSE.
- **FMA (Fused Multiply-Add)**: `a*b+c` se compila a `vfmadd` que es más preciso que `(a*b)+c` — pero solo si el target soporta FMA3. Compilar con `-mfma` en AVX2+ diverge vs SSE2.
- **`-ffast-math` / `/fp:fast`**: rompe IEEE 754. Nunca usar en sim determinista.
- **Transcendentals** (`sin`, `cos`, `exp`): libm varía entre Linux glibc, Windows msvcrt, Apple libc. Solución: tabla propia, o Sleef (vectorized libm con resultados garantizados).
- **Denormals**: FTZ/DAZ bit MXCSR cambia comportamiento. Fijar explícitamente al entrar al sim.

Recomendación alze-engine:
```cpp
// Al inicio del sim loop (por thread que correrá sim):
#include <xmmintrin.h>
#include <pmmintrin.h>
_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
// MSVC: /fp:precise  + /arch:SSE2 (no AVX/FMA)
// GCC/Clang: -msse2 -mno-fma -fno-fast-math -ffp-contract=off
```

Sherry Ignatchenko's CppCon 2024 "Cross-Platform Floating-Point Determinism Out of the Box" propone un enfoque basado en "softfloat" para determinismo absoluto entre ARM/x86 — costo 5-20× en performance.

### 7.3 Deterministic RNG

**mt19937** es DEFAULT en `<random>` — funciona pero seeding portable es un dolor (ver PCG seeding surprises post). Recomendación 2026:

| Gen | Speed (cycles/output) | State size | Period | Notas |
|-----|----------------------|-------------|--------|-------|
| mt19937 | ~6-8 | 2.5 KB | 2^19937 | Default, excesivo estado |
| **xoshiro256++** | **~2-3** | 32 B | 2^256 | Recomendado por Vigna. Pasa TestU01 BigCrush |
| xoroshiro128+ | ~2 | 16 B | 2^128 | Más rápido, menor calidad en bit bajo |
| **PCG32** | ~2-3 | 16 B | 2^64 stream × 2^63 streams | Recomendado por O'Neill. Jump-ahead O(log n) |
| splitmix64 | ~2 | 8 B | 2^64 | Solo para seeding otros RNGs |

Pattern: **un RNG por sistema** (physics, AI, VFX) + `splitmix64` para derive del master seed. Estado serializable con save/load.

### 7.4 Enum stability + sorted iteration

Dos fuentes sutiles de no-determinismo:

1. **Enum values** cambiando entre builds (agregas un enumerator en medio → desplaza el resto → save files antiguos inválidos). Regla: append-only + `_Count` sentinel al final. Considera explicit values `{FOO = 1, BAR = 2, BAZ = 3}`.

2. **Iteración de `unordered_map`** — orden hash-dependent, varía con rehash, compiler, version libstdc++. Para determinismo: ordenar claves antes de iterar, o usar `flat_map` (sorted vector) / `btree_map`.

### 7.5 Por qué importa

- **Replays**: grabar solo inputs, reproducir simulación idéntica.
- **Rollback netcode** (§8): obligatorio — si los dos peers no simulan igual, desync.
- **Testing**: property-based y golden-image tests requieren determinismo.
- **Cheat resistance**: servidores validan client simulation replaying desde inputs.
- **Regression triage**: bug reproducible bit-a-bit en todas las máquinas de QA.

**Fuentes §7:**
- Fix Your Timestep: https://gafferongames.com/post/fix_your_timestep/
- Floating Point Determinism (Fiedler): https://gafferongames.com/post/floating_point_determinism/
- Floating Point Determinism (Dawson): https://randomascii.wordpress.com/2013/07/16/floating-point-determinism/
- Ignatchenko CppCon 2024: https://www.youtube.com/watch?v=7MatbTHGG6Q
- PRNG shootout (Vigna): https://prng.di.unimi.it/
- PCG: https://pcg-random.org/
- xoshiro C++: https://github.com/Reputeless/Xoshiro-cpp

---

## 8. Rollback netcode

### 8.1 GGPO — arquitectura canónica (Tony Cannon, 2006)

**Invariante**: la simulación es determinista dado el input. Entonces con inputs propios + inputs predichos del peer, simulo hacia adelante. Cuando llegan los inputs reales, si difieren del predicho, **rewind N frames + re-simulate**.

```
Frame 10 — local input + predicted remote. Save state snapshot.
Frame 11 — local input + predicted remote. Save snapshot.
...
Frame 15 — arriba llega input real de frame 11. Difiere.
       Load snapshot frame 10. 
       Re-simulate frames 11-14 con input real del 11 + predicciones 12-14.
       Continue frame 15.
```

Componentes mínimos:
- **Simulación determinista** (§7).
- **Snapshot ring buffer** (N frames, típ 7-8 = 120 ms @ 60 Hz, o 16 = 250 ms a 60 Hz).
- **Serialize/deserialize** cada frame — **tiene que ser rápido** (<1 ms). Esto es lo que mata engines OOP con `shared_ptr` everywhere.
- **Input prediction**: típ. "repeat last input" o Markov ligero.
- **Desync detection**: checksum del state cada frame, compare cuando se confirma.

### 8.2 Estado del arte fighting games 2021-2026

| Juego | Motor | Rollback framework | Max rollback | Notas |
|-------|-------|--------------------|--------------|-------|
| Skullgirls | Custom 2D | GGPO (primer AAA ship) | 7 frames | 2012, benchmark |
| Street Fighter III Third Strike OE | GGPO | GGPO | 7 frames | Emu rollback |
| Killer Instinct (2013) | Unreal 3 + custom net | Custom GGPO-style | 8 frames | Primer AAA 3D con rollback desde día 1 |
| Mortal Kombat X | UE3 fork | Custom | 5-7 | |
| **Guilty Gear Strive** (2021) | UE4 + Arc System Works custom | Custom rollback | 8 frames | Industry benchmark 2021-2023 |
| **Street Fighter 6** (2023) | RE Engine | Custom built-from-scratch | 8+ frames | Capcom abandonó código SF5 |
| Mortal Kombat 1 (2023) | UE5 heavily forked | Custom | 8 frames | |
| Tekken 8 (2024) | UE5 | Custom rollback + delay hybrid | — | Hybrid aproach |

**Observaciones industry-wide**:
- Custom engine > UE/Unity por coste de serialize. UE4/5 tienen GC overhead que mata determinismo puro.
- Confirmed input window: 8 frames típico (133 ms @ 60). > 10 y siente horrible; < 5 exige internet perfecto.
- Desync detection: checksum CRC32 del state cada 30 frames, si diverge desconectar con mensaje.
- State serialization cost budget: < 1 ms por save, < 500 μs por load. RE Engine en SF6 opera ~400 μs save / 200 μs load.

### 8.3 Patrones compartidos en todos los títulos modernos

1. **Save only "sim state"** — excluir render, audio, UI. Reduce de 50 MB a 100-500 KB.
2. **Delta encoding** — si state es 500 KB pero frame-to-frame cambia 5 KB, guardar delta.
3. **Memory pool + handle system** (§4.4) — save = memcpy del pool, load = memcpy back. Nada de chasing pointers.
4. **Rollback-friendly code boundary**: marcar explícitamente `@RollbackState` los components participantes. Hot-cached, separados de render-only data.
5. **Bit-exact FP** — ver §7.2. Absolutamente non-negotiable.

**Fuentes §8:**
- GGPO official: https://www.ggpo.net/
- GGPO MIT open source: https://github.com/pond3r/ggpo (2019 release)
- SnapNet Netcode Architectures Rollback: https://www.snapnet.dev/blog/netcode-architectures-part-2-rollback/
- GG Strive rollback deep-dive: https://www.qualbert.com/guilty-gear-and-the-importance-of-rollback-netcode/
- Multiplayer networking resources: https://multiplayernetworking.com/

---

## 9. Editor architecture

### 9.1 Undo/redo — command vs memento vs transaction

| Patrón | Memoria | Re-apply speed | Complex ops | Usado por |
|--------|---------|-----------------|-------------|-----------|
| **Command** (cada accion objeto con `do()`/`undo()`) | Bajo (solo parámetros) | Rápido | Escalable pero exige disciplina | Photoshop, Blender ops |
| **Memento** (snapshot state full) | Alto (copia completa) | Trivial (swap state) | Fácil pero cuesta RAM | Pequeños editores |
| **Diff-based** (guardar delta bytes) | Medio | Medio (apply diff) | Escala a estados grandes (texturas, malla) | Photoshop heavy ops |
| **Transaction** (UE5: `Modify()` annotations en UObject, framework serializa delta) | Medio | Medio | UObject-centric | UE4/UE5 editor |

UE5 transaction system: `GEditor->BeginTransaction(TEXT("Move Actor"))`. Dentro, cualquier `UObject::Modify()` antes de mutar marca el objeto. Al finalizar, el sistema serializa el diff (campos dirtied). Permite scope anidado y merges. Ver https://forums.unrealengine.com/t/how-does-the-transaction-undo-redo-system-work/355792

### 9.2 Serialization — binary vs JSON vs USD

| Format | Speed | Size | Human-edit | Diffable | Version | Uso |
|--------|-------|------|------------|----------|---------|-----|
| Binary custom (FlatBuffers-like) | Ultra-rápido | Mínimo | No | No (sin tool) | Manual schemas | Runtime assets (cooked) |
| Protobuf/FlatBuffers/Capnproto | Rápido | Compacto | Parcial | Parcial | Schemas +compat | Network + save |
| JSON (rapidjson, nlohmann, simdjson) | Medio | Verbose | Sí | Sí | Manual | Config, editor assets source |
| YAML | Lento | Medio | Sí | Sí | Manual | Unity .prefab (hecho mal cost = pain) |
| **USD (OpenUSD)** | Medio | Medio (con .usdc binary) | Sí (.usda) | Sí (layer system) | Composition built-in | 3D scenes industry-wide 2023+ |

USD es ya stdandar de facto para intercambio 3D (Pixar originalmente, hoy NVIDIA Omniverse + Apple RealityKit + Autodesk + UE5 + Unity + Houdini + Maya). **Layers + overrides** permite nondestructive editing: artist puede override prop de un asset sin tocar el .usd original.

Para alze-engine 2026, patrón razonable:
- **Source assets**: USD (`.usda` text + `.usdc` binary).
- **Config/prefabs editor**: JSON con schemas typesafe codegen.
- **Cooked runtime**: binario flatbuffer-style.

### 9.3 Play-In-Editor (PIE)

UE5 enfoque (dual-world):
- Al entrar PIE, duplica `UWorld` entera (editor → PIE world). 
- Garbage collector separa both worlds.
- Al salir, destruye PIE world y revierte al editor world.

Costo: ~100-500 ms "Entering PIE" con mundo grande (duplicate of 100MB+ de UObjects). Cuello en hot-iteration.

Alternativa (Unity "Edit Mode"): transaction-log based. Record changes durante play, al parar, rollback via undo/redo stack. Más ligero pero más frágil (fácil olvidar mark dirty).

### 9.4 Inspector auto-gen de reflection

UE5 `UPROPERTY` macros → UHT parsea y genera metadata. Editor reflection runtime introspecta y pinta UI. Similarly Unity `[SerializeField]` + Godot `@export`.

Sin reflection stdandar (§10), opciones en C++17:
- **refl-cpp**: macros explicit `REFL_FIELD(name)`. Decente DX.
- **Boost.PFR**: aggregate-only, cero macros. Limitación: no funciona con constructores, no names de fields (solo indices).
- **Codegen offline** (propio UHT-like): regex/libclang → emite `.reflection.cpp` junto a cada `.h`. Más trabajo pero cero runtime cost.

### 9.5 Asset browser con 100k assets — virtualización

- **Virtualized grid**: solo renderizar tiles visibles + 2 de buffer arriba/abajo. ImGui `ImGuiListClipper` o custom.
- **Thumbnail cache** en disco: BLAKE3(asset_guid + mtime) → 256×256 .jpg. LRU eviction.
- **Query index**: FTS5 SQLite sobre nombres + tags + metadata. 100k assets indexan < 1s, query < 10ms.
- **Directory watch** con debouncing 500ms.

### 9.6 Node graphs — compile-to-C++ vs interpret

| Engine | Node graph para | Strategy |
|--------|----------------|----------|
| **Snowdrop (Ubisoft/Massive)** | Materiales + gameplay | **Compile to C++** (offline). Short compile, zero runtime overhead |
| **UE5 Blueprint** | Gameplay | Bytecode + **VM interpret** — 10-15× slower que C++ |
| **UE5 Material** | Shaders | **Compile to HLSL/MSL** (offline) |
| **Unity Shader Graph** | Shaders | Compile to HLSL (offline) |
| **Unity Visual Scripting** | Gameplay | VM interpret |
| **Godot VisualScript** | Gameplay | DEPRECATED 4.0 por performance/complejidad |
| **UE Niagara** | VFX | Compile to HLSL compute |

**Lección**: VM interpret solo justifica para hot-reload dev workflow. Para shipping, compile-to-native. Blueprint en UE ha sido históricamente el mayor cuello de rendimiento en juegos UE.

**Fuentes §9:**
- UE5 transaction system: https://forums.unrealengine.com/t/how-does-the-transaction-undo-redo-system-work/355792
- Command vs Memento tutorial: https://takt.dev/design-pattern/advanced/practical/command-memento-undo
- OpenUSD intro: https://openusd.org/release/intro.html
- Blueprint VM anatomy: https://ikrima.dev/ue4guide/engine-programming/blueprints/bp-virtualmachine-overview/
- Blueprint performance: https://intaxwashere.github.io/blueprint-performance/
- Snowdrop node graph: https://www.massive.se/blog/games-technology/snowdrop/the-history-of-snowdrop-from-rd-concept-to-aaa-engine/

---

## 10. Reflection en C++17/20/23/26

### 10.1 Por qué RTTI es banned en engines

1. **Tamaño**: `typeid`/`dynamic_cast` emiten type_info por cada polymorphic class (nombre mangled + vtable offset). Engine con 10k classes = 2-5 MB exe bloat.
2. **Non-deterministic layout**: hash orden compiler-dependent, vtable offsets varían entre builds.
3. **Runtime cost**: `dynamic_cast` ~20-50 ns vs static cast 0 ns.
4. **No compile-time introspection** de miembros — necesitas macros o codegen igual.

Por eso UE4/5, Unreal Source, Frostbite, RE Engine, Snowdrop, Cryengine, id Tech — *todos* tienen `-fno-rtti`.

### 10.2 Opciones para reflection en no-RTTI C++17

| Approach | Macros needed | Names | Static/Runtime | Pros | Contras |
|----------|---------------|-------|-----------------|------|---------|
| **Boost.PFR** | 0 | No (sólo indices) | Static-only (aggregate) | Cero boilerplate | Solo aggregates, no names, no methods |
| **refl-cpp** (veselink1) | Per field `REFL_FIELD(x)` | Sí | Static | Compile-time iteration | Macro soup moderado |
| **rttr** (rttrorg) | Registro runtime en `.cpp` | Sí | Runtime | Dynamic features (script bind) | Requires registration, runtime cost |
| **magic_get / Boost.PFR** | 0 | No | Static | Similar PFR | Ídem |
| **Codegen (libclang-based)** | Anotaciones `[[reflect]]` | Sí | Static via generated header | Cero runtime overhead, full power | Build complexity |
| **UE5 UHT-style** | `UPROPERTY()` macros | Sí | Mixed (FName at runtime) | Probado a escala AAA | Heavy build step |
| **Qt moc** | `Q_OBJECT` macro | Sí | Runtime | Signals/slots | Qt lock-in |

### 10.3 P2996 — C++26 reflection — el cambio

Adoptado formalmente en WG21 St. Louis plenary, junio 2025. Entra en C++26. Core:

```cpp
struct Player {
    std::string name;
    int hp;
    float speed;
};

consteval auto field_names_of(std::meta::info R) {
    std::vector<std::string_view> names;
    for (auto member : nonstatic_data_members_of(R))
        names.push_back(identifier_of(member));
    return names;
}

// Splicer [: R :] lifts a reflection back into the code
template<typename T>
void serialize(const T& v) {
    constexpr auto members = nonstatic_data_members_of(^T);
    [:expand_each(members):]({
        save(v.[:member:]);  
    });
}
```

Prefix `^` = reflect, `[: refl :]` = splice. `std::meta::info` opaque handle.

**Hoy (2026)**: compilador experimental Clang P2996 branch en Compiler Explorer funciona. Producción general requiere Clang 20+ o GCC 15+ (esperados 2026-2027).

Para alze-engine C++17: **no está disponible**. Workaround hasta migrar: **codegen offline con libclang**, o refl-cpp + disciplina de macros.

### 10.4 Pattern recomendado 2026 para alze-engine

1. **Introspección de POD/aggregate**: Boost.PFR (zero macros, C++17).
2. **Introspección con nombres de fields**: refl-cpp o codegen.
3. **Dynamic scripting bindings**: hand-rolled via concepts + CRTP, NO rttr (pesa).
4. **Roadmap C++26**: cuando Clang 20 estable en CI, migrar serialización y editor UI a reflection nativa. Retirar refl-cpp.

**Fuentes §10:**
- P2996 paper: https://isocpp.org/files/papers/P2996R4.html
- Learn Modern C++ P2996 explicada: https://learnmoderncpp.com/2025/07/31/reflection-in-c26-p2996/
- Lemire (why care): https://lemire.me/blog/2025/06/22/c26-will-include-compile-time-reflection-why-should-you-care/
- refl-cpp: https://github.com/veselink1/refl-cpp
- rttr: https://github.com/rttrorg/rttr
- Boost.PFR limitations: https://www.boost.org/doc/libs/master/doc/html/boost_pfr/limitations_and_configuration.html

---

## 11. Scripting patterns

### 11.1 Comparativa de lenguajes scripting embeddables

| Lang | Binding cost | Runtime perf | Memory | Hot reload | Debug | Shipped en |
|------|---------------|--------------|--------|------------|-------|-------------|
| **Lua 5.4** | Bajo (C API) | JIT (LuaJIT): 2-5× slower C. Plain: 20-50× slower | ~300 KB runtime | Trivial (eval chunk) | Ok (mobdebug) | WoW, Roblox (Luau), Factorio, Warframe |
| **AngelScript** | Medio (C++-like, natural types) | Interpret: 30-50× slower. JIT community 5-10× | ~600 KB | Yes (rebuild module) | Decente | Bitsquid, ArkAngel, muchos indie |
| **Wren** | Bajo (moderno) | Interpret: 20-40× slower | ~100 KB | Yes | Mínimo | Varios indie |
| **Mono (C#)** | Alto | JIT near-native. Burst AOT native. | 15-30 MB runtime | HotReload .NET | Excelente (Visual Studio, Rider) | UE5 (via plugins), Unity, Godot |
| **WebAssembly (Wasmtime/Wasmer)** | Medio | AOT Cranelift: 1.5-2× slower C. Interpret: 10× | 2-5 MB runtime | Module swap | mejorando | Roblox (WASM option), Second Life, emergente |
| **Native script DLL (C++/Rust)** | Alto | Native | N/A | Vía Live++/hot-reload patterns §6 | Full debug | UE5 (hybrid), custom engines |

### 11.2 Dual-path pattern: interpret dev + AOT native ship

RE Engine (Capcom, SF6) y algunos motores modernos implementan **dual-path**:

1. **Development**: script runs en **interpreter/VM**. Hot reload <1s, iter rápida.
2. **Shipping**: el mismo script **AOT-compilado a C++ generado o WASM AOT**. Runtime ~1-2× C++.

Ventajas: devs no sufren rebuild, pero release tiene zero-overhead.

Implementación: el script language define un AST estable + dos backends (tree-walker interpreter + AST-to-C++ codegen). Build system en shipping corre el codegen, compila junto con el engine.

Ejemplo OSS: Lua → LuaJIT (jit) o Luau (typed Lua AOT). AngelScript tiene JIT community pero no mainline AOT.

### 11.3 Recomendación alze-engine 2026

- **Scripting primario**: Lua 5.4 para sencillez + Luau fork si tipos estáticos importan. Binding via `sol2` (C++17 header-only, pequeño).
- **Hot zone**: si queremos determinismo exacto (§7/§8), el script corre en fixed-tick y NO asigna memoria en hot frame (arena allocator específico de script).
- **Ship optimization**: considerar a futuro (post-MVP) compilar Lua→C++ para módulos críticos de gameplay.
- **NO Mono/C#**: demasiado heavy para engine C++17 no-except.

**Fuentes §11:**
- Lukas Boersma scripting language choice: https://lukas-boersma.com/en/blog/2016-10-01-choosing-scripting-language
- Luau (Roblox): https://luau-lang.org/
- sol2: https://github.com/ThePhD/sol2
- AngelScript: https://www.angelcode.com/angelscript/
- Wren: https://wren.io/

---

## 12. Profiling infrastructure

### 12.1 Tracy — Bartosz Taudul

Profiler open-source (BSD 3-clause) que es hoy *de facto* standard en engines custom (Wicked Engine, Flax, O3DE, bsnes, muchos indie AAA):

**Arquitectura**:
- **Client** (el juego): lib `Tracy.cpp` (~5 MB compiled). Macros `ZoneScoped`, `FrameMark`, `TracyGpuZone`.
- **Server** (GUI): app separada, TCP/IPC conecta con client.
- Zonas miden con `__rdtsc` (x86) o `clock_gettime(CLOCK_MONOTONIC_RAW)` (<10 ns overhead per zone).
- Buffer ring en client, flush continuo al server.

**Features 2026** (v0.11+):
- GPU zones: Vulkan via VK_EXT_calibrated_timestamps + VK_EXT_debug_utils, D3D12 via PIX markers, OpenGL via ARB_timer_query.
- **Context switches** (Linux: ebpf; Windows: ETW) — ve qué thread fue preempted.
- **Lock contention**: `TracyLockable` wrapper sobre `std::mutex`.
- **Memory events**: `TracyAlloc`/`TracyFree` — detecta leaks.
- **Frame overview + flame graph**.
- **CPU topology visualization** (NUMA, SMT, P-cores vs E-cores).

Integrar en 200k LOC sin death-by-macros: envolver `new`/`delete` (o allocator) una vez, añadir `FrameMark` en main loop, y `ZoneScopedN("name")` solo en funciones candidatas. No se necesita instrumentar todo.

### 12.2 GPU markers y queries

Vulkan: `vkCmdBeginDebugUtilsLabelEXT` + `vkCmdWriteTimestamp2`. RenderDoc y PIX parsean estos.

D3D12: `ID3D12GraphicsCommandList::SetMarker`/`BeginEvent`/`EndEvent` via PIX library.

Patrón: en cada pass, emitir un marker. Tools como Nsight Graphics, PIX, RenderDoc parsean y muestran timeline gpu-side correlated con CPU zones si el profiler sabe ambos clocks.

**Calibrated timestamps**: `VK_EXT_calibrated_timestamps` da una función para consultar *mismo* punto en dos clocks (host + device). Permite calcular offset + skew GPU↔CPU. Sin esto, los tiempos GPU y CPU no son comparables.

### 12.3 Otras opciones

- **Optick** (bombomby): similar Tracy, más ligero, menos features. Shipped en Insomniac internos.
- **Remotery**: HTML UI, web-based. Bueno para embed.
- **Superluminal** (commercial): best-in-class sampling + instrumentation para C++. Costoso.
- **Intel VTune / AMD uProf**: sampling profilers system-wide. Complementarios — detectan hot spots Tracy no cubre (microarch events, branch mispred rate, cache miss rate).

**Fuentes §12:**
- Tracy repo: https://github.com/wolfpld/tracy
- Tracy manual PDF: https://github.com/wolfpld/tracy/releases (cada release trae tracy.pdf)
- O3DE Tracy integration: https://www.docs.o3de.org/docs/user-guide/profiling/cpu_profiling/
- Optick: https://github.com/bombomby/optick
- PIX for Windows: https://devblogs.microsoft.com/pix/

---

## 13. Crash reporting / telemetry

### 13.1 Crashpad/Breakpad pipeline

**Breakpad** (Google, pre-2010): in-process minidump generator. Integrado en Firefox, Chrome, Valve Steam. Signals handler (Linux), UnhandledExceptionFilter (Win), Mach exception ports (macOS).

**Crashpad** (Google, post-2015, reemplazo): **out-of-process** dumps. Proceso "crashpad_handler" separado que vigila el game. Más robusto (si el game corrompe su memoria no corrompe el reporter).

**Minidump format** (`.dmp`): subset del Windows memory dump format extendido. Contiene:
- Thread list + register state (todas las threads, no solo la crasher).
- Callstack memory (parte cercana de cada stack).
- Loaded modules (DLLs/sos + file versions).
- Exception info (type, address).
- Annotations custom (build version, user id, game state hash).

**Symbolication**: el `.dmp` por sí solo da addresses raw. Se necesita el **symbol server** con `.pdb`/`.sym`. Tools:
- `minidump_stackwalk` (breakpad/crashpad CLI).
- Sentry / BugSplat / Backtrace.io (SaaS).
- Socorro (Firefox's open-source pipeline).

### 13.2 GPU hangs — NVIDIA Aftermath

GPU crashes son categoría aparte. Razones típicas:
- **TDR (Timeout Detection Recovery)** — shader no termina en 2s.
- **Page fault** (Vulkan sparse, D3D12 reserved resources acceso inválido).
- **Invalid descriptor** (bindless con index OOB).

NVIDIA Aftermath SDK:
- Library C/C++ que hookea al driver.
- Al TDR, genera `.nv-gpudmp` con:
  - Faulting shader + línea HLSL/SPIR-V.
  - Last submitted commands.
  - Resource debug names (si `vkSetDebugUtilsObjectNameEXT` usado).
  - GPU register state en moment of fault.

Integración: `GFSDK_Aftermath_EnableGpuCrashDumps()` antes de crear device. Typical integration: ~200 LOC.

Equivalentes: **AMD Radeon GPU Detective** (similar). **Intel GPA Graphics Frame Analyzer** (debug, no crash-time).

### 13.3 Field crash feedback loops

Patrón production-grade:
1. Game crashea → minidump + log + custom annotations local.
2. Client sube zip a servidor (directo o via CDN a S3).
3. Servidor processes minidump via Stackwalk → groups by stack signature hash → attaches annotations.
4. Dashboard (Sentry/Backtrace/custom Grafana): top crashers por signature, por build, por hardware.
5. Dev recibe notif Slack/email al ver nuevo regression.

Time to feedback típico AAA 2024: minutos desde crash a ticket. Sin esto, los primeros weekends de lanzamiento son ciegos.

**Fuentes §13:**
- Breakpad docs: https://chromium.googlesource.com/breakpad/breakpad/+/master/docs/getting_started_with_breakpad.md
- Crashpad overview: https://chromium.googlesource.com/crashpad/crashpad/+/HEAD/doc/overview_design.md
- Sentry C++ crash SDK: https://docs.sentry.io/platforms/native/
- Backtrace.io: https://backtrace.io/
- NVIDIA Aftermath docs: https://docs.nvidia.com/nsight-aftermath/UserGuide/index.html
- Aftermath SDK: https://developer.nvidia.com/nsight-aftermath
- Jake Shadle Rust crash reporting: https://jake-shadle.github.io/crash-reporting/

---

## 14. Testing infrastructure

### 14.1 Unit test frameworks — taxonomy

| Framework | Build | Header-only | Sections | Benchmarks | Tests/sec | Uso |
|-----------|-------|--------------|----------|-------------|------------|-----|
| **doctest** | Ultra-rápido (~0s overhead) | Sí (1 header) | Sí | Básico | ~1M/s | Recomendado para engine (2024 JetBrains study) |
| **Catch2 v3** | Medio | No (v3 linked) | Sí | BDD, micro-bench | ~50K/s | Popular, ergonómico |
| **gtest** | Lento (gmock pesado) | No | Via fixtures | Separado (gbenchmark) | ~10K/s | De facto enterprise |
| **Boost.Test** | Lento (boost entero) | No | No | No | — | Legacy |

2024 JetBrains C++ Developer Ecosystem report pone los 4 como opciones validas; doctest recomendado para proyectos greenfield priorizando velocidad compile/run (ideal para engine 500+ tests).

### 14.2 Property-based testing — rapidcheck

```cpp
#include <rapidcheck.h>

rc::check("vector double-reverse is identity", [](std::vector<int> v) {
    auto rev = v; std::reverse(rev.begin(), rev.end());
    std::reverse(rev.begin(), rev.end());
    RC_ASSERT(rev == v);
});
```

Engine en math/physics: excelente para invariants (`mat4 * inv = identity`, `quat_normalize(q).length() == 1 ± ε`, `AABB ∪ AABB contains both`). Genera casos random + shrinks a failure mínimo.

Combinable con libFuzzer vía adapter (siedentop/rapidfuzz). Coverage-guided shrinking.

### 14.3 Fuzzing engine systems

LibFuzzer (llvm) / Google FuzzTest: coverage-guided mutation. Target:
- Deserialization (savefiles, levels).
- Protocol parsers (netcode messages).
- Math libraries (weird NaN, inf, denormal inputs).
- Shader parser / script parser.

Integration: un binario `*_fuzz_target` por sistema con `LLVMFuzzerTestOneInput`. CI corre 10 min por target por commit.

Note: libfuzzer activamente frozen, LLVM team recomienda **Centipede** (Google) o **FuzzTest** que wrappea ambos.

### 14.4 Visual / golden image regression

Patrón:
1. Render escena fija en headless → PNG.
2. Compare con `reference.png` via PSNR/SSIM + pixel-diff threshold.
3. Si > threshold: fail, upload `diff.png` como CI artifact.

Tolerancia: 0.1% pixeles > 2/255 delta = fail típico. Demasiado estricto → flaky. Demasiado laxo → misses regressions sutiles.

Engines usando golden: id Tech, UE internal QA, Valve Source 2. OSS: **glamour** (OSS tool), custom Python usualmente.

### 14.5 Perf regression CI

**PerfGuard** (UE5 plugin, 2025): record baselines, detect regressions stat-significant. Similar para custom engine: **Google Benchmark** + **bencher.dev** tracking + fail CI si p95 >+10%.

Patrón:
```
# CI job:
build --release
run benchmark_suite --benchmark_format=json > bench.json
upload to bencher.dev / timescale DB
query: current vs median últimos 30 commits; if regression >10% → fail
```

### 14.6 Testing physics engines

Desafío: tests discretos de funciones no cubren convergencia ni estabilidad.

Patrones:
- **Convergence tests**: simular con dt = 1/60, 1/120, 1/240. Resultado debe converger (error O(dt²) para RK4, O(dt) para Euler).
- **Energy conservation**: sistema conservativo (2 bodies orbiting) — energía total debe drift < 1%/sec.
- **Known-result scenarios**: ball drop from 10m at 9.81 m/s² debe llegar al suelo en √(2h/g) ≈ 1.428 s. Tol ±5%.
- **Restitution**: ball bouncing con e=0.5 debe rebotar a altura 0.25h.
- **Stability stress**: 1000 cubos en torre, no deben atravesarse ni explode al cabo de 60s.

**Fuentes §14:**
- doctest: https://github.com/doctest/doctest
- Catch2: https://github.com/catchorg/Catch2
- rapidcheck: https://github.com/emil-e/rapidcheck
- Google fuzztest: https://github.com/google/fuzztest
- libFuzzer: https://llvm.org/docs/LibFuzzer.html
- PerfGuard: https://getperfguard.com/
- Jolt physics (reference tested engine): https://github.com/jrouwe/JoltPhysics

---

## 15. C++23/26 features

### 15.1 Tabla de evaluación para engine no-RTTI/no-exceptions

| Feature | Paper | Stdlib | Útil en engine no-except | Notas |
|---------|-------|--------|--------------------------|-------|
| `std::expected<T,E>` | P0323 | C++23 | **Sí, crítico** | El `Result` de Rust en C++. Sustituye excepciones en signatures. |
| `std::mdspan<T, Extents>` | P0009 | C++23 | Sí | Multi-dim view. Útil para tensores (ML, physics state matrices). |
| Monadic `optional`/`expected` | P0798 | C++23 | Sí | `.and_then()`, `.or_else()`, `.transform()`. Compone errores sin throw. |
| `std::print` / `std::println` | P2093 | C++23 | Sí | Faster, typesafe than iostream. No ABI issues. |
| `std::flat_map` / `flat_set` | P0429 | C++23 | Sí | Sorted-vector based. Determinista iteración (§7.4). |
| `constexpr` math (sin/cos/etc.) | P1467 | C++26 | Sí | Tablas LUT compiletime. |
| `std::execution` (Sender/Receiver) | P2300 | C++26 | **Sí, game-changing** | Async story unificada. Necesita investigación ROI vs fibers custom. |
| Reflection `^`/`[::]` | P2996 | C++26 | **Sí, game-changing** | Mata necesidad de UHT/refl-cpp (ver §10). |
| Pattern matching | P2392 | Target C++26 (no yet) | Sí | `inspect` substituye visitor/variant switch. |
| `std::generator` (coroutine gen) | P2502 | C++23 | Depende | Stackful? No, stackless. Requires `co_yield`. |
| Contracts | P2900 | C++26 target | Neutral | `pre`/`post`/`assert`. Nice pero coste runtime vs release. |
| `if consteval` | P1938 | C++23 | Sí | Útil para generic code compile vs runtime paths. |
| `std::stacktrace` | P0881 | C++23 | Sí | Sustituye mini-impl custom, complementa Crashpad. |
| `std::views::zip`, `cartesian_product`, `chunk` | P2321, P2374, P2442 | C++23 | Sí | Composable loops sobre datos paralelos. |
| `deducing this` | P0847 | C++23 | Sí | Elimina const/nonconst duplication, CRTP simplification. |

### 15.2 NO compatibles con `-fno-exceptions`

- Cualquier cosa que use `throw` en stdlib (e.g., `std::vector::at()`, `std::stoi`, `std::bad_alloc` paths).
- `std::expected` está diseñado justamente para REEMPLAZAR eso en codebases no-except.

### 15.3 NO compatibles con `-fno-rtti`

- `std::any` usa `typeid` internamente → rompe.
- `dynamic_cast` no disponible → usar static/CRTP.
- `std::type_index` unusable.

### 15.4 `std::execution` (P2300) — perspectiva

Modelo sender/receiver:

```cpp
namespace ex = std::execution;

auto snd = ex::schedule(my_thread_pool)
         | ex::then([]{ return load_asset("foo.bin"); })
         | ex::then([](Asset a){ return decompress(a); })
         | ex::then([](DecompressedAsset d){ upload_gpu(d); });

ex::sync_wait(snd);
```

**Para alze-engine 2026**:
- C++17 constraint → no disponible nativo.
- stdexec (NVIDIA ref impl) requiere C++20.
- Si migramos a C++20/23 en 2027, considerar **reemplazar** custom job system por `std::execution`. Trade-off: menos control fino que fibers custom, pero composabilidad estándar y futura interop.

**Fuentes §15:**
- P2996 reflection: https://isocpp.org/files/papers/P2996R4.html
- P2300 execution: https://wg21.link/P2300
- std::expected: https://www.cppstories.com/2024/expected-cpp23/
- mdspan reference: https://github.com/kokkos/mdspan
- C++23 compiler support: https://en.cppreference.com/cpp/compiler_support/23
- MC++ std::execution overview: https://www.modernescpp.com/index.php/stdexecution/

---

## 16. Ranking urgencia para alze-engine 2026

**Contexto asumido**: C++17, no-RTTI, no-exceptions, actualmente compila, tiene `build_err.txt` y `ERRORES.md` activos. Engine "proto" con assets cargados, renderer rudimentario, físicas tentativas. 1-2 devs.

**Criterios de priorización**:
1. Cost vs. benefit del pattern.
2. Dependencias (patterns que habilitan otros).
3. Ventana de corrección (antes de que el código crezca hace 10× coste).
4. Riesgo de bug si se posterga.

---

### 🥇 P0 — Cambios base infrastructurales (adoptar primero, habilitan todo lo demás)

**1. Handle-based pool system + arena allocator (§4)**

Justificación: es la **dependencia oculta** de ECS, streaming, hot reload, rollback, serialización. Si los objetos viven en un pool con `{idx, gen}` handles desde hoy, *todo* patrón posterior encaja. Si se deja para luego, reescribir memory layout del engine cuesta semanas.
Esfuerzo: 3-5 días implementar pool<T,N> template + arena per-frame + handle<T>.
Señal de adopción: `ERRORES.md` probablemente tiene casos de UAF o pointer invalid — este patrón los elimina clase-completa.

**2. Determinism checklist baseline (§7)**

Justificación: imponer FTZ/DAZ + `-fno-fast-math` + `-ffp-contract=off` + evitar `unordered_map` en sim loop cuesta 1 día y prohibe una clase de bugs imposibles de debuggear después.
Esfuerzo: 1 día flags + audit uso de `<random>` + `<unordered_map>`.
Payoff: replays testeables, rollback posible a futuro, bugs reproducibles QA.

**3. Fixed timestep loop Gaffer-style (§7.1)**

Justificación: si el engine hoy hace `dt = now - last` variable, ya tiene non-determinism latente. Cambiar a accumulator fixed es ~50 LOC.
Esfuerzo: <1 día.
Payoff: estabilidad de sim, base para netcode, tests reproducibles.

**4. Tracy profiling integrado (§12)**

Justificación: sin profiling **nada** se puede optimizar. Tracy + ZoneScopedN en 20 funciones core se adopta en medio día.
Esfuerzo: 1 día inicial + añadir zones incrementalmente.
Payoff: visibilidad permanente; cada PR review puede mirar trace.

---

### 🥈 P1 — Amplifiers de productividad y calidad

**5. Job system fiber-based (§3)**

Justificación: el engine necesita paralelismo CPU (physics broadphase, animation, culling, audio). Hacerlo con `std::thread` directo acaba en lock hell. Fiber job system + counter sync es 800-1500 LOC one-time.
Esfuerzo: 5-10 días (portar FiberTaskingLib o rodar custom sobre boost::context).
Payoff: 2-4× speedup en CPU-bound workloads, fundación para todo paralelismo.

**6. Crashpad + Tracy memory zones (§13 + §12)**

Justificación: sin crash reporting, QA ciego. Out-of-process dump es 200 LOC integration. Aftermath para GPU cuando toquemos D3D12/Vulkan.
Esfuerzo: 2 días Crashpad, +1 día Aftermath (cuando aplique).
Payoff: bugs de campo actionable en minutos.

**7. Testing infrastructure doctest + rapidcheck + golden images (§14)**

Justificación: engine sin tests es tiempo prestado. doctest es zero friction. rapidcheck sobre math lib cubre casos Wild que QA nunca encontrará.
Esfuerzo: 2 días setup + fixture tests + CI.
Payoff: refactor confidence, regression prevention, onboarding docs.

---

### 🥉 P2 — Feature-level patterns (cuando el core esté sólido)

**8. ECS archetype (§2)**

Justificación: sin ECS, el engine probablemente tiene `GameObject` con virtual inheritance (olor a OOP). Migración completa es trabajo, pero archetype-based SoA es el modelo "correcto" para 2026. Se puede empezar con un subsystem (partículas) antes de migrar todo.
Esfuerzo: 10-20 días migración progresiva.
Payoff: cache performance 5-10×, data-driven gameplay, fundación para multiplayer determinismo.

**9. Hot reload pipeline (§6)**

Justificación: productividad dev. Primero shader hot reload (fácil, 1 día). C++ hot reload después via Live++ (licencia) o cr.h (OSS, simple).
Esfuerzo: 1 día shaders + 3-5 días código.
Payoff: iter time 30s → 2s = 10× más experimentos/día.

**10. Reflection-lite + serialización (§9 + §10)**

Justificación: necesario para savefiles, editor inspector, network serialize. Boost.PFR + custom codegen para nombres.
Esfuerzo: 3-5 días.
Payoff: editor UI auto-gen, savefiles robustos, networking-ready.

---

### 🎯 P3 — Futuro (roadmap 2027+)

**11. std::execution** cuando migremos a C++26.
**12. Rollback netcode** si el juego target lo requiere (multiplayer competitivo).
**13. DirectStorage + SFS** cuando tengamos renderer GPU-driven maduro.
**14. USD serialization** si queremos pipeline content AAA-compatible.
**15. Scripting dual-path** Lua+Luau con AOT fallback.

---

### Resumen ejecutivo ranking

| Rank | Pattern | Esfuerzo | Habilita | Priority |
|------|---------|----------|----------|----------|
| 1 | Handle-based pool + arena | 3-5 d | ECS, streaming, rollback, serialize | **P0** |
| 2 | Determinism flags + RNG | 1 d | Tests, replays, rollback | **P0** |
| 3 | Fixed timestep loop | <1 d | Stability, determinism | **P0** |
| 4 | Tracy profiling | 1 d | All perf work | **P0** |
| 5 | Fiber job system | 5-10 d | Parallelism | P1 |
| 6 | Crashpad + Aftermath | 2-3 d | Field quality | P1 |
| 7 | doctest + rapidcheck + golden | 2 d | Refactor safety | P1 |
| 8 | ECS archetype migration | 10-20 d | Cache perf, data-driven | P2 |
| 9 | Hot reload (shader + C++) | 4-6 d | Dev velocity | P2 |
| 10 | Reflection-lite + serialize | 3-5 d | Editor, networking | P2 |

**Total P0+P1 stack**: ~15-25 días dev estimado → diferencia ENORME entre "hobby engine" y "motor con disciplina profesional". Sin este mínimo, los patrones P2/P3 compound de manera frágil.

---

## Meta: Fuentes y gaps

**Fuentes primarias consultadas (≥35)**:

1. Mike Acton CppCon 2014 slides (GitHub CppCon)
2. Stoyan Nikolov CppCon 2018 slides (GitHub CppCon)
3. Andrew Kelley Handmade Seattle 2021
4. Vittorio Romeo CppCon 2025 (isocpp.org blog)
5. Christian Gyrling GDC 2015 (media.gdcvault.com)
6. Johan Andersson Siggraph 2009 (UC Davis idav)
7. Sander Mertens Flecs docs (flecs.dev)
8. Bevy ECS V2 PR (github.com/bevyengine)
9. Mass Entity UE5 docs (dev.epicgames.com)
10. arxiv 2508.15264 ECS concurrency paper
11. EG Digital Library sparse-set vs archetype paper
12. std::execution P2300 (wg21.link)
13. stdexec NVIDIA reference (github.com/NVIDIA/stdexec)
14. TLSF paper (semanticscholar)
15. mimalloc repo + bench (github.com/microsoft/mimalloc)
16. rpmalloc (github.com/mjansson/rpmalloc)
17. Handles vs pointers (floooh.github.io)
18. Rigtorp huge pages
19. Sampler Feedback Spec (Microsoft DirectX-Specs)
20. RTX IO GDeflate (developer.nvidia.com blog)
21. Virtual Texturing UE5 docs
22. Gaffer Fix Your Timestep
23. Bruce Dawson FP determinism
24. Sherry Ignatchenko CppCon 2024 FP determinism
25. Vigna xoshiro/xoroshiro (prng.di.unimi.it)
26. PCG (pcg-random.org)
27. GGPO (ggpo.net + github.com/pond3r/ggpo)
28. SnapNet rollback blog
29. Intaxwashere Blueprint performance
30. ikrima UE4 blueprint VM
31. Massive Entertainment Snowdrop history
32. OpenUSD intro
33. P2996 reflection paper (isocpp.org files)
34. Lemire C++26 reflection
35. refl-cpp (github.com/veselink1)
36. Boost.PFR docs
37. Live++ (liveplusplus.tech)
38. UE5 Live Coding docs
39. Tracy repo + manual
40. Crashpad design doc (chromium.googlesource.com)
41. NVIDIA Aftermath SDK docs
42. doctest + Catch2 vs gtest comparisons
43. rapidcheck (github.com/emil-e/rapidcheck)
44. PerfGuard UE5
45. libFuzzer docs (llvm.org)
46. Cilk work-stealing paper (uwo.ca)
47. Chase-Lev deque (ACM)
48. RichieSams FiberTaskingLib
49. Chromium Breakpad docs
50. Jake Shadle crash reporting blog

**Gaps identificados** (áreas que requerirían research dedicado adicional):

1. **Animation system cross-cutting**: anim state machines, retargeting, compression (ACL library). No cubierto — es tema propio.
2. **Audio engine patterns**: DSP graph, spatializacion, streaming audio. Tema propio.
3. **Network protocol layer** (bajo GGPO): reliable UDP, connection management, NAT traversal. Tema aparte.
4. **Navmesh / pathfinding**: Recast/Detour patterns. Tema aparte.
5. **Shader compilation pipeline**: DXC workflow, HLSL→SPIR-V→MSL, permutations. Merecería research dedicado.
6. **Benchmarking de TLSF vs mimalloc en workload de engine real** (no Doom 3): gap — los benchmarks disponibles son generales, no game-specific reciente 2024+.
7. **Open-source fiber libs comparación objetiva** (boost::context vs libaco vs libfiber): gap — info dispersa.
8. **Concrete SF6 netcode details**: Capcom no ha publicado paper. Solo postmortems comunidad.
9. **Empírica: cuánto cuesta hot reload C++ a escala UE/AAA** (dev-day productividad delta real): gap — poca data pública.
10. **Reflection C++26 real-world pitfalls**: aún nadie lo ha shipped en prod a escala engine.

---

**Fin del documento** — `/root/lab_journal/research/alze_engine/deep_2026_04_22/cross_cutting_patterns.md`
