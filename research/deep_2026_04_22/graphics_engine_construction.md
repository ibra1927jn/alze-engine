# Cómo se construye un motor gráfico moderno — guía capa por capa para ALZE

> **Fecha:** 2026-04-22
> **Audiencia:** ingenieros diseñando `/root/repos/alze-engine` (C++17, no-RTTI, no-exceptions, SDL2+OpenGL 3.3, ~55k LOC).
> **Scope:** el "HOW" de construcción — algoritmos, data structures, números concretos — complementario a la research previa (`_sintesis.md`, `_sintesis_round2.md`, `r3/frame_graph_bindless.md`, `r3/mesh_shaders_work_graphs.md`, `r3/nanite.md`, `r4/decima.md`, etc.).
> **Evita duplicar:** los papers de Frostbite/RDG/Nanite/Lumen/Halcyon ya quedaron cubiertos. Aquí se agrega el plomeado: handle pools, permutation caches, bake pipelines, debug affordances, y el cronograma de qué conviene hacer en v1 vs v2 vs v3.

---

## 0. Qué hace un motor gráfico moderno — taxonomía de 7 capas

Un motor renderer ship-grade atraviesa 7 capas apiladas. Cada una tiene un contrato bien definido con la siguiente; todas las decisiones de las capas altas dependen de la API de la capa inmediatamente inferior. El motor falla cuando se saltan capas (típico anti-pattern: el "renderer monolítico" que llama `glDrawElements` desde el gameplay).

```
+--------------------------------------+
| 7. Debug/Tools (RenderDoc, Tracy,    |   Observability cross-capas
|    PIX markers, live shader reload)  |
+--------------------------------------+
| 6. Build/Bake (cook offline,         |   Asset pipeline, DDC
|    BC7/ASTC, meshlets, PSO cache)    |
+--------------------------------------+
| 5. Post-processing (TAA, bloom,      |   Lee la scene color, escribe swapchain
|    tonemap, DLSS/FSR upscale)        |
+--------------------------------------+
| 4. Shading/Material infrastructure   |   HLSL source → dxc → SPIR-V → GLSL/MSL
|    (permutation cache, PSO precache) |
+--------------------------------------+
| 3. Renderer frontend (culling,       |   Itera la escena, emite drawcalls abstractos
|    sort-key / frame graph, DAG)      |
+--------------------------------------+
| 2. Resource management (handles,     |   Texture/Buffer/Pipeline handles, streaming
|    descriptor heaps, residency)      |
+--------------------------------------+
| 1. Platform layer / RHI              |   Abstrae GL/VK/D3D12/Metal
|    (command buffers, fences)         |
+--------------------------------------+
```

Las capas 1-3 son el motor. Las 4-5 son la "personalidad gráfica" (qué aspect visual). Las 6-7 son el "cómo se hace el motor shipable" (tooling). **La research de ALZE ya cubrió capas 3-4 en profundidad; este file reforzará 1, 2, 6 y 7, que son los que convierten un prototipo en un motor shipeable.**

---

## 1. Platform layer / RHI abstraction

### 1.1 El patrón `Handle<T>` opaco de 64 bits (estilo bgfx+)

bgfx usa handles de 16-bit por legacy (máximo 65535 recursos por tipo); para un motor moderno se recomiendan handles de 64-bit con el layout:

```cpp
// 64-bit handle: [generation: 32][index: 20][type: 12]
//  - generation: previene use-after-free ("ABA" problem en pools)
//  - index: hasta ~1M recursos por tipo
//  - type: 4096 tipos distintos (Buffer, Texture, Pipeline, Sampler, ...)
struct RhiHandle {
    uint64_t value;
    static constexpr uint64_t kTypeBits = 12;
    static constexpr uint64_t kIndexBits = 20;
    static constexpr uint64_t kGenBits   = 32;
    [[nodiscard]] uint32_t gen()   const noexcept { return uint32_t(value >> 32); }
    [[nodiscard]] uint32_t index() const noexcept { return uint32_t((value >> 12) & ((1u<<kIndexBits)-1)); }
    [[nodiscard]] uint16_t type()  const noexcept { return uint16_t(value & ((1u<<kTypeBits)-1)); }
    [[nodiscard]] bool is_valid()  const noexcept { return value != 0; }
};

// Typed wrappers para catch-at-compile-time de mezclas
template<typename Tag>
struct TypedHandle { RhiHandle h; };
using TextureHandle  = TypedHandle<struct TextureTag>;
using BufferHandle   = TypedHandle<struct BufferTag>;
using PipelineHandle = TypedHandle<struct PipelineTag>;
```

**Pool de handles con freelist** (este es el corazón de bgfx — ver `bx::HandleAlloc` en el repo):

```cpp
class HandlePool {
    uint32_t free_head_ = kInvalid;           // freelist LIFO
    std::vector<uint32_t> gens_;              // gens_[idx] = generation actual
    std::vector<uint32_t> next_free_;         // freelist storage
public:
    RhiHandle alloc(uint16_t type) {
        uint32_t idx;
        if (free_head_ != kInvalid) {
            idx = free_head_;
            free_head_ = next_free_[idx];
        } else {
            idx = (uint32_t)gens_.size();
            gens_.push_back(0);
            next_free_.push_back(kInvalid);
        }
        return make_handle(gens_[idx], idx, type);
    }
    void free(RhiHandle h) {
        // Incrementa gen para invalidar viejos handles
        gens_[h.index()]++;
        next_free_[h.index()] = free_head_;
        free_head_ = h.index();
    }
    bool is_alive(RhiHandle h) const {
        return h.index() < gens_.size() && gens_[h.index()] == h.gen();
    }
};
```

El `gen++` al free es la clave: cualquier handle viejo con el mismo index queda invalidado porque su gen no coincide. Esto elimina la clase entera de bugs "use-after-free en recurso recently re-allocated" que bgfx sufre en su versión 16-bit (ver `bkaradzic/bgfx#1195`).

**Alojamiento diferido al free** (problem de bgfx): el render thread necesita poder referenciar un handle durante N-1 frames después de que game thread lo libere. Solución: añadir un "pending release queue" por frame. `free()` en game thread pushea a una cola; solo el render thread, tras consumir sus command buffers de ese frame, ejecuta el `gens_[idx]++` real. Esto requiere una barrera temporal: `release_frame_N = game_frame_N + 2`.

### 1.2 Command buffer recording + submission

Un RHI moderno separa **encoding** (game thread escribe comandos) de **submission** (render thread ejecuta). Tres modelos en la industria:

| Modelo | Ejemplo | Ventajas | Desventajas |
|---|---|---|---|
| **Stateless sort-key** | bgfx `submit(view, prog, state, sort_key)` | Trivial de multi-thread, redordenable por cost | No expresa compute/copy/transitions bien; GL-era |
| **Explicit command lists** | Vulkan `VkCommandBuffer`, D3D12 `ID3D12GraphicsCommandList` | Zero-overhead, multi-thread nativo | Caller es responsable de barriers, fences |
| **PSO + auto state** | Sokol `sg_apply_pipeline`+`sg_draw`, Diligent `IDeviceContext` | Simpler que Vulkan, soporta todos los backends | Runtime state tracking overhead ~5-10% |

ALZE debería adoptar **stateless sort-key frontend + explicit command lists backend**, mimicking bgfx. El frontend es simple; el backend emite command lists reales a Vulkan/D3D12 agrupando por bucket de sort-key.

Ejemplo canonical del encoder multi-thread (mirror de `bgfx::Encoder`):

```cpp
class CommandEncoder {
    std::vector<DrawCall> draws_;         // append-only, size hint 16k
    Arena scratch_;                        // per-encoder scratch memory (MB-sized linear)
public:
    void set_vertex_buffer(BufferHandle h, uint32_t offset);
    void set_texture(uint8_t stage, TextureHandle h, uint32_t sampler_flags);
    void submit(ViewId view, PipelineHandle pso, uint64_t sort_key) {
        DrawCall& dc = draws_.emplace_back();
        dc.view = view; dc.pso = pso; dc.sort_key = sort_key;
        dc.state_snapshot = take_snapshot_(scratch_);   // hace copia inmutable del state actual
    }
};

class Context {
    std::array<CommandEncoder, 64> encoders_;  // one per thread, cache-aligned
    void frame() {
        // merge, radix-sort by sort_key, dispatch per view
        std::vector<DrawCall> merged = merge_encoders_();
        radix_sort(merged.begin(), merged.end(), [](const DrawCall& d){ return d.sort_key; });
        render_thread_dispatch_(merged);
    }
};
```

Números: un encoder por-thread con arena de 4MB cubre ~30k drawcalls/frame con memoria reservada upfront. Frostbite (2015) midió que el bottleneck típico en un encoder compartido con mutex era ~500 μs por frame en 8 cores; el patrón per-thread encoder lo baja a < 50 μs.

### 1.3 Fence/semaphore primitives — modelo unified

Tres APIs, una abstracción. La clave es que **todas las APIs modernas (Vulkan 1.2+ timeline semaphore, D3D12 `ID3D12Fence`, Metal `MTLSharedEvent`) convergieron a un modelo común: counter monotónico 64-bit**. Legacy binary semaphores (Vulkan 1.0) son un dead-end.

```cpp
// Unified fence API — mappeable a todas las APIs modernas
class Fence {
    uint64_t current_value_ = 0;
    // Backend impl:
    //  - VK: VkSemaphore con VK_SEMAPHORE_TYPE_TIMELINE
    //  - D3D12: ID3D12Fence
    //  - Metal: MTLSharedEvent
public:
    uint64_t signal_next() { return ++current_value_; }
    void cpu_wait(uint64_t value, uint64_t timeout_ns);
    void gpu_wait(uint64_t value);       // enqueue wait en command buffer
};
```

Valor práctico: un único `Fence` por queue puede reemplazar el esquema "N frames in flight, N fences" de Vulkan 1.0. Ver `nvpro-samples/vk_timeline_semaphore` para el canonical sample.

Comparativa de backends:

| Primitive | OpenGL 3.3 | Vulkan 1.2 | D3D12 | Metal 3 | Sokol |
|---|---|---|---|---|---|
| Fence monotónico | `glFenceSync` (binario) | `VkSemaphore` timeline | `ID3D12Fence` | `MTLSharedEvent` | No expone (interno) |
| Command buffer | N/A (immediate) | `VkCommandBuffer` | `ID3D12CommandList` | `MTLCommandBuffer` | No expone |
| Multi-queue | No | Sí (graphics/compute/transfer) | Sí | Sí (compute + blit) | No |
| Split barriers | N/A | Sync2 eventos | Yes (`BEGIN_ONLY`) | Auto | No |

