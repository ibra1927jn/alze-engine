# alze -- Motor grafico 2D/3D custom (C++17)

## Stack
- C++17 (-fno-exceptions, -fno-rtti) + CMake 3.20 + Ninja
- SDL2 2.30.12 + OpenGL 3.3 (GLAD loader)
- Libs embebidas: stb_image, cgltf (glTF)
- SIMD SSE2 en Vec2/3, Mat3x3/4x4, Quaternion
- Compilador: GCC o Clang (MSVC no soportado, Windows requiere MSYS2 MinGW)

## Build
```bash
cmake -G Ninja -B build -S .
cmake --build build
./build/ALZE.exe          # Motor principal
./build/demo_3d.exe       # Demo 3D
./build/benchmark_3d.exe  # Benchmark 250 objetos
./build/test_physics3d.exe # Test mas completo (2327 lineas)
```

## Arquitectura (src/)
- **core/** -- Engine loop 60Hz, Window, InputManager, AudioEngine, JobSystem, EventBus, UISystem, Logger, Profiler, ResourceManager, Serializer
- **renderer/** -- ForwardRenderer (PBR, CSM, IBL), DeferredRenderer, PostProcess (bloom, FXAA, SSAO), ShaderLibrary, LOD, instancing, skeletal animation, 2D pipeline
- **ecs/** -- Generational IDs (20-bit index + 12-bit gen), sparse-set SoA, QueryCache FNV-1a, systems: Collision, Physics3D, Render3D
- **physics/** -- 36 archivos. RigidBody3D, 6 shapes, GJK+EPA, DynamicBVH3D, sequential impulse solver, constraints, SoftBody XPBD, SPH fluids, thermodynamics, electromagnetism, N-Body Barnes-Hut
- **scene/** -- Camera3D, FPSController, SceneGraph
- **game/** -- Play3DState, PlayState (2D), WorldScene3D, BenchmarkScene3D, ParticleSystem3D
- **editor/** -- Scene editor (hierarchy, inspector, gizmos, F1 toggle)
- **ai/** -- NavMesh (A* pathfinding)
- **math/** -- Vec2/3, Mat3x3/4x4, Quaternion, AABB, Transform (SIMD SSE2)
- **tests/** -- 11 archivos, ~4900 lineas, asserts custom (sin framework externo)

## Reglas
- Codigo en ingles, comentarios en espanol
- Commits en ingles: tipo(scope): descripcion breve
- Header-heavy: mayoria del codigo inline en .h
- Shaders embebidos como string literals en ShaderLibrary.h
- Tab cambia entre 2D y 3D en runtime
- Ventana: 1024x768, fixed timestep 60Hz
- Leer ERRORES.md y PROGRESS.md antes de empezar cualquier tarea

## Estado actual (2026-03-29)
- ~178 archivos fuente, ~25-30k lineas
- Build compila (ALZE.exe, demo_3d, benchmark_3d, 9 test executables)
- 20+ bugs P0/P1/P2 corregidos en sesion 2026-03-28
- Pendiente: tests para renderer/audio/NavMesh, performance BVH/deferred cache
- Ver ERRORES.md para bugs conocidos, PROGRESS.md para tareas pendientes
