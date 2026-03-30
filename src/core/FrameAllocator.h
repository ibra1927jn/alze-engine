#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace engine {
namespace core {

/// FrameAllocator — Allocador lineal que se resetea cada frame.
///
/// Reserva un bloque grande al iniciar. Cada alloc() mueve un puntero.
/// reset() devuelve el puntero a 0 (coste cero, sin free() individual).
///
/// Ideal para datos temporales por frame: render commands, collision
/// pairs, sort buffers, etc.
///
/// Uso:
///   FrameAllocator::init(2 * 1024 * 1024);  // 2MB
///   // Cada frame:
///   FrameAllocator::reset();
///   auto* cmds = FrameAllocator::alloc<RenderCommand>(count);
///   // ... usar cmds ...
///   // Al final del programa:
///   FrameAllocator::shutdown();
///
class FrameAllocator {
public:
    /// Inicializar con N bytes (por defecto 2MB)
    static void init(size_t sizeBytes = 2 * 1024 * 1024) {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_buffer) { std::free(s_buffer); s_buffer = nullptr; }
        s_buffer = static_cast<uint8_t*>(std::malloc(sizeBytes));
        s_capacity = sizeBytes;
        s_offset = 0;
        s_peakUsage = 0;
        s_allocCount = 0;
        s_totalAllocCount = 0;
    }

    /// Liberar memoria (llamar al cerrar el motor)
    static void shutdown() {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_buffer) {
            std::free(s_buffer);
            s_buffer = nullptr;
        }
        s_capacity = 0;
        s_offset = 0;
    }

    /// Resetear al inicio de cada frame (O(1), coste cero)
    static void reset() {
        std::lock_guard<std::mutex> lock(s_mutex); // Proteger contra data race
        s_offset = 0;
        s_allocCount = 0;
    }

    /// Reservar N bytes alineados
    static void* alloc(size_t bytes, size_t alignment = 8) {
        std::lock_guard<std::mutex> lock(s_mutex); // Proteger contra data race
        // Alinear el offset
        size_t aligned = (s_offset + alignment - 1) & ~(alignment - 1);

        if (aligned + bytes > s_capacity) {
            // Overflow — devolver nullptr (caller debe manejar)
            return nullptr;
        }

        void* ptr = s_buffer + aligned;
        s_offset = aligned + bytes;
        s_allocCount++;
        s_totalAllocCount++;

        if (s_offset > s_peakUsage) s_peakUsage = s_offset;

        return ptr;
    }

    /// Reservar N elementos de tipo T (sin constructor)
    template<typename T>
    static T* alloc(size_t count = 1) {
        return static_cast<T*>(alloc(sizeof(T) * count, alignof(T)));
    }

    /// Reservar y zero-inicializar
    static void* allocZeroed(size_t bytes, size_t alignment = 8) {
        void* ptr = alloc(bytes, alignment);
        if (ptr) std::memset(ptr, 0, bytes);
        return ptr;
    }

    // ── Stats ────────────────────────────────────────────────────
    static size_t getCapacity()       { std::lock_guard<std::mutex> lock(s_mutex); return s_capacity; }
    static size_t getUsed()           { std::lock_guard<std::mutex> lock(s_mutex); return s_offset; }
    static size_t getRemaining()      { std::lock_guard<std::mutex> lock(s_mutex); return s_capacity - s_offset; }
    static size_t getPeakUsage()      { std::lock_guard<std::mutex> lock(s_mutex); return s_peakUsage; }
    static int    getAllocCount()      { std::lock_guard<std::mutex> lock(s_mutex); return s_allocCount; }
    static int    getTotalAllocCount() { std::lock_guard<std::mutex> lock(s_mutex); return s_totalAllocCount; }
    static float  getUsagePercent()   { std::lock_guard<std::mutex> lock(s_mutex); return s_capacity > 0 ? static_cast<float>(s_offset) / s_capacity * 100.0f : 0; }
    static bool   isInitialized()     { std::lock_guard<std::mutex> lock(s_mutex); return s_buffer != nullptr; }

private:
    static inline uint8_t* s_buffer = nullptr;
    static inline size_t s_capacity = 0;
    static inline size_t s_offset = 0;
    static inline size_t s_peakUsage = 0;
    static inline int s_allocCount = 0;
    static inline int s_totalAllocCount = 0;
    static inline std::mutex s_mutex;
};

/// FrameArray - Un arreglo rápido respaldado por el FrameAllocator.
/// Se comporta como std::vector pero sin allocations en el heap.
/// La capacidad es fija al momento de crearse.
template<typename T>
class FrameArray {
public:
    FrameArray(size_t capacity) 
        : m_capacity(capacity), m_size(0) {
        m_data = FrameAllocator::alloc<T>(capacity);
    }

    void push_back(const T& value) {
        if (m_size < m_capacity && m_data) {
            m_data[m_size++] = value;
        }
    }

    void pop_back() {
        if (m_size > 0) m_size--;
    }

    T& back() { return m_data[m_size - 1]; }
    const T& back() const { return m_data[m_size - 1]; }

    bool empty() const { return m_size == 0; }
    size_t size() const { return m_size; }
    size_t capacity() const { return m_capacity; }
    
    void clear() { m_size = 0; }

    T& operator[](size_t index) { return m_data[index]; }
    const T& operator[](size_t index) const { return m_data[index]; }

    T* begin() { return m_data; }
    T* end() { return m_data + m_size; }
    const T* begin() const { return m_data; }
    const T* end() const { return m_data + m_size; }

private:
    T* m_data;
    size_t m_capacity;
    size_t m_size;
};

} // namespace core
} // namespace engine
