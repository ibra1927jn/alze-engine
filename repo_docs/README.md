# ALZE -- Game Engine

**Motor grafico de proposito general construido desde cero en C++17.**

---

## Vision

ALZE es un motor de videojuegos con arquitectura profesional, alto rendimiento y flexibilidad total. No es un proyecto educativo -- es un motor real con la ambicion de crear juegos reales.

## Estado Actual -- Fase 7+ (Motor completo con PBR, ECS, Physics, Audio)

| Metrica | Valor |
|---------|-------|
| **Archivos fuente** | 178+ (.h/.cpp) |
| **Lineas de codigo** | ~25,000-30,000 |
| **Archivos test** | 11 (~4,900 lineas) |
| **Lenguaje** | C++17 (sin RTTI, sin excepciones) |
| **Dependencias** | SDL2 + OpenGL 3.3 (GLAD), stb_image, cgltf |
| **Build** | CMake + Ninja/MinGW |
| **SIMD** | SSE2 en Vector2D/3D, Matrix3x3/4x4 |

## Arquitectura

```
ALZE/
├── src/
│   ├── core/       Engine, Window, InputManager, AudioEngine, ProceduralAudio,
│   │               JobSystem, EventBus, FrameAllocator, Profiler, Logger,
│   │               ResourceManager, UISystem, Serializer, SceneSerializer,
│   │               StateManager
│   ├── ecs/        ECSCoordinator (generational IDs, sparse-set SoA),
│   │               QueryCache (FNV-1a), CollisionSystem, Physics3DSystem,
│   │               Render3DSystem, PhysicsSystem, InputSystem, RenderSystem
│   ├── math/       Vector2D/3D (SSE2), Matrix3x3/4x4 (SSE2), Quaternion,
│   │               Transform2D/3D, AABB, Color, MathUtils
│   ├── renderer/   ForwardRenderer (PBR Cook-Torrance, CSM, IBL),
│   │               DeferredRenderer (G-Buffer, 4 RTs), ShaderLibrary,
│   │               ShaderLoader, PostProcess (Bloom, ACES, FXAA, Vignette,
│   │               ChromAb, FilmGrain, CAS), SSAO, ShadowMap (2-cascade CSM),
│   │               InstancedRenderer, LODSystem, DecalRenderer,
│   │               SkeletalAnimation, ModelLoader (glTF via cgltf),
│   │               ScreenEffects, ProceduralTextures, TextRenderer,
│   │               SpriteBatch2D, TileMap
│   ├── physics/    RigidBody3D (6 shapes), Collider3D (sphere, AABB, OBB,
│   │               capsule, heightfield, convex hull), GJK+EPA,
│   │               DynamicBVH3D, CollisionSolver3D (sequential impulse),
│   │               Constraints3D (distance, ball-socket, hinge),
│   │               SoftBody3D (XPBD), FluidSystem (SPH), Thermodynamics,
│   │               Electromagnetism, GravityNBody (Barnes-Hut), WaveSystem,
│   │               + reference headers (Quantum, Nuclear, Relativity, etc.)
│   ├── scene/      Camera3D, FPSController, SceneGraph, SceneNode
│   ├── game/       Play3DState, PlayState, WorldScene3D, BenchmarkScene3D,
│   │               ParticleSystem3D, PauseState, SharedWorldState
│   └── ai/         NavMesh (A* pathfinding)
├── assets/
│   ├── shaders/    GLSL shader files (pbr, depth, flat2d, unlit3d)
│   ├── hdri/       HDR environment maps
│   └── models/     glTF models
├── tests/          11 test suites (~4,900 lines)
└── CMakeLists.txt
```

## Sistemas Implementados

### ECS (Entity Component System)
- IDs generacionales (12 bits generacion + 20 bits indice)
- ComponentStorage sparse-set SoA, O(1) lookup, dense iteration
- QueryCache con FNV-1a hash y generation invalidation
- Compile-time type IDs sin RTTI

