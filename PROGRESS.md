# PROGRESS.md — alze (Motor Grafico 2D/3D)

## Metricas generales (2026-03-28)
- **Archivos fuente:** 178 (.h/.cpp)
- **Archivos test:** 13 (~5,300 lineas)
- **Subsistemas physics:** 36 archivos (11 integrados, 6 standalone, ~10 formula-only)
- **Lineas de codigo estimadas:** 25,000-30,000
- **Build:** CMake + Ninja + GCC/Clang

## En curso
- [2026-03-28] | Auditoria completa | 100% — terminada, pendiente aplicar fixes
- [2026-03-28] | Aplicar fixes P0 criticos | 100% — 5 fixes P0 + cleanup aplicados
- [2026-03-28] | Refactor shader system + slot fix | 100%
- [2026-03-28] | Scene Editor foundation | 100% — hierarchy, inspector, gizmo con flechas
- [2026-03-28] | Quality/hardening pass | 100% — 8 fixes: adjacency CSR, double iteration, SIMD negation, JobSystem allocs, FPS title, Quaternion dedup, sortByMaterial .get(), ForwardRenderer negation
- [2026-03-29] | Fix build errors UISystem+Editor API mismatch | 100% — ALZE.exe compila
- [2026-03-29] | Heartbeat maintenance pass | 100% — .gitignore security, NavMesh neighbor bug fix, 111 new tests
- [2026-03-29] | Heartbeat maintenance pass #2 | 100% — getActiveVoices data race fix, 28 Serializer tests, CLAUDE.md bugs section updated
- [2026-03-29] | Heartbeat maintenance pass #3 | 100% — build fix (ostream includes), header include optimization (iostream→iosfwd), Texture2D RAII fix, unused cassert removal, repo cleanup (3861 junk files removed from tracking)
- [2026-03-29] | Heartbeat maintenance pass #4 | 100% — removed dead sortFrontToBack() method, removed unused PHONG3D shader aliases, unused include cleanup
- [2026-03-30] | Heartbeat maintenance pass #5 | 100% — security cleanup (removed test_key.txt + _del_*.txt, updated .gitignore), fixed vibratoDept→vibratoDepth typo, removed dead assignment in EnvironmentMap lookAt
- [2026-03-30] | Heartbeat maintenance pass #6 | 100% — fixed -Wshadow warnings (Physics3DSystem r, PlayState alpha), migrated std::cout/std::cerr→Logger in 6 files (ModelLoader, SSAO, EnvironmentMap, ShaderProgram, ShaderLibrary, GraphicsContext)
- [2026-03-30] | Heartbeat maintenance pass #7 | 100% — fixed -Wshadow warnings (Color.h constructor params r/g/b/a, AABB.cpp min/max ~126 warnings eliminated), migrated remaining std::cout/std::cerr→Logger in 5 renderer files (Texture2D.h, ShaderLoader.h, ShadowMap.h, PostProcess.cpp, SSAO.cpp). All headers now use Logger consistently.
- [2026-03-30] | Heartbeat maintenance pass #8 | 100% — fixed AudioEngine listener position data race (SDL_LockAudioDevice in setListener3D/setListenerPosition), updated CLAUDE.md to reflect 5 previously-fixed thread-safety issues (EventBus unsubscribe, JobSystem atomic, FrameAllocator mutex, Profiler mutex, AudioEngine locks)
- [2026-03-30] | Heartbeat maintenance pass #9 | 100% — completed cstdio migration: replaced iostream/fstream with cstdio in Logger.h, FrameLogger.h, InputRecorder.h, Serializer.h, ShaderLoader.h. Zero C++ stream headers remain in src/
- [2026-03-30] | Heartbeat maintenance pass #9 | 100% — Logger s_minLevel atomic thread-safety, Serializer readBool bounds-safe for -fno-exceptions, StateManager render loop single-state fix, ResourceManager forEachAlive mutex, removed dead LowPassFilter var in ProceduralAudio, removed unused <functional> include
- [2026-03-30] | Heartbeat maintenance pass #10 | 100% — removed unused includes (Logger.h: sstream+vector, UISystem.h: functional+vector), fixed data races in Logger accessors (isFileEnabled/getFilePath/getLineCount) and ResourceManager stat methods (getCacheSize/getCacheHits/getTotalLoads/getHitRate)
- [2026-03-30] | Heartbeat maintenance pass #11 | 100% — fixed dangling references in test_subsystems (auto& → auto for return-by-value Profiler methods), eliminated AudioEngine spawnVoice race window (double-lock → single lock), removed unused SDL.h includes from main.cpp and benchmark.cpp
- [2026-03-30] | Heartbeat maintenance pass #12 | 100% — removed dead HuffTable struct from ImageDecoder (broken decode() always returned -1, replaced by CanonHuff), removed unused pushDir parameter from CollisionSystem::applyWarmImpulse
- [2026-03-30] | Heartbeat maintenance pass #13 | 100% — fixed AudioEngine data races (setGroupVolume/stopMusic/playMusic/crossfadeMusic/update missing SDL_LockAudioDevice), removed unused includes (AudioEngine.h: cmath+cstring+mutex, FrameLogger.h: sstream, SharedWorldState.h: cstring)
- [2026-03-30] | Heartbeat maintenance pass #14 | 100% — removed debug prints from test_physics3d.cpp, removed unused includes (<functional> from 6 headers, <numeric> from ProceduralAudio.h, <thread> from ECSCoordinator.h)
- [2026-03-30] | Heartbeat maintenance pass #15 | 100% — removed 14 unused includes from 13 files (moved <algorithm>/<cmath> from NavMesh.h→.cpp, removed unused <limits>/<complex>/<type_traits>/<string>/<algorithm>/<cmath> from Collider3D/QuantumSystem/ObjectPool/BenchmarkScene3D/ModelLoader/etc)
- [2026-03-30] | Heartbeat maintenance pass #16 | 100% — fixed double m_gcTimer increment bug in Play3DState::update() (GC fired at 2.5s instead of 5s), replaced ostringstream with snprintf in Engine::run() and removed <sstream> include
- [2026-03-30] | Heartbeat maintenance pass #17 | 100% — completed math ostream→cstdio migration (Color, Vector2D/3D, Matrix3x3/4x4, Quaternion, AABB: operator<< → toString() with snprintf, removed all <iosfwd>/<ostream>), removed unused includes (SpatialHash.h from Raycast, <cstdlib> from ScreenEffects), removed dead accessors (FrameLogger::getPath, InputRecorder::getPlaybackPath)
- [2026-03-30] | Heartbeat maintenance pass #18 | 100% — removed unused <cstdio> from 5 math headers (Vector2D/3D, Matrix3x3/4x4, Quaternion), removed unused <cstdlib> from PlayState, Play3DState, SSAO headers, added explicit <cstdio> to 5 math .cpp files that use snprintf
- [2026-03-30] | Heartbeat maintenance pass #19 | 100% — removed 28 redundant includes across 24 headers (transitively provided by MathUtils.h/Vector3D.h/etc), extracted 3 duplicate color conversion lambdas in Editor.cpp to static methods, removed 5 more redundant includes (SpatialHash, DebugDraw, Components, AABB.cpp, Matrix3x3)
- [2026-03-30] | Heartbeat maintenance pass #20 | 100% — moved 2 header-only includes to .cpp (ImageDecoder.h from ModelLoader.h, ShaderLibrary.h from ForwardRenderer.h), removed 19 duplicate includes from 15 .cpp files (SDL.h, cmath, glad/gl.h, CollisionSolver3D.h, DynamicBVH3D.h, JobSystem.h already provided by corresponding headers)
- [2026-03-30] | Heartbeat maintenance pass #21 | 100% — removed unused <cmath> from AudioEngine.cpp and NavMesh.cpp, guarded AudioBuffer::resampleTo against division by zero (targetRate <= 0), added default cases to switch statements in SpriteBatch2D.cpp (BlendMode) and TextRenderer.cpp (TextAlign)
- [2026-03-30] | Heartbeat maintenance pass #22 | 100% — added default cases to switch statements in ParticleSystem3D.h (EmitterShape) and SpriteAnimation.h (LoopMode), guarded division by zero in smoothstep/smootherstep/repeat (MathConstants.h, PhysicsMath.h) and sphereSubmergedFraction (radius==0)
- [2026-03-30] | Heartbeat maintenance pass #23 | 100% — guarded division by zero in ADSR envelope (ProceduralAudio.cpp), UISystem slider/progressBar, MathUtils::remap, SpriteAnimation (fps/columns), aspect ratio calculations (Play3DState, BenchmarkScene3D). Added default initializers to uninitialized POD struct members in Physics3DSystem (SolverContact, PhysEntity), RenderSystem (RenderEntry), EventBus (5 event structs), Window (m_width/m_height), Play3DState (Collision3DEvent)
- [2026-03-30] | Heartbeat maintenance pass #24 | 100% — added default initializers to uninitialized struct members (RayHit::distance, ConductingFluidProperties 3 floats, Nuclide Z/A/atomicMass/halfLife/bindingEnergy), guarded div-by-zero in SoftBody3D fatigue (stress==0) and PhysicsMaterial::stribeckFriction (stribeckVelocity==0)

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
- [2026-03-28] | Forward renderer PBR (Cook-Torrance, CSM shadows, IBL, normal/parallax mapping) | Estable
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
- [2026-03-28] | 11 test executables | Funcional
- [2026-03-28] | Fix texture slot collision (IBL 7-9, shadows 10-11, material 0-6) | Estable
- [2026-03-28] | Shaders extraidos a assets/shaders/ con ShaderLoader + fallback embebido | Estable
- [2026-03-28] | README actualizado (178 files, 25-30k lines, phases 2.5-11 done) | Estable
- [2026-03-28] | Fix EPA contactPoint — interpolacion baricentrica de support points | Estable
- [2026-03-28] | Physics2D system (AABB, Circle, RigidBody2D, broadphase spatial hash) | Funcional
- [2026-03-28] | Scene Editor foundation (hierarchy, inspector, gizmos, F1 toggle) | Funcional
- [2026-03-28] | 6 P0 bugs fixed (vibrato, GPU leak, material cmp, subsystem dt, entity sentinel) | Commit 1ba4872
- [2026-03-28] | 6 P1/P2 bugs fixed (capsule 16pts, island O(N+M), raycast BVH, deferred cache, JobSystem, NavMesh move) | Commit f2b97f1
- [2026-03-28] | 8 quality fixes (zero-alloc CSR, fix 80→8 iterations, SIMD negation, snprintf FPS, inline chunks, deduplicate quaternion) | Commit 45a9373

