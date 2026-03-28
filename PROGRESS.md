# PROGRESS.md — alze (Motor Grafico 2D/3D)

## Metricas generales (2026-03-28)
- **Archivos fuente:** 178 (.h/.cpp)
- **Archivos test:** 11 (~4,900 lineas)
- **Subsistemas physics:** 36 archivos (11 integrados, 6 standalone, ~10 formula-only)
- **Lineas de codigo estimadas:** 25,000-30,000
- **Build:** CMake + Ninja + GCC/Clang

## En curso
- [2026-03-28] | Auditoria completa | 100% — terminada, pendiente aplicar fixes
- [2026-03-28] | Aplicar fixes P0 criticos | 100% — 5 fixes P0 + cleanup aplicados

## Completado — Funcional y probado
- [2026-03-28] | Math library (Vec2/3, Mat3x3/4x4, Quaternion, AABB, Transform) con SIMD SSE2 | Estable
- [2026-03-28] | ECS custom (generational IDs, sparse-set, dense iteration, query cache) | Estable
- [2026-03-28] | Physics core: RigidBody3D, 6 shapes, GJK+EPA, BVH broadphase, sequential impulse solver | Estable
- [2026-03-28] | Constraints: Distance, BallSocket, Hinge (limits + motor) | Funcional
- [2026-03-28] | Soft body XPBD (cloth, rope, fatigue, plastic deformation) | Funcional
- [2026-03-28] | SPH Fluids (Navier-Stokes, poly6/spiky/viscosity kernels) | Funcional
- [2026-03-28] | Thermodynamics (conduction, radiation, phase transitions) | Funcional
- [2026-03-28] | Electromagnetism (Coulomb, Lorentz, dipoles) | Funcional
- [2026-03-28] | N-Body gravity (Barnes-Hut octree) | Funcional
- [2026-03-28] | Wave system (acoustic, Doppler) | Funcional
- [2026-03-28] | Forward renderer PBR (Cook-Torrance, CSM shadows, IBL, normal/parallax mapping) | Funcional (texture slot collision)
- [2026-03-28] | Deferred renderer (G-Buffer, PBR lighting pass) | Funcional (uniforms no cacheados)
- [2026-03-28] | Post-process (bloom, tonemapping, FXAA, vignette, chromatic aberration, film grain) | Funcional
- [2026-03-28] | SSAO | Funcional
- [2026-03-28] | Instanced rendering, LOD system, decals, skeletal animation | Funcional
- [2026-03-28] | Audio engine (32 voices, positional 3D, crossfade, procedural synth) | Estable
- [2026-03-28] | JobSystem thread pool + parallel_for | Estable
- [2026-03-28] | UI System (immediate mode: panels, buttons, sliders) | Estable
- [2026-03-28] | Scene graph, Camera3D, FPS controller | Estable
- [2026-03-28] | Input recording/replay (binary .inp format) | Funcional
- [2026-03-28] | Frame profiling (CSV export) | Funcional
- [2026-03-28] | PWA toggle 2D/3D con Tab key | Funcional
- [2026-03-28] | 9 test executables | Funcional

## Completado — Formula reference (no simulacion)
- [2026-03-28] | QuantumSystem.h, NuclearPhysics.h, Relativity.h | Formulas correctas, sin loop de simulacion
- [2026-03-28] | OpticsSystem.h, Hyperelasticity.h, CompressibleFlow.h | Formulas correctas, no integradas
- [2026-03-28] | AdvancedFriction.h (Pacejka), MolecularDynamics.h | Implementadas, standalone

## Pendiente
- Fix P0: thread safety restante (FrameAllocator, Logger, Profiler) | Prioridad: alta
- Fix P1: EPA contactPoint, subsystem dt, StateManager bounds | Prioridad: alta
- Fix P1: texture slot collision en ForwardRenderer | Prioridad: alta
- Tests: renderer, audio, NavMesh, Color, MathConstants, Transform2D | Prioridad: media
- Performance: BVH raycast, island adjacency, deferred uniform cache | Prioridad: media
- Cleanup: build duplicates (CMakeLists duplicado Engine.cpp, NavMesh en math) | Prioridad: baja

## Bloqueado
- Ninguno