### Physics (36 archivos)
- RigidBody3D con 6 shapes y aerodynamics/buoyancy
- GJK+EPA para deteccion de colisiones convexas
- DynamicBVH3D broadphase con fat AABBs y AVL balance
- Sequential impulse solver con Coulomb friction y warm starting
- Constraints: distance, ball-socket, hinge (limits + motor)
- Soft body XPBD (cloth, rope, fatigue, plastic deformation)
- SPH Fluids (Navier-Stokes, poly6/spiky/viscosity kernels)
- Thermodynamics, Electromagnetism, N-Body gravity (Barnes-Hut), Waves

### Rendering
- Forward Renderer: PBR Cook-Torrance, 2-cascade CSM shadows, IBL
- Deferred Renderer: G-Buffer (4 RTs), PBR lighting pass
- Post-processing: Bloom, ACES tonemapping, FXAA, vignette, chromatic aberration, film grain, CAS
- SSAO, GPU instancing, LOD system con hysteresis, decals
- Normal mapping, parallax occlusion mapping, emissive maps
- Volumetric god rays via raymarching
- Render modes: PBR, Toon cel-shading, Neon holographic
- Shaders en archivos GLSL (assets/shaders/) con fallback embebido
- Skeletal animation con bone hierarchy e interpolacion
- glTF model loading via cgltf
- 2D pipeline: SpriteBatch2D, TileMap, TextRenderer

### Audio
- 32 voices con positional 3D y crossfade
- Procedural synth: ADSR, waveforms, LPF

### Core
- Fixed timestep 60Hz con delta cap 0.25s
- JobSystem thread pool con parallel_for
- EventBus pub/sub type-erased
- FrameAllocator (2MB bump, O(1) reset)
- Profiler, Logger, ResourceManager (weak-ptr cache, async load)
- Immediate mode UI (panels, buttons, sliders)
- Scene graph, Camera3D, FPS controller
- Input recording/replay (binary .inp format)
- Tab toggle 2D/3D en runtime

### AI
- NavMesh con A* pathfinding

## Texture Slot Layout (Forward Renderer)

| Slot | Uso |
|------|-----|
| 0 | Albedo texture |
| 1 | Normal map |
| 3 | Metallic/Roughness map |
| 4 | Emissive texture |
| 5 | AO texture |
| 6 | Height map (parallax) |
| 7 | IBL Irradiance cubemap |
| 8 | IBL Prefilter cubemap |
| 9 | IBL BRDF LUT |
| 10 | Shadow map cascade 0 |
| 11 | Shadow map cascade 1 |

## Build

```bash
cmake -G Ninja -B build -S .
cmake --build build
```

## Executables

```bash
./build/ALZE.exe            # Motor principal
./build/demo_3d.exe         # Demo 3D
./build/benchmark_3d.exe    # Benchmark (250 objetos)
./build/test_math.exe       # Tests de matematicas
./build/test_physics3d.exe  # Tests de fisica 3D (2,327 lineas)
./build/test_ecs.exe        # Tests de ECS
./build/test_stress.exe     # Tests de carga
```

## Roadmap

```
Fase 2.5   DONE  Pre-3D Improvements
Fase 3.0   DONE  3D Math (Vector3D, Matrix4x4, Quaternion, SSE2)
Fase 3.1   DONE  OpenGL 3.3 Rendering Backend
Fase 3.2   DONE  PBR Shaders + Materials + 3D Pipeline
Fase 4.0   DONE  Scene Graph + Transform Parenting
Fase 5.0   DONE  Asset Pipeline (glTF, textures, HDRI)
Fase 6.0   DONE  Serialization (JSON)
Fase 7.0   DONE  Physics Engine (36 files, GJK+EPA, constraints, soft body, fluids)
Fase 8.0   DONE  Audio Engine (32 voices, positional 3D, procedural synth)
Fase 9.0   DONE  Post-processing (Bloom, FXAA, SSAO, volumetrics)
Fase 10.0  DONE  Skeletal Animation
Fase 11.0  DONE  UI System (immediate mode)
Fase 12.0  TODO  Networking
Fase 13.0  TODO  Editor Visual (ImGui)
Fase 14.0  TODO  Scripting (Lua)
```

---

**ALZE** -- *Creado para ser real, no para ser un ejercicio.*