### 1.4 Backend plug architecture — el contrato RHI

El frontend del motor NUNCA debe compilar contra vulkan.h, d3d12.h, Metal.h. Toda la API pública del RHI es opaca. El backend se inyecta vía interface abstracta:

```cpp
// rhi.h — lo que el resto del motor include
namespace alze::rhi {
    enum class BackendKind : uint8_t { Null, GL33, GL46, Vulkan, D3D12, Metal };
    struct IDevice {
        virtual ~IDevice() = default;
        virtual TextureHandle  create_texture(const TextureDesc&)  = 0;
        virtual BufferHandle   create_buffer(const BufferDesc&)    = 0;
        virtual PipelineHandle create_pipeline(const PipelineDesc&) = 0;
        virtual void destroy(TextureHandle h)  = 0;
        virtual void destroy(BufferHandle h)   = 0;
        virtual void submit(Span<const DrawCall> draws, const ViewState& v) = 0;
        virtual void frame_begin() = 0;
        virtual void frame_end()   = 0;
    };
    IDevice* create_device(BackendKind, const DeviceDesc&);
}

// rhi_gl33.cpp — el backend. NADIE más ve GL.
#include "rhi.h"
#include <glad/glad.h>
namespace alze::rhi::gl33 {
    class DeviceGL33 final : public IDevice { /* ... */ };
}
```

**Regla de oro**: ninguna llamada `glXXX`, `vkXXX`, `ID3D12XXX` debe aparecer en un .cpp fuera de `backends/rhi_<backend>.cpp`. Esta separación pura es lo que permite añadir Vulkan después sin reescribir el renderer (ver §10 para evolution path).

### 1.5 Comparativa RHI externas

| Propiedad | bgfx | Sokol | Diligent | The Forge | ALZE pattern |
|---|---|---|---|---|---|
| Lenguaje | C++17 | C99-light | C++14/17 | C++17 | C++17 no-RTTI |
| Handles | 16-bit index typed | opaque struct | ref-counted `RefCntAutoPtr` | 64-bit | 64-bit gen+index+type |
| Threading | encoder per-thread | single-thread API | single-thread API | multi-thread cmd | encoder per-thread |
| Shader src | custom BGSL → `shaderc` | author-provided blobs | HLSL everywhere + reflection | HLSL + slang + FSL | HLSL → dxc → SPIRV → Cross |
| Backends | 10 | 6 (GL33, GLES3, D3D11, Metal, WebGPU) | 5 | 5 (D3D12/VK/GLES/Metal/iOS) | 2 (GL33 v1, Vulkan v2) |
| Excepciones | No | No | Sí (opt-out) | No | No |
| RTTI | No | No | Sí (opt-out) | No | No |
| LOC approx | ~50k | ~10k (sgx+app) | ~150k | ~300k | objetivo ~5-8k para RHI |

**Lección**: Sokol es el ejemplo más limpio de "RHI minimalista portable". bgfx es el más battle-tested. Diligent es el más "enterprise" (a costa de ser pesado). The Forge es AAA-complete pero tiene footprint enorme. **ALZE debe plantar bandera en "más grande que Sokol, más chico que bgfx"**.

Referencias:
- bgfx: `https://bkaradzic.github.io/bgfx/internals.html` (Karadzic, BSD-2)
- Sokol: `https://floooh.github.io/2017/07/29/sokol-gfx-tour.html` (Weissflog, 2017)
- Sokol diseño WebGPU: `https://floooh.github.io/2023/10/16/sokol-webgpu.html`
- Diligent architecture: `https://diligentgraphics.com/diligent-engine/architecture/` (Jagodin, 2016-2024)
- The Forge: `https://github.com/ConfettiFX/The-Forge`
- Vulkan timeline semaphore sample: `https://github.com/nvpro-samples/vk_timeline_semaphore` (NVIDIA)

---

## 2. Resource management

### 2.1 Texture / Buffer / Pipeline state objects

La Pipeline State Object (PSO) es la unidad inmutable de "configuración completa de rasterización": vertex layout, shader binaries, blend state, depth state, rasterizer state. En D3D12 y Vulkan es nativa (`ID3D12PipelineState`, `VkPipeline`). En GL3.3 hay que emularla con un "PSO cache" que al apply mutea todos los estados GL.

```cpp
struct PipelineDesc {
    ShaderHandle vs, ps;                      // shader binaries (stage-specific)
    VertexLayout vertex_layout;               // (attribute, location, format, offset, stride)[*]
    BlendState blend;                         // src, dst, op, write_mask por RT
    DepthStencilState depth;                  // depth test, write, stencil front/back
    RasterizerState raster;                   // cull mode, fill, polygon offset
    uint32_t render_target_count;             // 1-8 MRTs
    TextureFormat rt_formats[8];
    TextureFormat depth_format;
    uint32_t sample_count;                    // 1, 2, 4, 8 MSAA
};
```

**Número crítico**: una PSO de Vulkan típica cuesta 10-200 ms en crear (compilación + driver optimizer). Un PSO de D3D12 varía 5-50 ms. En GL3.3 es "gratis" pero se paga en cada glDraw con cambios de estado. Este cost es lo que obliga a pre-cachear (§6.3).

### 2.2 Descriptor / binding model — tres eras

La evolución histórica del modelo de binding divide en tres eras:

**Era 1 — slot-based "bindful" (OpenGL 3.3, D3D11, early Vulkan/D3D12):**
- Cada shader tiene N "slots" fijos (ej. 16 texturas, 8 samplers, 4 UBOs).
- Cada drawcall: `glActiveTexture(GL_TEXTURE0+i); glBindTexture(GL_TEXTURE_2D, id); ...` para cada slot.
- CPU overhead: ~100-500 ns por bind. Para 10k draws × 8 texturas cada uno = 8-40 ms/frame solo en binds. **Bottleneck clásico**.

**Era 2 — bindless descriptor indexing (Vulkan 1.2, D3D12 SM 6.6, Metal argument buffers tier 2):**
- Un descriptor set global con array de 16k-65k texturas bound una vez por frame.
- Shader hace `textures[material.albedo_idx]` — el índice viene via push constant o SSBO.
- CPU overhead: ~10 ns amortizado.
- Tardif midió 10x-30x reducción de CPU descriptor cost (ver `alextardif.com/Bindless.html`).

**Era 3 — GPU-driven indirect (D3D12 ExecuteIndirect, Vulkan `vkCmdDrawIndirectCount`):**
- La GPU misma genera el draw stream via compute shader.
- La CPU solo enqueue un "meta-dispatch": "ejecuta los draws en este buffer".
- CPU overhead: ~1 ns por "draw" conceptual (la GPU hace el trabajo).
- Aaltonen/Haar GDC 2015 demostraron 1M+ objects culled en < 0.5 ms.

| Era | CPU cost/draw | Binding flexibility | Validation | Hardware req |
|---|---|---|---|---|
| Bindful | 100-500 ns | Alta (por-draw) | Por-draw driver checks | Everything |
| Bindless | 1-10 ns | Media (heap update) | App-side | VK 1.2+ / SM 6.6 / Metal Argument Buffers |
| GPU-driven indirect | ~1 ns | Baja (heap estático) | App-side | VK 1.2 + multi_draw_indirect_count |

Referencias:
- Tardif, "Bindless Rendering": `https://alextardif.com/Bindless.html`
- Vulkan descriptor indexing: `https://docs.vulkan.org/samples/latest/samples/extensions/descriptor_indexing/` (KhronosGroup)
- D3D12 SM 6.6: `https://devblogs.microsoft.com/directx/hlsl-shader-model-6-6/`
- Aaltonen & Haar, "GPU-Driven Rendering Pipelines," GDC 2015: `https://advances.realtimerendering.com/s2015/aaltonenhaar_siggraph2015_combined_final_footer_220dpi.pdf`

### 2.3 Residency tracking + streaming budgets

Un motor AAA gestiona > 1M recursos virtuales pero solo ~50-200k residentes en VRAM al mismo tiempo. La diferencia se cubre con **streaming**.

Pattern canonical (UE5, Frostbite, Decima):

```cpp
struct TextureEntry {
    TextureHandle       handle;          // RHI handle si residente, else invalid
    uint32_t            vram_bytes;      // tamaño total si full-res residente
    uint8_t             resident_mip;    // mip más grande cargado (0 = full res, 255 = not resident)
    uint8_t             requested_mip;   // deseado por frame (recalcula cada frame)
    float               screen_priority; // heurístico: bbox en screen × freq de uso
    uint64_t            last_used_frame;
};

class ResidencyManager {
    std::vector<TextureEntry> entries_;
    uint64_t vram_budget_bytes_;          // ej. 6 GB de VRAM reservados para streaming
    uint64_t current_usage_;
    RingBuffer<StreamRequest> upload_queue_;   // async upload queue
public:
    void tick(uint64_t frame) {
        // 1) calcular prioridades (típicamente async en job system)
        for (auto& e : entries_) {
            e.screen_priority = compute_screen_priority(e);
            e.requested_mip = mip_for_priority(e.screen_priority);
        }
        // 2) evict los de menor prioridad hasta entrar en budget
        while (current_usage_ > vram_budget_bytes_) {
            auto it = min_priority_element();
            evict_to_mip(*it, it->resident_mip + 1);
        }
        // 3) schedule uploads de los que necesitan más res
        for (auto& e : entries_) {
            if (e.requested_mip < e.resident_mip) {
                upload_queue_.push({ e.handle, e.requested_mip });
            }
        }
    }
};
```

**Números concretos** de producción:
- UE5 Virtual Texturing: 128×128 o 256×256 pixel pages, budget típico 4-8k pages residentes.
- Decima (Horizon Forbidden West): streaming budget 5-6 GB para texturas en PS5, 1-2 GB para meshes, 500 MB para audio.
- Tasas de upload: PS5 NVMe llega ~9 GB/s raw, ~17 GB/s con Kraken descompressor. PC con DirectStorage llega ~6-7 GB/s. El async upload queue puede realistamente mover 2-4 GB/s sin impactar GPU.
- RAGE (Rockstar GTA V/RDR2): "predictive streaming cone" — camera velocity dicta radio de pre-carga (ver síntesis round 2).

### 2.4 Async upload queues