## Completado — Formula reference (no simulacion)
- [2026-03-28] | QuantumSystem.h, NuclearPhysics.h, Relativity.h | Formulas correctas, sin loop de simulacion
- [2026-03-28] | OpticsSystem.h, Hyperelasticity.h, CompressibleFlow.h | Formulas correctas, no integradas
- [2026-03-28] | AdvancedFriction.h (Pacejka), MolecularDynamics.h | Implementadas, standalone

## Pendiente
- ~~Fix P0: thread safety restante (FrameAllocator, Logger, Profiler)~~ | **DONE**
- ~~Fix P1: EPA contactPoint~~ | **DONE**
- ~~Fix P1: StateManager bounds, EventBus unsubscribe, ResourceManager locks~~ | **DONE**
- ~~Fix P1: subsystem dt (physics), ForwardRenderer material skip~~ | **DONE**
- ~~Fix P1: texture slot collision en ForwardRenderer~~ | **DONE**
- ~~Tests: NavMesh, Color, MathConstants, Transform2D~~ | **DONE** (111 tests in test_utils + test_navmesh)
- Tests: renderer, audio | Prioridad: media
- ~~Performance: BVH raycast, island adjacency, deferred uniform cache~~ | **DONE**
- ~~Cleanup: build duplicates (CMakeLists duplicado Engine.cpp, NavMesh en math)~~ | **DONE**

## Bloqueado
- Ninguno
