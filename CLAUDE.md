# alze — Motor grafico 2D/3D

## Stack
C++17 (fno-exceptions, fno-rtti) + CMake 3.20 + Ninja
SDL2 2.30.12 + OpenGL 3.3 (GLAD loader)
Librerias embebidas: stb_image, cgltf (glTF)
Zero dependencias externas para physics, audio, ECS, UI
SIMD: SSE2 en Vector2D/3D, Matrix3x3/4x4

## Comandos
- `cmake -G Ninja -B build -S .` — Configurar build
- `cmake --build build` — Compilar todo
- `./build/ALZE.exe` — Ejecutar motor principal
- `./build/demo_3d.exe` — Demo 3D directo
- `./build/benchmark_3d.exe` — Benchmark de rendimiento (250 objetos)
- `./build/test_math.exe` — Tests de matematicas
- `./build/test_physics3d.exe` — Tests de fisica 3D (el mas completo, 2327 lineas)
- `./build/test_ecs.exe` — Tests de ECS
- `./build/test_stress.exe` — Tests de carga

## Arquitectura real (auditada 2026-03-28)

### Core (src/core/)
- Engine.h/cpp — Game loop fixed timestep 60Hz, delta cap 0.25s
- Window.h/cpp — SDL2, OpenGL 3.3 core profile, resize, gamepad
- InputManager.h/cpp — Keyboard, mouse, gamepad con deadzone
- StateManager.h — Push/pop/change con transparencia (bug bounds check)
- EventBus.h — Pub/sub type-erased (sin unsubscribe — dangling risk)
- JobSystem.h — Thread pool con parallel_for (m_running no-atomico — UB)
- AudioEngine.h/cpp — 32 voces, positional 3D, crossfade (data races)
- ProceduralAudio.h/cpp — Synth ADSR, waveforms, LPF (bug vibrato param)
- FrameAllocator.h — Linear allocator per-frame (no thread-safe)
- Profiler.h — High-res timer, ring buffers (no thread-safe)
- Logger.h — Dual output consola+archivo (no thread-safe)
- ResourceManager.h — Weak-ptr cache, async load (partial thread-safe)
- UISystem.h — Immediate mode UI
- Serializer.h — JSON read/write (crash con -fno-exceptions)
- SceneSerializer.h — Scene save/load (bug parseo isStatic)

### Renderer (src/renderer/)
- ForwardRenderer — PBR Cook-Torrance, 2-cascade CSM, IBL, 8 point + 4 spot lights
- DeferredRenderer — G-Buffer (4 RTs), PBR lighting pass
- ShaderLibrary.h — Shaders embebidos como string literals
- PostProcess — Bloom, ACES tonemapping, FXAA, vignette, chromatic aberration, film grain, CAS
- SSAO — Screen-space ambient occlusion
- ShadowMap — 2-cascade CSM con hardware PCF
- InstancedRenderer — GPU instancing
- LODSystem — Distance-based con hysteresis
- DecalRenderer, ScreenEffects, ProceduralTextures
- ModelLoader — glTF via cgltf
- SkeletalAnimation — Bone hierarchy con interpolacion
- TextRenderer, SpriteBatch2D, TileMap (2D pipeline)

### ECS (src/ecs/)
- ECSCoordinator — Generational IDs (20-bit index + 12-bit gen)
- ComponentStorage — Sparse-set SoA, O(1) lookup, dense iteration
- QueryCache — FNV-1a hash con generation invalidation
- Systems: CollisionSystem, Physics3DSystem, Render3DSystem, PhysicsSystem, InputSystem, RenderSystem

### Physics (src/physics/) — 36 archivos
**Integrados en PhysicsWorld3D:**
- RigidBody3D (6 shapes, aerodynamics, buoyancy)
- Collider3D (sphere, AABB, OBB, capsule, heightfield, convex hull)
- CollisionSolver3D (sequential impulse, Coulomb friction, warm start)
- GJK+EPA (convex collision detection)
- DynamicBVH3D (broadphase, fat AABBs, AVL balance)
- Constraints3D (distance, ball-socket, hinge with limits/motor)
- Integrados via setXXXSystem(): SoftBody3D (XPBD), FluidSystem (SPH), Thermodynamics, Electromagnetism, GravityNBody (Barnes-Hut), WaveSystem

**Standalone (no integrados):**
- Chemistry, MHDSystem, FractureSystem, MolecularDynamics, CrossSystemCoupling, UnifiedSimulation

**Formula reference (sin simulacion):**
- QuantumSystem, NuclearPhysics, Relativity, OpticsSystem, Hyperelasticity, CompressibleFlow, AdvancedFriction

### Scene (src/scene/)
- Camera3D, FPSController, SceneGraph, SceneNode

### Game (src/game/)
- Play3DState, PlayState (2D), WorldScene3D, BenchmarkScene3D
- ParticleSystem3D, PauseState, SharedWorldState

### AI (src/ai/)
- NavMesh (A* pathfinding)

### Tests (tests/)
- 11 archivos, ~4,900 lineas, asserts custom (sin framework externo)
- test_physics3d.cpp es el mas completo (2,327 lineas, cubre 20+ subsistemas)

## Reglas del proyecto
- Header-heavy: mayoria del codigo inline en .h
- Shaders embebidos como string literals en ShaderLibrary.h
- Sin framework de test externo: asserts custom con contadores pass/fail
- Compilador: GCC o Clang (MSVC no soportado)
- Tab cambia entre 2D y 3D en runtime
- Ventana: 1024x768, fixed timestep 60Hz
- Build en Windows requiere MSYS2 MinGW
- Codigo en ingles, comentarios en espanol
- Commits en ingles: tipo(scope): descripcion breve

## Bugs conocidos (ver ERRORES.md)
- Todos los P0 y P1 resueltos (ver ERRORES.md para historial completo)
- ~20 physics headers son formula-reference sin simulacion (Quantum, Nuclear, Relativity, etc.)