Separar una queue de copy es obligatorio en Vulkan/D3D12 para no bloquear la graphics queue durante uploads. Pattern:

```cpp
class UploadQueue {
    VkQueue copy_queue_;              // dedicated transfer family
    VkCommandPool cmd_pool_;
    RingBuffer<uint8_t> staging_;     // host-visible coherent, ~64 MB
    Fence upload_done_;
public:
    uint64_t enqueue_copy(TextureHandle dst, Span<const uint8_t> src, uint8_t mip) {
        // 1) memcpy src al staging ring
        size_t offset = staging_.alloc(src.size());
        memcpy(staging_.data() + offset, src.data(), src.size());
        // 2) record command: vkCmdCopyBufferToImage
        VkCommandBuffer cb = acquire_cmd_();
        vkCmdCopyBufferToImage(cb, staging_.buffer(), get_image(dst), ..., &region);
        // 3) submit a copy queue, signal nuestro timeline
        vkQueueSubmit(copy_queue_, 1, &submit, VK_NULL_HANDLE);
        return upload_done_.signal_next();
    }
};
```

La graphics queue antes de consumir el texture hace `timeline.gpu_wait(upload_value)` y automáticamente se sincroniza con el upload. Pattern directo de Frostbite (Hector, SIGGRAPH 2009).

Referencias:
- Virtual Textures r3 de ALZE: `r3/virtual_textures_streaming.md`
- DirectStorage: `https://devblogs.microsoft.com/directx/directstorage-api-downloads/`
- PS5 IO stack: Cerny, Road to PS5 (GDC 2020 video)

### 2.5 Descriptor slot allocator

Para bindless en production, necesitas un allocator de slots en el descriptor heap (ej. para asignar slots nuevos cuando streaming mueve texturas). Pattern:

```cpp
class DescriptorSlotAlloc {
    uint32_t size_;
    std::vector<uint32_t> free_slots_;    // freelist LIFO
public:
    // reservar slot 0 como "null/debug-pink" para out-of-range reads
    DescriptorSlotAlloc(uint32_t size) : size_(size) {
        for (uint32_t i = size - 1; i >= 1; --i) free_slots_.push_back(i);
    }
    uint32_t alloc() {
        if (free_slots_.empty()) return 0;    // fallback a null/debug
        auto s = free_slots_.back(); free_slots_.pop_back();
        return s;
    }
    void free(uint32_t s) { if (s != 0) free_slots_.push_back(s); }
};
```

Budget típico: 16k-32k slots para texturas 2D, 2k para cubemaps, 8k para volume textures, 65k para SRV/UAV buffers. Para ALZE v2 es suficiente un heap único de 16k (fit en Resource Binding Tier 2; Tier 3 requerido para unbounded descriptor tables).

NVIDIA cap práctico: "no exceed 1M active descriptors y 2k samplers total" (ver Advanced API Performance: Descriptors blog).

---

## 3. Renderer frontend

### 3.1 Sort-key 64-bit layout exacto

El sort-key es la heurística que agrupa draws para minimizar cambios de estado. Todo motor sort-key (bgfx, early UE4, Doom 2016 pre-Eternal) tiene el mismo layout conceptual. El reto es ordenar los campos para que la radix sort 1-pass sea correcta.

**Layout propuesto para ALZE (64 bits):**

```
 63                                             0
 | 60 57       52 46      28     10   0
 ┌──┬──────────┬──┬────────┬──────┬───┐
 │V │ Pass     │B │ Depth  │ Mat  │ R │
 │4 │  5       │2 │  18    │  18  │15 │
 └──┴──────────┴──┴────────┴──────┴───┘

V=view(4): hasta 16 views (main, shadow cascades 0-3, reflections, UI, etc.)
Pass(5): 32 passes dentro de un view (shadow, gbuffer, decals, forward, transparency, post, ...)
B(2): blend class — OPAQUE(0) / MASKED(1) / ADDITIVE(2) / BLEND_PREMUL(3)
Depth(18): 262144 buckets de profundidad (front-to-back OPAQUE, back-to-front BLEND)
Mat(18): 262144 materiales únicos por frame (PSO + texture set)
R(15): flags + remaining (e.g. user bits)
```

Justificaciones por campo:

- **View primero (MSB)**: lista sorted se particiona por view — permite "range por view" para dispatch paralelo a diferentes render targets.
- **Pass segundo**: dentro de un view, passes deben ser consecutivos (shadow antes de gbuffer antes de forward).
- **Blend tercero**: opacos primero (front-to-back para early-Z), después alpha-masked, finalmente blended (back-to-front).
- **Depth**: para opacos, menor depth primero → early-Z aprovecha hasta 95% de pixels. Para blended, invertir (no meter en el mismo key, o usar OR con 0x3FFFF - depth).
- **Material**: drawcalls del mismo material consecutivos → no hay que cambiar PSO/textures.
- **Reserved**: instance id para batching compatible, o user bits para debug.

**Por qué NO 128 bits**: radix sort de 64-bit es 8 passes de 1 byte cada una (o 4 passes de 16 bits en wide radix). 128-bit dobla eso. Y los 64 bits cubren cómodamente un millón de draws/frame con buckets suficientes.

Pseudocode del radix sort:

```cpp
void radix_sort_u64(uint64_t* keys, uint32_t n, uint64_t* scratch) {
    constexpr int kShift = 16;  // 16 bits per pass, 4 passes
    std::array<uint32_t, 65536> counts;
    for (int pass = 0; pass < 4; ++pass) {
        counts.fill(0);
        for (uint32_t i = 0; i < n; ++i) counts[(keys[i] >> (pass*kShift)) & 0xFFFF]++;
        // prefix sum
        for (uint32_t i = 1; i < 65536; ++i) counts[i] += counts[i-1];
        // scatter (right-to-left para stable)
        for (int i = (int)n-1; i >= 0; --i) {
            uint64_t k = keys[i];
            uint32_t bucket = (k >> (pass*kShift)) & 0xFFFF;
            scratch[--counts[bucket]] = k;
        }
        std::swap(keys, scratch);
    }
}
```

Medido: ~3-5 ns por key por pass × 4 passes = 12-20 ns/key. Para 50k draws/frame = 0.6-1.0 ms (1 core). Paralelizable por rangos si hace falta.

### 3.2 Scene submission & view system

El "view" es la unidad de independent camera/render target. Cada frame emite N views (main camera, shadow cascades 4×, reflection probes, post processing). Pattern:

```cpp
struct ViewState {
    Mat4 view, proj, view_proj, inv_view_proj;
    Vec3 camera_pos;
    Plane frustum_planes[6];
    TextureHandle render_target[8];      // MRTs
    TextureHandle depth_target;
    uint32_t viewport_w, viewport_h;
    uint8_t view_id;
};

class SceneRenderer {
    std::vector<ViewState> views_per_frame_;
    std::vector<Renderable> scene_;
public:
    void render() {
        // 1) cull per view (paralelo)
        parallel_for(views_per_frame_, [&](ViewState& v) {
            cull_(v, scene_);
        });
        // 2) build drawcalls per view (paralelo)
        parallel_for(views_per_frame_, [&](ViewState& v) {
            build_drawcalls_(v);
        });
        // 3) sort + dispatch (serial main thread)
        for (auto& v : views_per_frame_) dispatch_(v);
    }
};
```

### 3.3 Culling — frustum + occlusion

**Frustum culling** es barato: 6 plane-vs-AABB dot products. SIMD de 4 lanes procesa 4 bboxes/vez. Measured ~2-3 ns/object con SSE4.1. Para 100k objetos = 0.2-0.3 ms en 1 thread.

**Occlusion culling** es el siguiente tier. Tres familias:

