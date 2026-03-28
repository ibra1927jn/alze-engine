# ERRORES.md — Lo que no volvemos a hacer

## Formato
[Fecha] | [Archivo afectado] | [Error] | [Fix aplicado]

---

## P0 — Undefined Behavior / Crashes

- [2026-03-28] | core/JobSystem.h:146 | m_running es bool no-atomico leido/escrito desde multiples threads — data race UB | **FIXED** Cambiado a std::atomic<bool>
- [2026-03-28] | core/AudioEngine.cpp:345-353 | m_instanceCounts accedido sin lock desde main thread y audio callback — data race | **FIXED** SDL_LockAudioDevice en spawnVoice y canPlayInstance
- [2026-03-28] | ecs/ECSCoordinator.h:329-341 | cloneEntity() copia mask de componentes pero NO copia datos a los storages — crash al acceder componentes del clon | **FIXED** Virtual cloneComponent() en storages, cloneEntity itera y copia datos
- [2026-03-28] | core/SceneSerializer.h:186 | isStatic parsea float Y bool, consumiendo el siguiente token — corrupcion silenciosa del parseo | **FIXED** Cambiado a solo r.readBool()
- [2026-03-28] | core/Serializer.h:195 | std::stof lanza excepcion en input invalido, pero compilamos con -fno-exceptions — terminacion del programa | **FIXED** Reemplazado con strtof + validacion manual

## P1 — Bugs funcionales serios

- [2026-03-28] | core/StateManager.h:106-117 | Render loop de estados transparentes puede ir a indice -1 si no hay estado opaco debajo — out-of-bounds | Clampar firstVisible a 0
- [2026-03-28] | core/EventBus.h | Sin mecanismo de unsubscribe — lambdas con captures se vuelven dangling pointers al destruir states | Agregar unsubscribe o weak references
- [2026-03-28] | core/FrameAllocator.h | No es thread-safe pero se usa desde JobSystem workers — data race en s_offset | Usar atomic o allocator per-thread
- [2026-03-28] | core/Logger.h | No es thread-safe (s_file, s_lineCount) pese a incluir <mutex> — data race en output | Agregar lock_guard en log()
- [2026-03-28] | core/Profiler.h | No thread-safe — data race si se profila desde workers | Agregar locks o profiler per-thread
- [2026-03-28] | core/ResourceManager.h:77,107 | has() y getAliveCount() no toman lock del mutex — data race | Agregar lock
- [2026-03-28] | renderer/ForwardRenderer.cpp:350-355 | Texture slots 4-6 colisionan entre IBL (irradiance/prefilter/brdfLUT) y material (emissive/AO/height) | **FIXED** IBL reasignado a slots 7-9, shadows a 10-11. Material mantiene 0-6
- [2026-03-28] | renderer/ForwardRenderer.cpp:334 | Material skip compara direcciones de copias en RenderItem3D — siempre false, optimizacion nunca activa | Comparar por material ID o por valor
- [2026-03-28] | physics/GJK.inl:94 | EPA nunca calcula contactPoint — convex hull collisions obtienen punto (0,0,0) | **FIXED** Interpolacion baricentrica de support points en la cara mas cercana del polytope; se trackean supportA/supportB por vertice
- [2026-03-28] | physics/PhysicsWorld3D.cpp:344-357 | Subsistemas fuera del substep loop reciben subDt en vez de dt — simulacion a velocidad incorrecta | Pasar dt a fluid/EM/softbody/gravity/wave

## P2 — Bugs menores y performance

- [2026-03-28] | physics/Collider3D.cpp:253-263 | capsuleVsOBB usa 5 muestras discretas — pierde colisiones entre samples | Implementar closest-point-on-segment-to-OBB analitico
- [2026-03-28] | physics/PhysicsWorld3D.cpp:155-168 | Island building escanea TODOS los contactos por body — O(N*M) | Construir adjacency list una vez
- [2026-03-28] | physics/PhysicsWorld3D.cpp:368 | Raycast es O(N) brute force ignorando el BVH que ya existe | Usar BVH para raycast
- [2026-03-28] | physics/Constraints3D.h:277 + PhysicsWorld3D.cpp:224-226 | Double iteration loop: solver.iterations * world.iterations = 80 iteraciones | Quitar uno de los dos loops
- [2026-03-28] | renderer/DeferredRenderer.cpp:102-113 | glGetUniformLocation llamado por-objeto por-frame en vez de cacheado | Cachear como ForwardRenderer
- [2026-03-28] | core/ProceduralAudio.cpp:49 | Vibrato usa vibratoDept para rate en vez de vibratoRate — audio incorrecto | Cambiar a p.vibratoRate
- [2026-03-28] | ecs/EntityManager.h:61-63 | Entity{0} gen 0 es indistinguible del sentinel "not found" | Reservar index 0 o usar Optional
- [2026-03-28] | renderer/Texture2D.h:155-157 | wrapHandle() no libera handle previo si ya tenia ownership — GPU memory leak | Verificar y liberar handle existente
- [2026-03-28] | ecs/ECSCoordinator.h:289-322 | parallelForEach() crea std::thread por llamada (~100us por thread) | Usar JobSystem existente

## P3 — Deuda tecnica / cleanup

- [2026-03-28] | CMakeLists.txt (core) | Engine.cpp listado dos veces — warning o error de linker | Remover duplicado
- [2026-03-28] | CMakeLists.txt (math) | NavMesh.cpp incluido en libreria math — no pertenece ahi | Mover a su propio target
- [2026-03-28] | tests/test_pmm.c | Archivo de test del proyecto alze-os, no pertenece aqui | **FIXED** Eliminado
- [2026-03-28] | tests/test_memory.cpp | No esta en CMakeLists.txt — no se puede compilar | Agregar al build
- [2026-03-28] | raiz del repo | ~35 frames_*.csv + ~35 input_*.inp + test_physics3d.exe — artifacts de runtime | **FIXED** Agregado *.csv, *.inp, *.exe, *.o, *.a a .gitignore
- [2026-03-28] | recover.py, refactor.py | Scripts one-time ya ejecutados | **FIXED** Eliminados
- [2026-03-28] | math/Quaternion.h:144,178 | rotateVector() y rotate() son identicos — codigo duplicado | Eliminar uno, alias el otro
- [2026-03-28] | math/MathConstants.h | Duplica PI y EPSILON de MathUtils.h | Unificar en una sola fuente
- [2026-03-28] | ~20 physics headers (Quantum, Nuclear, Relativity, etc.) | Formula reference sheets sin simulacion real ni integracion a PhysicsWorld3D | Documentar como "reference only" o mover a physics/reference/
