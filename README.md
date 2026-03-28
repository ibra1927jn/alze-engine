# 🔥 ALZE — Game Engine

**Motor gráfico de propósito general construido desde cero en C++17.**

---

## Visión

ALZE es un motor de videojuegos diseñado para ser **comercialmente viable**, con arquitectura profesional, alto rendimiento y flexibilidad total. No es un proyecto educativo — es un motor real con la ambición de crear juegos reales.

## Estado Actual — Fase 3.2 (3D Materials + Shaders)

| Métrica | Valor |
|---------|-------|
| **Archivos** | 72+ |
| **Líneas de código** | ~11,000 |
| **Tests** | 436/436 ✅ |
| **Lenguaje** | C++17 (sin RTTI, sin excepciones) |
| **Dependencias** | SDL2 (local MSYS2 o FetchContent) |
| **Build** | CMake + Ninja/MinGW |
| **Score auditoría** | 4.6/5.0 |

## Arquitectura

```
ALZE/
├── src/
│   ├── core/       → Engine, Window, GraphicsContext, Input, Audio,
│   │                 Profiler, Logger, EventBus, JobSystem, FrameAllocator,
│   │                 RenderCommand, RenderBackend, Camera2D, DebugDraw
│   ├── ecs/        → EntityManager, ComponentStorage, ECSCoordinator,
│   │                 SystemManager, Components, PhysicsSystem,
│   │                 CollisionSystem, RenderSystem, InputSystem
│   ├── math/       → Vector2D, Matrix3x3, Transform2D, AABB, Color, MathUtils
│   │                 Vector3D, Matrix4x4, Quaternion, Transform3D
│   ├── renderer/   → GLAD, ShaderProgram, ShaderLibrary, Mesh, Mesh3D,
│   │                 GLRenderBackend, Texture2D, Material, MeshPrimitives,
│   │                 ForwardRenderer (Blinn-Phong)
│   ├── physics/    → SpatialHash, ContactCache, Raycast
│   └── game/       → PlayState, PauseState, ParticlePool
├── tests/          → 7 suites, 436 tests
└── CMakeLists.txt
```

## Sistemas Implementados

### 🧱 ECS (Entity Component System)

- IDs generacionales (12 bits generación + 20 bits índice)
- ComponentStorage con Swap-and-Pop + sparse hash map
- Compile-time type IDs sin RTTI
- Signature matching con fold expressions

### ⚡ Física

- Integración Semi-Implícita de Euler
- Fixed Timestep 60Hz + accumulator + interpolación alpha
- Drag exponencial (`exp(-drag*dt)`) frame-rate independiente
- Sleep system con thresholds configurables
- Paralelismo via JobSystem (N-1 worker threads)

### 💥 Colisiones

- Broad Phase: SpatialHash con counting-sort (grid configurable + OOB warnings)
- Narrow Phase: AABB overlap con MTV
- Resolución: impulso + fricción + warm starting (ContactCache con hash table fijo)
- Separación proporcional a masa (invMass ratio)
- Layers por bitmask bidireccional + triggers

### 🎨 Rendering

- RenderCommand Queue desacoplada del backend
- RenderBackend abstracto (actualmente SDL2, preparado para OpenGL)
- Z-sort + type batching para minimizar cambios de estado
- Frustum culling
- 0 llamadas SDL directas en lógica de juego

### 🛠️ Core

- Engine con Game Loop (Fixed Timestep)
- Window separada de GraphicsContext (preparado para OpenGL)
- FrameAllocator (2MB bump allocator, O(1) reset)
- EventBus pub/sub con RAII (unique_ptr)
- Profiler con secciones ID-based (std::array, O(1))
- Logger, FrameLogger (CSV), InputRecorder (binario)
- Camera2D con follow, zoom, shake, culling
- StateManager con estados transparentes (PlayState + PauseState)

### 🎮 Gameplay

- Sistema de partículas con pool pre-allocado
- Platformer controller con coyote time + jump buffer
- HUD con barras de FPS, partículas, velocidad, estado

## Roadmap

```
Fase 2.5  ✅ Pre-3D Improvements (COMPLETADA)
Fase 3.0  ✅ 3D Math (Vector3D, Matrix4x4, Quaternion) (COMPLETADA)
Fase 3.1  ✅ OpenGL Rendering Backend (COMPLETADA)
Fase 3.2  ✅ Shaders + Materials + 3D Pipeline (COMPLETADA)
Fase 4.0  ⬜ Scene Graph + Transform Parenting
Fase 5.0  ⬜ Asset Pipeline (modelos, texturas, audio)
Fase 6.0  ⬜ Serialización de escenas (JSON/binario)
Fase 7.0  ⬜ Scripting (Lua)
Fase 8.0  ⬜ Editor Visual (ImGui)
Fase 9.0  ⬜ Audio Engine
Fase 10.0 ⬜ Animación Esqueletal
Fase 11.0 ⬜ UI System (in-game)
Fase 12.0 ⬜ Networking
```

## Build

```bash
cmake -G Ninja -B build -S .
cmake --build build
```

## Tests

```bash
./build/test_physics.exe
./build/test_ecs.exe
./build/test_subsystems.exe
./build/test_phase25.exe
./build/test_phase3.exe
./build/test_stress.exe
./build/test_math3d.exe
```

---

**ALZE** — *Creado para ser real, no para ser un ejercicio.*