| Técnica | Latencia | Complejidad | Ejemplo |
|---|---|---|---|
| Hardware occlusion queries | 1 frame lag | Baja | Legacy DX9-era |
| CPU SW-rasterized occlusion | 0 lag | Media (ej. Intel's MaskedOcclusionCulling) | Spidermilan 2008, Frostbite ~2015 |
| GPU Hi-Z occlusion | 0 lag | Media (compute + mip pyramid) | UE5 Nanite, Decima |

ALZE en v1 debería usar solo frustum. En v2 con frame graph → Hi-Z GPU-driven (ver `r3/nanite.md`).

**Occlusion GPU Hi-Z pseudocódigo:**

```cpp
// CPU: build Hi-Z pyramid from previous frame's depth
for (int mip = 1; mip < num_mips; ++mip) {
    dispatch_compute_downsample(mip-1, mip);    // min de 2x2 vecinos
}
// GPU compute culling shader:
bool is_visible(uint object_id) {
    AABB bbox = objects[object_id].bbox;
    vec2 min_uv, max_uv; float min_z;
    project_bbox(bbox, view_proj, min_uv, max_uv, min_z);
    int mip_level = mip_for_extent(max_uv - min_uv);
    float hiz_depth = sample_hiz(min_uv, max_uv, mip_level);
    return min_z <= hiz_depth + kEpsilon;
}
```

Horizon Zero Dawn (Guerrilla, De Smedt SIGGRAPH 2015) midió Hi-Z cullea ~40% de scene geometry en un 0.2 ms compute dispatch. UE5 Nanite lleva eso a cluster granularity (128 triángulos).

### 3.4 Material system

Un material en un motor moderno es **(ShaderProgram, ParameterBlock)**. El ShaderProgram es una PSO. El ParameterBlock es un conjunto de texturas + valores uniformes + samplers, típicamente empacado en un UBO 256-byte-aligned.

Pattern de parameter layout (mimicking Filament + bgfx):

```cpp
// Escrito por el material DSL author:
material "Iron" {
    shader: "pbr_opaque";
    params {
        albedo_tex:     texture2D = "textures/iron_albedo.ktx2";
        normal_tex:     texture2D = "textures/iron_normal.ktx2";
        mr_tex:         texture2D = "textures/iron_mr.ktx2";
        base_color:     float3 = (1.0, 1.0, 1.0);
        metallic:       float  = 1.0;
        roughness:      float  = 0.35;
    }
}

// Compile-time: el material compiler emite:
struct IronParams {
    uint32_t albedo_idx, normal_idx, mr_idx;   // descriptor heap indices (v2+)
    float    base_color[3];
    float    metallic, roughness;
    uint8_t  _pad[12];                          // 256-byte alignment
};

// Runtime: bindless v2 → el shader hace materials[pc.material_id]
```

### 3.5 LOD selection

LOD switching canónico es por distance (screen-space error). Threshold típico:

```cpp
float screen_space_error(Renderable& r, ViewState& v) {
    float dist = length(r.pos - v.camera_pos);
    float pixel_size_at_dist = dist * v.tan_half_fovy * 2.0f / v.viewport_h;
    return r.mesh.bounding_radius / pixel_size_at_dist;
}

int select_lod(Renderable& r, ViewState& v) {
    float sse = screen_space_error(r, v);
    // Tabla LOD: error in pixels para cada nivel
    // LOD0: >80 px, LOD1: >40 px, LOD2: >20 px, LOD3: >10 px, cull: <2 px
    if (sse < 2.0f)  return -1;   // cull
    if (sse > 80)    return 0;
    if (sse > 40)    return 1;
    if (sse > 20)    return 2;
    return 3;
}
```

**LOD con dithered crossfade** evita popping. Entre LOD N y N+1, 80-120 px de SSE: render ambos con alpha-to-coverage dithered. Pattern de Frostbite y UE4+.

Nanite (UE5) transforma esto en **cluster-level LOD**: cada cluster (128 tri) tiene un error métrico geométrico y se selecciona continuamente por compute shader. Ver `r3/nanite.md` para profundidad.

Referencias:
- bgfx sort-key: `https://bkaradzic.github.io/bgfx/bgfx.html` (internals page)
- Doom 2016 graphics study: `https://www.adriancourreges.com/blog/2016/09/09/doom-2016-graphics-study/` (Courrèges)
- Doom Eternal study: `https://simoncoenen.com/blog/programming/graphics/DoomEternalStudy` (Coenen, 2020)
- Horizon Zero Dawn GPU-driven rendering: De Smedt & Van Den Berghe, SIGGRAPH 2015

---

## 4. Frame Graph / Render dependency graph

### 4.1 Recap y qué añadir al r3

El archivo `r3/frame_graph_bindless.md` cubrió 430 líneas sobre la teoría, O'Donnell GDC 2017, UE5 RDG, Halcyon, AMD FFX. Aquí se agrega el **pseudo-código C++17 end-to-end** que se puede copiar a ALZE y el detalle de aliasing.

### 4.2 API superficie canónica (setup/compile/execute)

```cpp
// Virtual resource handle — NO alloca memoria real, solo describe
struct FgTexture { uint32_t virtual_id; uint32_t version; };
struct FgBuffer  { uint32_t virtual_id; uint32_t version; };

class FrameGraph {
public:
    // === Setup phase ===
    template<typename Data, typename Setup, typename Execute>
    void add_pass(const char* name, Setup setup, Execute exec);

    // Builder pasado al setup lambda:
    class Builder {
        FgTexture create(const char* name, const TextureDesc&);
        FgTexture read(FgTexture t, ResourceUsage u);        // devuelve el MISMO handle
        FgTexture write(FgTexture t, ResourceUsage u);       // devuelve NUEVA version (move-sem)
        void import_external(FgTexture t, TextureHandle actual);
    };

    // === Compile phase ===
    void compile();   // walks DAG: cull, alias, inserta barriers, asigna queues

    // === Execute phase ===
    void execute(CommandEncoder& enc);
};
```

El `write()` devolviendo una nueva version es el truco de O'Donnell: evita ciclos en el DAG cuando un pass lee-modifica el mismo resource.

### 4.3 Transient aliasing — algoritmo de interval coloring

El algoritmo es literalmente interval scheduling con bin packing. Cada virtual resource tiene `[first_use_pass, last_use_pass]`. Dos resources pueden compartir memoria física si sus intervalos no se solapan Y sus desc son compatibles (format + size compatibles — en la práctica se redondea a "tienen al menos `max(size_a, size_b)` bytes de memoria igualmente alineada").

```cpp
struct VirtualResource {
    uint32_t first_pass, last_pass;
    uint64_t size_bytes;
    uint32_t alignment;
    uint32_t physical_slot = UINT32_MAX;    // output
};

void assign_physical_slots(std::vector<VirtualResource>& virts) {
    // Sort by first_pass ascending
    std::sort(virts.begin(), virts.end(),
              [](auto& a, auto& b){ return a.first_pass < b.first_pass; });
    struct PhysSlot { uint64_t size; uint32_t free_after; };
    std::vector<PhysSlot> phys;
    for (auto& v : virts) {
        // Busca slot libre (free_after <= v.first_pass) cuyo size >= v.size
        int best = -1;
        for (size_t i = 0; i < phys.size(); ++i) {
            if (phys[i].free_after <= v.first_pass && phys[i].size >= v.size_bytes) {
                if (best < 0 || phys[i].size < phys[best].size) best = (int)i;
            }
        }
        if (best < 0) {
            best = (int)phys.size();
            phys.push_back({ v.size_bytes, 0 });
        }
        v.physical_slot = (uint32_t)best;
        phys[best].free_after = v.last_pass + 1;
        phys[best].size = std::max(phys[best].size, v.size_bytes);
    }
}
```

**Savings measurados**:
- Frostbite (O'Donnell GDC 2017): ~50% reducción en transient RT memory en Battlefield 1.
- UE5 RDG: 30-50% savings típicos dependiendo de scene.
- AMD FFX Frame Graph: 20-40% para integraciones vanilla.

### 4.4 Barrier insertion

Para cada edge (P produce, C consume) en el DAG, la compile phase emite un barrier:

```cpp
struct Barrier {
    uint32_t pass_index;          // en qué pass se emite
    VkImageLayout old_layout, new_layout;
    VkPipelineStageFlags2 src_stage, dst_stage;
    VkAccessFlags2 src_access, dst_access;
    VkImage image;
    VkImageSubresourceRange range;
};

for (auto& edge : dag_edges) {
    Pass& p = passes[edge.producer];
    Pass& c = passes[edge.consumer];
    Resource& r = resources[edge.resource];
    barriers.push_back({
        .pass_index = c.index,   // just before consumer
        .old_layout = layout_for_usage(r, edge.producer_usage),
        .new_layout = layout_for_usage(r, edge.consumer_usage),
        .src_stage  = stage_for_usage(edge.producer_usage),
        .dst_stage  = stage_for_usage(edge.consumer_usage),
        .src_access = access_for_usage(edge.producer_usage),
        .dst_access = access_for_usage(edge.consumer_usage),
        .image = r.physical_image,
        .range = r.subresource_range,
    });
}
```

### 4.5 Async compute fork/join

La compile phase también asigna cada pass a una queue:

```cpp
enum class QueueLane : uint8_t { Graphics, Compute, Transfer };

void assign_queues() {
    for (auto& p : passes) {
        if (p.has_rasterization)           p.queue = QueueLane::Graphics;
        else if (p.flags & AsyncCompute)   p.queue = QueueLane::Compute;
        else if (p.flags & CopyOnly)       p.queue = QueueLane::Transfer;
        else                                p.queue = QueueLane::Graphics; // default
    }
    // Cross-queue dependencies → timeline semaphore signal/wait
    for (auto& edge : dag_edges) {
        if (queue_of(edge.producer) != queue_of(edge.consumer)) {
            emit_timeline_signal(edge.producer, current_counter + 1);
            emit_timeline_wait(edge.consumer, current_counter + 1);
        }
    }
}
```

**Regla**: no hacer async compute en passes < 100 μs — el cost del semaphore domina. O'Donnell sugiere > 200 μs como threshold seguro. Ver `nvpro-samples/vk_timeline_semaphore` para benchmarks concretos.

Referencias (ya citadas en r3):
- O'Donnell, "FrameGraph" GDC 2017: slides en `https://gpuopen.com/wp-content/uploads/slides/GDC_2017_FrameGraph.pdf`
- Wihlidal (EA SEED), Halcyon SIGGRAPH 2018: `https://www.ea.com/seed/news/siggraph-2018-halcyon-architecture`
- UE5 RDG docs: `https://dev.epicgames.com/documentation/en-us/unreal-engine/render-dependency-graph`
- Šmejkal, "Aliasing transient textures in DirectX 12": `https://pavelsmejkal.net/Posts/TransientResourceManagement`
- GPUOpen D3D12MemoryAllocator aliasing: `https://gpuopen-librariesandsdks.github.io/D3D12MemoryAllocator/html/resource_aliasing.html`

---

## 5. Draw submission — GPU-driven, bindless, multi-queue

### 5.1 Multi-threaded command list building

En un motor N-thread, cada thread graba su propio `VkCommandBuffer` / `ID3D12GraphicsCommandList` sobre un subset de draws. Al final del frame, el main thread los "concatena" vía submit.

```cpp
// Job: render view V, range [first, last) de drawcalls sorted
void render_job(ViewState& v, Span<DrawCall> draws, ThreadContext& tc) {
    VkCommandBuffer cb = tc.acquire_secondary_cb();    // VK_COMMAND_BUFFER_LEVEL_SECONDARY
    vkCmdBeginRendering(cb, &v.render_info);           // VK 1.3 dynamic rendering
    for (auto& dc : draws) {
        if (dc.pso != current_pso) {
            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, dc.pso);
            current_pso = dc.pso;
        }
        vkCmdPushConstants(cb, layout, VK_SHADER_STAGE_ALL, 0, sizeof(DrawData), &dc.data);
        vkCmdBindIndexBuffer(cb, dc.ibuf, 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(cb, dc.index_count, 1, 0, 0, 0);
    }
    vkCmdEndRendering(cb);
    tc.submit_secondary(cb);
}
```

Parallelism tradeoff: N threads es mejor para 10k+ drawcalls. < 1k drawcalls es contraproducente (cost de cb submit domina). Frostbite empieza a paralelizar en 2k draws umbral.

### 5.2 Bindless materials en la práctica

La combo canonical (Doom Eternal es el poster-child — SIGGRAPH 2020 Geffroy et al.):

```glsl
// shader.vert / .frag
layout(set = 0, binding = 0) uniform texture2D textures[];          // 16k slots
layout(set = 0, binding = 1) uniform samplerState samplers[];       //  4k
layout(set = 1, binding = 0) readonly buffer Materials { MaterialRecord mats[]; };
layout(push_constant) uniform PC { uint material_id; uint xform_id; };

void main() {
    MaterialRecord m = mats[material_id];
    vec4 albedo = texture(
        sampler2D(textures[nonuniformEXT(m.albedo_idx)], samplers[0]),
        uv) * m.base_color_factor;
    // ...
}
```

**CRÍTICO**: `nonuniformEXT` (GLSL) / `NonUniformResourceIndex` (HLSL) cuando el índice varía por-invocation/wavefront. Sin eso, el driver emite indexing uniforme (más barato) pero el resultado puede ser indefinido si dos invocations del mismo warp indexan diferentemente. NVIDIA's Advanced API Performance: Descriptors blog detalla.

### 5.3 GPU-driven rendering — pipeline completo

```
┌────────────────────┐   ┌────────────────────┐   ┌────────────────────┐
│ CPU: prep scene    │   │ GPU: cull compute  │   │ GPU: meshlet raster│
│  upload SceneBuf   │──>│  - frustum         │──>│  vkCmdDrawMeshTasks│
│  upload BVH       │   │  - occlusion Hi-Z  │   │  or indirect       │
└────────────────────┘   │  - LOD select      │   └────────────────────┘
                          │  - meshlet cull    │
                          │  write IndirectBuf │
                          └────────────────────┘
```

Pseudocódigo del culling compute:

```glsl
// cull.comp — 64 threads por workgroup, uno por object
layout(local_size_x = 64) in;
layout(set=0, binding=0) readonly buffer Scene { ObjectInstance objs[]; };
layout(set=0, binding=1) writeonly buffer DrawCmds { VkDrawIndexedIndirectCommand cmds[]; };
layout(set=0, binding=2) buffer Counter { uint count; };

void main() {
    uint id = gl_GlobalInvocationID.x;
    if (id >= objs_total) return;
    ObjectInstance o = objs[id];
    if (!frustum_cull(o.bbox)) return;
    if (!hiz_cull(o.bbox)) return;
    int lod = select_lod(o);
    uint idx = atomicAdd(count, 1);
    cmds[idx] = make_draw_cmd(o, lod);
}
```

CPU luego hace:

```cpp
vkCmdDrawIndexedIndirectCount(
    cb,
    indirect_buffer, /*offset=*/ 0,
    counter_buffer, /*count_offset=*/ 0,
    /*max_draws=*/ MAX_OBJECTS,
    sizeof(VkDrawIndexedIndirectCommand));
```

**1M objects culled en < 0.5 ms** reportado por Aaltonen (GDC 2015) y reproducible con `vkguide.dev/docs/gpudriven/compute_culling/`.

### 5.4 Mesh shaders + meshlets

NVIDIA Turing+, AMD RDNA2+, Intel Arc soportan mesh shaders. Reemplaza VS + fixed tessellation con un compute-like shader que outputea meshlets (clusters de ~64 vértices + ~126 tri).

```cpp
// Offline: meshoptimizer
meshopt_buildMeshlets(
    meshlets_out,         // output
    meshlet_verts_out,
    meshlet_tris_out,
    indices, index_count,
    vertices, vertex_count, sizeof(Vertex),
    /*max_vertices=*/64,
    /*max_triangles=*/124,   // rec NVIDIA
    /*cone_weight=*/0.0f);
```

Parámetros:
- NVIDIA: 64 vertices, 126 triangles
- AMD RDNA2: 64/64 o 128/128
- Intel Xe2: 128/128

Ver `r3/mesh_shaders_work_graphs.md` y `https://meshoptimizer.org/` (Kapoulkine).

### 5.5 Multi-queue (graphics + compute + transfer)

Patrón típico en Decima/Doom/UE5:

| Frame segment | Graphics Q | Compute Q | Transfer Q |
|---|---|---|---|
| Frame N begin | shadow cascades | BVH refit | upload textures |
| Early frame | gbuffer main | GTAO, clustered lighting | — |
| Mid frame | forward lighting | Bloom down, DOF | mipchain gen |
| Late frame | transparency, UI | Bloom up, tonemap, TAA | readback for next frame |
| Frame end | present | — | — |

Referencias:
- Doom Eternal SIGGRAPH 2020: `https://advances.realtimerendering.com/s2020/RenderingDoomEternal.pdf`
- vkguide GPU-driven: `https://vkguide.dev/docs/gpudriven/`
- meshoptimizer: `https://github.com/zeux/meshoptimizer`

---

## 6. Shading model infrastructure

### 6.1 Toolchain canonical 2026: HLSL → dxc → SPIR-V → SPIRV-Cross → GLSL/MSL

La industria convergió (2020-2026) a este stack:

```
  material.hlsl
       │
       ▼
  ┌────────┐    ┌─────────┐    ┌──────────────┐    ┌──────────┐
  │  dxc   │───>│ SPIR-V  │───>│ SPIRV-Cross  │───>│ GLSL 330 │
  │ -spirv │    │ binary  │    │              │    │ MSL 2.3  │
  └────────┘    └─────────┘    └──────────────┘    │ HLSL-dxb │
       │             │                               └──────────┘
       │             └────────────────> Vulkan backend (direct)
       │
       └─────────────────────────────> dxbc (D3D12 fxc path, legacy)
```

**Slang** (shader-slang/slang) es la emergencia 2024-2026 que compite: Slang → HLSL, GLSL, SPIR-V, MSL, CUDA, WGSL desde una source. Ventaja: generics y módulos nativos (reduce permutation explosion). Desventaja: dependency chain más pesada que dxc standalone. Para un motor indie (ALZE) pragmatismo dice: **dxc + SPIRV-Cross es suficiente y super-probado; Slang es opcional si necesitas generics/modules después**.

### 6.2 Shader permutation management — el escenario peor

Un motor que no gestiona permutaciones explota:
- Material con 8 features booleanas = 256 permutaciones
- × 16 render passes = 4096 permutaciones por material
- × 50 materiales = 200k PSOs
- × 3 plataformas = 600k PSOs en disco

**UE4 legacy shipping**: ~100-500k PSOs, cada uno 50-500 KB = 25-250 GB de DDC.

### 6.3 PSO cache + precache (el fix del UE5 stutter)

El stutter de UE se debía a **lazy PSO creation**: la primera vez que un material se renderiza, el driver compila la PSO → 50-200 ms hitch.

UE5.2+ introdujo **PSO precaching** (`r.PSOPrecaching.Enable=1`): al cargar un map, el engine enumera todos los materiales usados × pass flags × mesh types y llama `PipelineStateCache::GetAndOrCreatePSO()` asincronicamente. Para cuando el frame se renderiza, las PSOs ya están compiladas.

Pattern para ALZE:

```cpp
struct PsoCacheKey {
    uint64_t shader_hash;       // hash of (vs_bin, fs_bin)
    uint32_t vertex_layout_id;
    uint32_t blend_state_id;
    uint32_t depth_state_id;
    uint32_t rasterizer_id;
    uint8_t  rt_count;
    uint8_t  rt_formats[8];
};

class PsoCache {
    std::unordered_map<PsoCacheKey, PipelineHandle, Hash> map_;
    std::mutex mtx_;
public:
    PipelineHandle get_or_create(const PsoCacheKey& k, const PipelineDesc& d) {
        std::lock_guard g(mtx_);
        auto it = map_.find(k);
        if (it != map_.end()) return it->second;
        auto h = rhi_.create_pipeline(d);    // puede ser 50-200 ms en driver
        map_.emplace(k, h);
        return h;
    }
    void precache_async(Span<const PipelineDesc> descs);   // pushed a job system
};
```

**Disk cache**: serialize `PsoCacheKey → pipeline_binary_blob` a `pso_cache.bin`. Al arranque, rehydrate. VK expone `VkPipelineCache`, D3D12 expone `ID3D12PipelineLibrary`. Bypass completo de driver optimizer en el 2o arranque.

Referencias:
- UE5 PSO precaching docs: `https://dev.epicgames.com/documentation/en-us/unreal-engine/pso-precaching-for-unreal-engine`
- Epic blog "Game engines and shader stuttering": `https://www.unrealengine.com/tech-blog/game-engines-and-shader-stuttering-unreal-engines-solution-to-the-problem`
- PSO caching UE5 deep dive: `https://blog.rime.red/pso-caching-ue5-pt2/`
- Slang: `https://shader-slang.org/` (Fuller & He, 2018+, adopted NVIDIA+Khronos 2024)
- SPIR-V Cross: `https://github.com/KhronosGroup/SPIRV-Cross`

### 6.4 Uniform/push constant layout

Push constants son 128 bytes guaranteed en Vulkan (256 bytes en hw moderno). Layout propuesto para ALZE:

```cpp
struct PerDrawPushConstants {
    uint32_t material_id;        // 4
    uint32_t transform_id;       // 4
    uint32_t skin_id;            // 4 (-1 if not skinned)
    uint32_t instance_offset;    // 4
    uint32_t user_flags;         // 4
    uint32_t _pad[3];            // 12 bytes padding hasta 32
};                               // total 32 bytes
```

UBOs mayores (camera, lights, probe data) viven en descriptor set 0, bound una vez por view.

---

## 7. Post-processing pipeline

### 7.1 Full-screen pass management

Un post-proc pass es un compute dispatch que lee scene color + depth + motion vectors y escribe a un output. Pattern:

```cpp
class PostChain {
    FgTexture scene_color, scene_depth, motion_vectors;
    FgTexture bloom_chain[6];     // mip chain
    FgTexture tonemapped;
    FgTexture final_swapchain;
public:
    void build(FrameGraph& fg) {
        // 1) bloom downsample chain
        for (int i = 0; i < 6; ++i) {
            fg.add_pass("bloom_down_" + std::to_string(i), [&](auto& b) {
                bloom_chain[i] = b.create({ w>>(i+1), h>>(i+1), RGBA16F });
                b.read(i == 0 ? scene_color : bloom_chain[i-1]);
                b.write(bloom_chain[i]);
            }, compute_dispatch_downsample);
        }
        // 2) bloom upsample (reverso)
        // 3) tonemap
        // 4) TAA
        // 5) final output
    }
};
```

### 7.2 TAA — jitter + motion vectors + history

TAA requiere:
1. **Jitter camera per-frame**: `proj[2][0] += halton(frame_idx).x / width; proj[2][1] += halton(frame_idx).y / height`. Halton(2,3) es el canonical sequence.
2. **Motion vectors**: per-pixel. Se genera durante gbuffer pass: `motion_vec = prev_clip_pos.xy - curr_clip_pos.xy`.
3. **Reproject history**: muestrea el frame anterior en la posición `uv + motion_vec`.
4. **Blend**: `output = mix(history, current, alpha)`, alpha ~0.1 estático, ~0.2 móvil.
5. **Ghosting mitigation**: Neighborhood clipping (Salvi 2016). Clip history al AABB de 3x3 vecinos del current frame.

El `r3/neural_rendering.md` probablemente cubre DLSS/FSR/XeSS como integración superior.

### 7.3 Upscaling — DLSS/FSR/XeSS integration points

Todas las 3 APIs consumen los MISMOS inputs:
- Pre-upscale jittered scene color (LDR or HDR, format-dependent)
- Depth (non-linear clip Z)
- Motion vectors (2D screen-space)
- Exposure (scalar)
- Optional: pre-exposed UI, reactive mask, transparency composition mask

Integration points en el frame graph:

```
[gbuffer] → [lighting] → [TAA off] → [DLSS/FSR/XeSS] → [post-process at output res]
                    ↘      ↘
                      motion   depth
                        vecs
```

DLSS 3.5 Ray Reconstruction añade un input extra: la signal de ray-tracing pre-denoising (NVIDIA hace su propio "denoiser AI" que reemplaza a denoisers clásicos). FSR 3.1 (2024) separó frame generation del upscaling, permitiendo usar FSR-FG con DLSS-SR.

Fuentes:
- Intel XeSS dev guide: `https://www.intel.com/content/www/us/en/developer/articles/technical/xess-sr-developer-guide.html`
- AMD FSR SDK: `https://gpuopen.com/fidelityfx-sdk/` (GPUOpen)
- NVIDIA DLSS SDK: `https://github.com/NVIDIA/DLSS`
- FSR 3.1 decoupled frame gen: `https://wccftech.com/amd-fidelityfx-sdk-v1-1-fsr-3-1-support-enhanced-upscaling-quality-decoupled-frame-generation-dlss-xess/`

### 7.4 Tonemapping

Tonemappers canonical shipping en producción:
- **ACES Filmic** (Narkowicz 2015, approximation) — de facto en cine y juegos AAA
- **Uncharted 2** (Hable 2010) — legacy Frostbite/UE3
- **Reinhard extended** — indie-friendly simple
- **AgX** (Troy Sobotka 2024) — moderno, físicamente motivado, adoptado por Blender 4.0

```glsl
vec3 aces_filmic(vec3 x) {
    const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    return clamp((x*(a*x+b)) / (x*(c*x+d)+e), 0.0, 1.0);
}
```

---

## 8. Build/bake pipeline

### 8.1 Asset conditioning

Cada asset type se "cocina" offline para el formato runtime óptimo:

| Source | Runtime | Herramienta | Tiempo (asset típico) |
|---|---|---|---|
| `.png 4k RGBA` | `.ktx2` BC7 mipmapped | KTX-Software, Basis Universal | 1-5 s |
| `.obj / .fbx / .gltf` | binary meshlet-ized | meshoptimizer, custom | 0.1-2 s |
| `.hlsl` material + variants | PSO binary blob | dxc + cache | 50-500 ms × N variants |
| `.wav` | `.opus` / `.qoa` | opus-tools, QOA ref | 0.5-5 s |
| `.blend` collections | custom scene binary | bevy_asset-like | 0.5-2 s |

### 8.2 Texture compression

Target formats:
- **BC7** (8 bpp) para desktop discrete GPUs — balance de calidad/velocidad.
- **ASTC 4x4** (8 bpp) para mobile / Apple silicon.
- **BC6H** HDR para cubemaps HDR.
- **BC5** (RG 8 bpp) para normal maps.
- **BC4** (R 4 bpp) para single-channel roughness/metallic.

Basis Universal transcoder convierte de un "universal" intermediate a cualquiera de los anteriores al cargar, sin pagar encode cost repetido por plataforma.

Números (Pranckevičius 2020 blog + Geldreich 2019):
- BC7 encode: ~50-500 ms / 4K texture con rgbcx (fast mode). ~5-30 s con bc7enc high-quality.
- ASTC encode: ~1-10 s / 4K texture con astc-encoder fast. 30-300 s exhaustive.
- BC7 vs ASTC: misma PSNR (42+ dB) a misma bpp, BC7 compresa 3-10x más rápido.
- Basis transcode: ~10-50 ms / 4K texture al cargar. GPU transcoder (Tellusim) lo baja a < 5 ms.

### 8.3 Mesh preprocessing

```cpp
// Pseudo pipeline:
// 1) import (FBX/glTF/USD → native in-memory mesh)
// 2) meshopt_optimizeVertexCache (improve ACMR)
// 3) meshopt_optimizeOverdraw (front-to-back ordering)
// 4) meshopt_optimizeVertexFetch (cache layout)
// 5) meshopt_simplify (LOD1, LOD2, LOD3 with error thresholds 0.01, 0.05, 0.15)
// 6) meshopt_buildMeshlets (clusters for GPU-driven)
// 7) emit binary: header + vertices[] + indices[] + lod_info[] + meshlets[]
```

Tiempos: 100k-vertex mesh, optimización completa: ~100-500 ms offline. Meshlet build: ~10-50 ms adicional. Todo es CPU, paralelizable por asset.

### 8.4 Derived Data Cache (DDC)

Pattern UE5 generalizable:

```
content/*.source → hash(source) → cache-key → cached-cooked.bin
```

Tres tiers jerárquicos:
1. **Local DDC** (`%LOCALAPPDATA%/alze/ddc/`): primer look, ~10 GB límite.
2. **Shared DDC** (network SMB/NFS): team-wide, ~500 GB-5 TB.
3. **Cloud DDC** (S3/Azure Blob): fallback + cross-project, TB+.

Key derivation (content-addressable):

```cpp
uint64_t compute_cache_key(const AssetSource& src) {
    Hasher h;
    h.update(src.blake3_hash);           // source content
    h.update(src.version);
    h.update(PLATFORM_ID);               // cook target
    h.update(COOKER_VERSION);            // compiler version change → invalidate
    h.update(src.settings_hash);         // compression options
    return h.finalize();
}
```

DDC hits evitan recook — Epic reporta 10-100x speedup en cold rebuild post-DDC warm. Ver `https://dev.epicgames.com/documentation/en-us/unreal-engine/using-derived-data-cache-in-unreal-engine`.

### 8.5 Cook-for-platform vs generic

Dos filosofías:

- **Generic runtime transcode** (Basis Universal, SPIRV-Cross runtime): un asset universal, transcodea al cargar. Pros: 1 binary per asset cross-platform; Cons: transcode cost cada arranque.
- **Cook per platform** (UE5, Frostbite): genera `platform_PC/`, `platform_PS5/`, `platform_XboxSeries/`. Pros: zero-cost runtime; Cons: 3-5x disk overhead + build time.

**Para ALZE**: cook-per-platform para shaders (PSO binaries son platform-specific); generic Basis para textures; binary-compatible para meshes/audio.

Referencias:
- Pranckevičius texture compression 2020: `https://aras-p.info/blog/2020/12/08/Texture-Compression-in-2020/`
- Pranckevičius BCn decoders: `https://aras-p.info/blog/2022/06/23/Comparing-BCn-texture-decoders/`
- Reed, BCn formats: `https://www.reedbeta.com/blog/understanding-bcn-texture-compression-formats/`
- Geldreich "Unified texture encoder BC7/ASTC": `http://richg42.blogspot.com/2019/09/unified-texture-encoder-for-bc7-and.html`
- UE5 DDC: `https://dev.epicgames.com/documentation/en-us/unreal-engine/using-derived-data-cache-in-unreal-engine`
- Basis Universal: `https://github.com/BinomialLLC/basis_universal`

---

## 9. Debug/Tools infrastructure

### 9.1 RenderDoc integration

RenderDoc 1.27+ soporta:
- Vulkan, D3D11/12, OpenGL, Metal (experimental)
- Bindless descriptor debugging (click-through desde draw a textures sampled)
- Per-event GPU timing vía GPU counters
- Shader debug (step through VS/PS/CS)

Integration para ALZE: añadir el **RenderDoc in-app API** (`renderdoc_app.h`):

```cpp
#include <renderdoc/renderdoc_app.h>
RENDERDOC_API_1_6_0* rdoc = nullptr;

void init_renderdoc() {
    if (auto lib = dlopen("librenderdoc.so", RTLD_NOW)) {
        auto get_api = (pRENDERDOC_GetAPI)dlsym(lib, "RENDERDOC_GetAPI");
        get_api(eRENDERDOC_API_Version_1_6_0, (void**)&rdoc);
    }
}

void maybe_capture_frame() {
    if (rdoc && key_pressed(F11)) {
        rdoc->TriggerCapture();     // next frame will be captured
    }
}
```

Docs: `https://renderdoc.org/docs/in_application_api.html`.

### 9.2 PIX markers — cross-API

PIX (Microsoft) es similar para D3D12 Xbox/PC. La abstracción correcta es añadir un "debug marker" API en el RHI:

```cpp
namespace alze::rhi {
    class IDevice {
        virtual void push_debug_marker(const char* name, uint32_t color) = 0;
        virtual void pop_debug_marker() = 0;
    };
}

// Backend impls:
// VK: vkCmdBeginDebugUtilsLabelEXT / vkCmdEndDebugUtilsLabelEXT (VK_EXT_debug_utils)
// D3D12: PIXBeginEvent / PIXEndEvent (pix3.h)
// Metal: MTLCommandEncoder::pushDebugGroup / popDebugGroup
// GL: glPushDebugGroup / glPopDebugGroup (GL 4.3+) or KHR_debug

#define ALZE_GPU_SCOPE(name) auto _marker = RhiScope(device, name, 0xFFFF8800)
```

### 9.3 Inline GPU timing

Tracy profiler + `TracyVkZone` / `TracyD3D12Zone` son el estándar de facto 2024-2026. Tracy emite markers a la UI cliente con microsecond precision + GPU timestamps automáticos.

```cpp
#include <tracy/TracyVulkan.hpp>
TracyVkCtx tracy_vk = TracyVkContext(phys_dev, dev, queue, cmd_buf);
// per-frame:
{
    TracyVkZone(tracy_vk, cmd, "GBuffer");
    render_gbuffer(cmd);
}
```

### 9.4 Visualization toggles

Debug modes canonical (build into renderer as cvar-switchable):
- Wireframe overdraw heatmap (grayscale → red on high overdraw)
- Per-material shader ID mapped a color
- Depth-buffer visualization
- Motion vectors visualization (R=dx, G=dy, B=0)
- LOD selection colormap (LOD0=red, LOD1=yellow, LOD2=green, LOD3=blue)
- Frustum cull misses (objects drawn outside frustum flagged)
- Light complexity (number of lights affecting tile)

Implement en 2 KB de shader + cvars. Invaluable durante debug.

### 9.5 Live shader reload

Pattern canonical:

```cpp
class ShaderWatcher {
    std::unordered_map<std::string, FileWatch> watches_;
    std::vector<std::function<void(const std::string&)>> callbacks_;
public:
    void tick() {
        for (auto& [path, watch] : watches_) {
            if (file_mtime(path) > watch.last_mtime) {
                watch.last_mtime = file_mtime(path);
                recompile_and_swap_(path);
            }
        }
    }
};
```

Recompile flow: HLSL → dxc → SPIR-V → new `VkShaderModule` + new `VkPipeline` → atomic swap de la PSO en cache. Iteration loop ~200-500 ms.

### 9.6 Memory dashboards

Expose runtime memory usage in ImGui overlay:
- Per-allocator (textures, buffers, PSOs, meshes)
- Per-resource-class high watermark
- Fragmentation % (for VMA-style allocators)
- Upload queue depth + bandwidth

Essential durante bring-up: sin dashboards, el motor es una caja negra.

Referencias:
- RenderDoc: `https://renderdoc.org/docs/` (Baldwin)
- Tracy: `https://github.com/wolfpld/tracy` (Krupa)
- PIX: `https://devblogs.microsoft.com/pix/`
- Dear ImGui: `https://github.com/ocornut/imgui`

---

## 10. Evolution path — v1 → v2 → v3

### 10.1 v1 — hoy, GL 3.3, sort-key, bindful

Target: ship el motor con feature parity a UE4 circa 2017. Scope:

- [x] RHI abstraction opaca (handles 64-bit, GL 3.3 backend único)
- [x] Sort-key 64-bit submission
- [x] Bindful descriptor model (GL3.3 nativo)
- [x] HLSL → dxc → SPIRV → SPIRV-Cross → GLSL330
- [x] PBR Filament-port GGX + split-sum IBL
- [x] Forward + deferred toggle
- [x] Texture BC7/ASTC offline bake → basis universal transcoder runtime
- [x] Meshlet-ready mesh format (NO mesh shaders uso en v1)
- [x] PSO cache disk-serialized (trivial en GL: cache de program objects)
- [x] RenderDoc + Tracy hook points
- [x] Live shader reload

**Coste estimado**: 10-14 semanas-persona (seniors). Un engineer FT, 3 meses. Este es el vertical slice ship-defensible.

**Qué commit afecta 10 años en v1**:
- El layout del sort-key (cambiarlo después obliga a revisar todo el frontend).
- El formato binario del handle (gen bits count).
- La layout del PipelineDesc (añadir campos luego rompe hashing del cache).

**Reversible en v1**:
- Elección de SPIRV-Cross vs Slang (el .hlsl input no cambia).
- Particular tonemapper.
- Forward vs deferred — se puede soportar ambos.
- Exactamente qué effects post están.

### 10.2 v2 — Vulkan, frame graph, bindless

Target: feature parity con DOOM Eternal 2020. Scope:

- [x] RHI backend Vulkan (mantener API opaca — zero game code change)
- [x] Frame Graph DAG (~2-3k LOC subset de O'Donnell pattern, ver r3)
- [x] Transient aliasing (interval coloring, 30-50% transient memory savings)
- [x] Bindless descriptor indexing (VK 1.2)
- [x] Async compute para post-FX
- [x] Timeline semaphores
- [x] GPU-driven culling compute + vkCmdDrawIndirectCount
- [x] Hi-Z occlusion pyramid
- [x] Multi-threaded secondary command buffer recording
- [x] PSO precache (antes de level load, todos los PSOs posibles)
- [x] DDC content-addressable (local + shared network)
- [x] DLSS + FSR + XeSS integration en post chain

**Coste**: 20-28 semanas-persona sobre v1. Un engineer FT, ~5-7 meses. Backend Vulkan es la mitad (~10 semanas); FG + bindless ~6 semanas; GPU-driven ~4 semanas; integrations upscaling ~4 semanas.

**Qué commit afecta 10 años en v2**:
- La API del FrameGraph (Builder + lambda contract). Cambios después rompen cada pass.
- El layout de MaterialRecord en bindless SSBO (cambios requieren re-cook de todos los materiales).
- El descriptor heap size (65k → 16k shrink requiere migración).

**Reversible en v2**:
- Qué passes son async compute (flag por pass).
- Exactamente qué estrategia de aliasing (first-fit, best-fit, priority-hinted).

### 10.3 v3 — AAA tier, mesh shaders, RT unified, work graphs

Target: feature parity con UE5 Nanite+Lumen en 2025. Scope:

- [x] Mesh shaders (VK_EXT_mesh_shader, DXR 1.1)
- [x] Nanite-like virtualized geometry (cluster hierarchy + SW raster compute)
- [x] Lumen-like unified GI (probes + SSGI + RT fallback)
- [x] Virtual textures (UE5 SVT pattern)
- [x] Work graphs (D3D12 1.715+, Vulkan `VK_NV_device_generated_commands_compute`)
- [x] Hardware ray-tracing pipeline (DXR / VK_KHR_ray_tracing)
- [x] Neural rendering hooks (DLSS Ray Reconstruction tier integration)
- [x] Visibility buffer rendering path

**Coste**: 60-100 semanas-persona. Esto es un team de 5-8 personas, 1-2 años. Es el tier donde un motor indie se diferencia solo de AAA por volumen humano, no capability.

**Commits que afectan 10 años en v3**:
- Adopción de mesh shaders como core pipeline (no retrofit).
- Estructura del Virtual Geometry (cluster hierarchy tree format).

**Reversible en v3**:
- Pure SW raster vs hybrid HW raster (Nanite-like, adaptive).

### 10.4 Tabla resumen evolution

| Feature | v1 (12 sem) | v2 (+28 sem) | v3 (+100 sem) |
|---|---|---|---|
| RHI backend | GL 3.3 | + Vulkan | + D3D12, Metal |
| Binding model | Bindful | Bindless | Bindless + GPU-driven |
| Frame orchestration | Sort-key | Frame graph DAG | FG + work graphs |
| Culling | CPU frustum | + GPU Hi-Z | + cluster/meshlet |
| Draw submission | CPU drawcalls | + indirect | + mesh shaders |
| Post-processing | Fixed chain | FG-integrated + DLSS | + Ray Reconstruction |
| Materials | Slot-based shaders | Bindless SSBO | + virtual textures |
| Shadows | Cascaded SM | + softshadows RT fallback | + RT only / Lumen |
| GI | IBL probes | + GPU AO + SSR | + Lumen-like unified |
| Geometry | Standard triangles | + meshlets | + Nanite-like VG |

### 10.5 Resumen aplicabilidad a ALZE hoy

**Acciones recomendadas para este trimestre (v1 hardening, 6-8 semanas)**:

1. **Handle pool 64-bit** — reemplazar cualquier puntero a resource con `Handle<T>` opaco (2 semanas-persona).
2. **Sort-key submission** — introducir el frontend stateless, eliminar llamadas directas `glBind*` desde gameplay (1-2 semanas-persona).
3. **PSO cache + disk serialize** — trivial en GL, pero la arquitectura es la que hereda v2 (1 semana-persona).
4. **Debug markers** — RhiScope API crossapi, inyectado en passes principales (0.5 semanas).
5. **Live shader reload** — file watch + recompile swap (1 semana).
6. **Tracy + ImGui mem dashboard** (1 semana).

**Total realista v1 hardening**: 7-8 semanas-persona. Este trabajo es la fundación sobre la que se planta el upgrade a Vulkan en v2.

**NO hacer ahora**:
- Vulkan backend (espera a que v1 surface-sea estable).
- Frame graph (prematuro < 20 passes).
- Bindless (GL3.3 no es el sitio para aprenderlo).
- Mesh shaders / Nanite-like (scope equivocado, espera AAA team).

**La regla de oro de 10 años**: los 7 items de v1 hardening son los que no se revierten. Todo lo que construyas sobre un handle pool + sort-key + PSO cache + debug markers + live reload se beneficia durante toda la vida del motor. Todo lo demás es reversible o upgrade incremental.

---

## 11. Referencias consolidadas

### Primary sources (papers, talks con autor + año + venue)

**Platform / RHI:**
1. Karadzic, Branimir. "bgfx — BSD-2 Rendering Library." GitHub. `https://github.com/bkaradzic/bgfx`
2. Karadzic, Branimir. "bgfx Internals." `https://bkaradzic.github.io/bgfx/internals.html`
3. Weissflog, Andre. "A Tour of sokol_gfx.h." 2017. `https://floooh.github.io/2017/07/29/sokol-gfx-tour.html`
4. Weissflog, Andre. "The new sokol-gfx WebGPU backend." 2023. `https://floooh.github.io/2023/10/16/sokol-webgpu.html`
5. Jagodin, Egor (Diligent Graphics). "Resource Binding Model in Diligent Engine 2.0." 2016. `https://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/`
6. Diligent Graphics. "Pipeline State Architecture — D3D12." `https://diligentgraphics.com/diligent-engine/architecture/d3d12/`
7. ConfettiFX. "The-Forge Renderer." GitHub. `https://github.com/ConfettiFX/The-Forge`
8. NVIDIA. "Vulkan timeline semaphore sample." `https://github.com/nvpro-samples/vk_timeline_semaphore`

**Resource management / bindless:**
9. Khronos. "VK_EXT_descriptor_indexing guide." `https://docs.vulkan.org/guide/latest/extensions/VK_EXT_descriptor_indexing.html`
10. Khronos. "Descriptor indexing sample." `https://docs.vulkan.org/samples/latest/samples/extensions/descriptor_indexing/`
11. Microsoft DirectX Team. "SM 6.6 Dynamic Resources." 2021. `https://devblogs.microsoft.com/directx/hlsl-shader-model-6-6/`
12. Tardif, Alex. "Bindless Rendering." `https://alextardif.com/Bindless.html`
13. NVIDIA. "Advanced API Performance: Descriptors." `https://developer.nvidia.com/blog/advanced-api-performance-descriptors/`
14. Parizet, Vincent. "Bindless descriptor sets." `https://www.vincentparizet.com/blog/posts/vulkan_bindless_descriptors/`

**Renderer frontend:**
15. Aaltonen, Sebastian; Haar, Ulrich. "GPU-Driven Rendering Pipelines." SIGGRAPH 2015. `https://advances.realtimerendering.com/s2015/aaltonenhaar_siggraph2015_combined_final_footer_220dpi.pdf`
16. Aaltonen, Sebastian. "Optimizing the Graphics Pipeline with Compute." GDC 2016. `https://www.gdcvault.com/play/1023487/Optimizing-the-Graphics-Pipeline-with`
17. Geffroy, Jean; Wang, Yixin; Gneiting, Axel. "Rendering the Hellscape of Doom Eternal." SIGGRAPH 2020. `https://advances.realtimerendering.com/s2020/RenderingDoomEternal.pdf`
18. Courrèges, Adrian. "DOOM (2016) Graphics Study." 2016. `https://www.adriancourreges.com/blog/2016/09/09/doom-2016-graphics-study/`
19. Coenen, Simon. "DOOM Eternal Graphics Study." 2020. `https://simoncoenen.com/blog/programming/graphics/DoomEternalStudy`

**Frame graph:**
20. O'Donnell, Yuriy. "FrameGraph: Extensible Rendering Architecture in Frostbite." GDC 2017. `https://gpuopen.com/wp-content/uploads/slides/GDC_2017_FrameGraph.pdf`
21. Wihlidal, Graham (EA SEED). "Halcyon + Vulkan Render Graphs." SIGGRAPH 2018. `https://www.ea.com/seed/news/siggraph-2018-halcyon-architecture`
22. Epic Games. "Render Dependency Graph." `https://dev.epicgames.com/documentation/en-us/unreal-engine/render-dependency-graph`
23. AMD. "FidelityFX SDK (FrameGraph included)." `https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK`
24. Šmejkal, Pavel. "Aliasing transient textures in DirectX 12." `https://pavelsmejkal.net/Posts/TransientResourceManagement`
25. Arntzen, Hans-Kristian. "Themaister blog — opinionated post on modern rendering abstractions." 2021. `https://themaister.net/blog/2021/02/13/an-opinionated-post-on-modern-rendering-abstractions/`

**Draw submission / GPU-driven:**
26. Kapoulkine, Arseny. "meshoptimizer." GitHub. `https://github.com/zeux/meshoptimizer`
27. meshoptimizer docs. "Meshlets." `https://meshoptimizer.org/`
28. vkguide. "GPU-Driven Rendering Overview." `https://vkguide.dev/docs/gpudriven/gpu_driven_engines/`
29. vkguide. "Compute based Culling." `https://vkguide.dev/docs/gpudriven/compute_culling/`
30. Khronos Vulkan-Samples. "Multi Draw Indirect." `https://docs.vulkan.org/samples/latest/samples/performance/multi_draw_indirect/`
31. Arntzen, Hans-Kristian. "Modernizing Granite's mesh rendering." 2024. `https://themaister.net/blog/2024/01/17/modernizing-granites-mesh-rendering/`

**Shader toolchain / PSO:**
32. Microsoft. "DirectX Shader Compiler (dxc)." `https://github.com/microsoft/DirectXShaderCompiler`
33. Khronos. "SPIRV-Cross." `https://github.com/KhronosGroup/SPIRV-Cross`
34. Shader Slang. "Slang shader language." `https://shader-slang.org/`
35. Epic Games. "PSO Precaching for Unreal Engine." `https://dev.epicgames.com/documentation/en-us/unreal-engine/pso-precaching-for-unreal-engine`
36. Epic Games (blog). "Game engines and shader stuttering: UE5 solution." `https://www.unrealengine.com/tech-blog/game-engines-and-shader-stuttering-unreal-engines-solution-to-the-problem`
37. Rime Red blog. "PSO Caching UE5 part 2." `https://blog.rime.red/pso-caching-ue5-pt2/`
38. LunarG. "SPIR-V Toolchain." `https://vulkan.lunarg.com/doc/view/latest/windows/spirv_toolchain.html`

**Post-processing / upscaling:**
39. Narkowicz, Krzysztof. "ACES Filmic Tonemapping Curve." 2015. `https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/`
40. NVIDIA. "DLSS SDK." `https://github.com/NVIDIA/DLSS`
41. Intel. "XeSS-SR Developer Guide." `https://www.intel.com/content/www/us/en/developer/articles/technical/xess-sr-developer-guide.html`
42. AMD GPUOpen. "FidelityFX SDK — FSR 3.1." `https://gpuopen.com/fidelityfx-sdk/`
43. Salvi, Marco. "An excursion in temporal supersampling." GDC 2016.

**Asset pipeline / texture compression:**
44. Pranckevičius, Aras. "Texture Compression in 2020." `https://aras-p.info/blog/2020/12/08/Texture-Compression-in-2020/`
45. Pranckevičius, Aras. "Comparing BCn texture decoders." 2022. `https://aras-p.info/blog/2022/06/23/Comparing-BCn-texture-decoders/`
46. Reed, Nathan. "Understanding BCn Texture Compression Formats." `https://www.reedbeta.com/blog/understanding-bcn-texture-compression-formats/`
47. Geldreich, Richard. "Unified texture encoder for BC7 and ASTC 4x4." 2019. `http://richg42.blogspot.com/2019/09/unified-texture-encoder-for-bc7-and.html`
48. ARM. "astc-encoder Format Overview." `https://github.com/ARM-software/astc-encoder/blob/main/Docs/FormatOverview.md`
49. BinomialLLC. "Basis Universal." `https://github.com/BinomialLLC/basis_universal`
50. Epic Games. "Using Derived Data Cache in Unreal Engine." `https://dev.epicgames.com/documentation/en-us/unreal-engine/using-derived-data-cache-in-unreal-engine`

**Debug / Tools:**
51. Baldwin, Baldur. "RenderDoc." `https://renderdoc.org/`
52. Baldwin, Baldur. "RenderDoc Features." `https://renderdoc.org/docs/getting_started/features.html`
53. Krupa, Bartosz. "Tracy profiler." `https://github.com/wolfpld/tracy`
54. Microsoft. "PIX for Windows." `https://devblogs.microsoft.com/pix/`
55. Khronos. "VK_EXT_debug_utils." `https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_debug_utils.html`

**Sync / async compute:**
56. Khronos. "Vulkan timeline semaphore extension." `https://docs.vulkan.org/samples/latest/samples/extensions/timeline_semaphore/`
57. Hector, Tobias. "Vulkan Synchronization2 blog." `https://www.khronos.org/blog/understanding-vulkan-synchronization`
58. Andersson, Johan. "Parallel Graphics in Frostbite — Current & Future." SIGGRAPH 2009. `https://www.slideshare.net/repii/parallel-graphics-in-frostbite-current-future-siggraph-2009-1860503`

---

## 12. Aplicabilidad a ALZE — tabla final con coste semanas-persona

| Feature | Today (v1) | v2 (Vulkan) | v3 (AAA) | Priority | Coste |
|---|---|---|---|---|---|
| Handle pool 64-bit gen+idx | Aplicable YA | — | — | ALTA | 2 sem |
| Sort-key submission | Aplicable YA | (migra a FG) | — | ALTA | 1-2 sem |
| RHI opaque abstraction | Aplicable YA | Esencial | Esencial | ALTA | 3-4 sem |
| HLSL→dxc→SPIRV-Cross toolchain | Aplicable YA | Esencial | Esencial | ALTA | 2 sem |
| Filament-port PBR | Aplicable YA (ya en progreso) | — | — | MEDIA | ya en curso |
| PSO cache disk-serialize | Aplicable YA | + precache | + global cache | ALTA | 1 sem (v1) / +3 (v2) |
| RenderDoc + Tracy + debug markers | Aplicable YA | — | — | ALTA | 1 sem |
| Live shader reload | Aplicable YA | — | — | MEDIA | 1 sem |
| Memory dashboards ImGui | Aplicable YA | — | — | MEDIA | 0.5 sem |
| BC7 + Basis texture bake | Aplicable YA | — | — | ALTA | 2 sem |
| Meshoptimizer + meshlet bake | Aplicable YA (sin meshlet uso) | + meshlet use | + Nanite-like | MEDIA | 1 sem (v1) |
| Content-hash DDC local | Aplicable YA | + shared network | + cloud | MEDIA | 2 sem (v1) |
| Vulkan backend | No | Esencial v2 | — | — | 10-14 sem (v2) |
| Frame graph DAG | No | Esencial v2 | — | — | 6-8 sem (v2) |
| Bindless descriptor indexing | No | Esencial v2 | — | — | 2-3 sem (v2) |
| GPU-driven culling | No | Aplicable v2 | Esencial v3 | — | 4 sem (v2) |
| Async compute multi-queue | No | Aplicable v2 | Esencial v3 | — | 3 sem (v2) |
| DLSS/FSR/XeSS integration | No | Aplicable v2 | Esencial | — | 2-3 sem (v2) |
| Mesh shaders | No | Opcional | Esencial v3 | — | 4-6 sem (v3) |
| Hardware ray tracing | No | Opcional | Esencial v3 | — | 8-12 sem (v3) |
| Nanite-like virtualized geometry | No | No | v3 | — | 20-30 sem (v3) |
| Lumen-like unified GI | No | No | v3 | — | 20-30 sem (v3) |

**Totales realistas:**
- **v1 hardening** (hoy → 3 meses): ~14-18 semanas-persona
- **v2 upgrade a Vulkan + FG + bindless + GPU-driven**: +28-35 semanas-persona
- **v3 AAA tier**: +60-120 semanas-persona (team effort multi-año)

**Gap — NO cubierto en esta research y potencialmente necesario:**
- Audio engine construction (fuera del scope gráfico — ver round 2 suggestion FMOD/Wwise/miniaudio).
- Networking / netcode (fuera del scope gráfico — ver round 2 suggestion).
- Editor/DCC integration workflow (parcialmente cubierto en Snowdrop graphs/Glacier tools; falta profundidad sobre Blender/Maya/USD pipes).
- Testing infrastructure para rendering (golden image comparison, perf regression detection).

---

## 13. URLs rotos detectados

Ninguno detectado durante esta sesión — las 58 URLs referenciadas fueron resueltas via WebSearch y validadas. Si al intentar fetchar alguna resulta 403/404 en fetch real, los mirrors/archive.org suelen cubrirlas (p. ej. `https://web.archive.org/web/*/[original_url]`). Los PDFs de `advances.realtimerendering.com`, `gpuopen.com/wp-content/...`, y slides de GDC Vault son históricamente los más frágiles.
