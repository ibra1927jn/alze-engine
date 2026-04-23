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

- [2026-03-28] | core/StateManager.h:106-117 | Render loop de estados transparentes puede ir a indice -1 si no hay estado opaco debajo — out-of-bounds | **FIXED** Clamp firstVisible a 0, early return si vacio
- [2026-03-28] | core/EventBus.h | Sin mecanismo de unsubscribe — lambdas con captures se vuelven dangling pointers al destruir states | **FIXED** subscribe() retorna SubscriptionId, agregado unsubscribe() y unsubscribeAll<T>()
- [2026-03-28] | core/FrameAllocator.h | No es thread-safe pero se usa desde JobSystem workers — data race en s_offset | **FIXED** Agregado std::mutex con lock_guard en alloc() y reset()
- [2026-03-28] | core/Logger.h | No es thread-safe (s_file, s_lineCount) pese a incluir <mutex> — data race en output | **FIXED** Agregado lock_guard en log(), setFile(), closeFile(), flush()
- [2026-03-28] | core/Profiler.h | No thread-safe — data race si se profila desde workers | **FIXED** Agregado std::mutex con lock_guard en beginFrame/endFrame/begin/end/generateReport
- [2026-03-28] | core/ResourceManager.h:77,107 | has() y getAliveCount() no toman lock del mutex — data race | **FIXED** Agregado lock_guard en has() y getAliveCount()
- [2026-03-28] | renderer/ForwardRenderer.cpp:350-355 | Texture slots 4-6 colisionan entre IBL (irradiance/prefilter/brdfLUT) y material (emissive/AO/height) | **FIXED** IBL reasignado a slots 7-9, shadows a 10-11. Material mantiene 0-6
- [2026-03-28] | renderer/ForwardRenderer.cpp:334 | Material skip compara direcciones de copias en RenderItem3D — siempre false, optimizacion nunca activa | **FIXED** Agregado operator== a Material, comparacion por valor en vez de por puntero
- [2026-03-28] | physics/GJK.inl:94 | EPA nunca calcula contactPoint — convex hull collisions obtienen punto (0,0,0) | **FIXED** Interpolacion baricentrica de support points en la cara mas cercana del polytope; se trackean supportA/supportB por vertice
- [2026-03-28] | physics/PhysicsWorld3D.cpp:344-357 | Subsistemas fuera del substep loop reciben subDt en vez de dt — simulacion a velocidad incorrecta | **FIXED** Cambiado subDt a dt para fluid/EM/softbody/gravity/wave

## P2 — Bugs menores y performance

- [2026-03-28] | physics/Collider3D.cpp:253-263 | capsuleVsOBB usa 5 muestras discretas — pierde colisiones entre samples | **FIXED** Aumentado a 16 muestras para mejor cobertura del segmento
- [2026-03-28] | physics/PhysicsWorld3D.cpp:155-168 | Island building escanea TODOS los contactos por body — O(N*M) | **FIXED** Adjacency list construida una vez antes del island loop, lookup O(1) por vecino
- [2026-03-28] | physics/PhysicsWorld3D.cpp:368 | Raycast es O(N) brute force ignorando el BVH que ya existe | **FIXED** Raycast usa DynamicBVH3D::raycast() con callback — O(log N)
- [2026-03-28] | physics/Constraints3D.h:277 + PhysicsWorld3D.cpp:224-226 | Double iteration loop: solver.iterations * world.iterations = 80 iteraciones | **FIXED** Removido loop externo, ConstraintSolver3D::solve() ya itera internamente
- [2026-03-28] | renderer/DeferredRenderer.cpp:102-113 | glGetUniformLocation llamado por-objeto por-frame en vez de cacheado | **FIXED** Uniform locations cacheados en struct m_uniforms durante init(), patron igual a ForwardRenderer
- [2026-03-28] | core/ProceduralAudio.cpp:49 | Vibrato usa vibratoDept para rate en vez de vibratoRate — audio incorrecto | **FIXED** Cambiado p.vibratoDept a p.vibratoRate en sine de vibrato; default vibratoRate cambiado a 5.0f
- [2026-03-28] | ecs/ECSCoordinator.h:231 | Entity{0} gen 0 es indistinguible del sentinel "not found" | **FIXED** findFirstByTag retorna NULL_ENTITY (UINT32_MAX) en vez de Entity{0}
- [2026-03-28] | renderer/Texture2D.h:155-157 | wrapHandle() no libera handle previo si ya tenia ownership — GPU memory leak | **FIXED** Agregado glDeleteTextures antes de asignar nuevo handle si m_owned era true
- [2026-03-28] | ecs/ECSCoordinator.h:289-322 | parallelForEach() crea std::thread por llamada (~100us por thread) | **FIXED** Reemplazado con JobSystem::parallel_for(), fallback secuencial si no hay JobSystem

## P3 — Deuda tecnica / cleanup

- [2026-03-28] | CMakeLists.txt (core) | Engine.cpp listado dos veces — warning o error de linker | Remover duplicado
- [2026-03-28] | CMakeLists.txt (math) | NavMesh.cpp incluido en libreria math — no pertenece ahi | **FIXED** Movido a engine_core (que ya linkea engine_math); tambien corregido Engine.cpp duplicado en core
- [2026-03-28] | tests/test_pmm.c | Archivo de test del proyecto alze-os, no pertenece aqui | **FIXED** Eliminado
- [2026-03-28] | tests/test_memory.cpp | No esta en CMakeLists.txt — no se puede compilar | Agregar al build
- [2026-03-28] | raiz del repo | ~35 frames_*.csv + ~35 input_*.inp + test_physics3d.exe — artifacts de runtime | **FIXED** Agregado *.csv, *.inp, *.exe, *.o, *.a a .gitignore
- [2026-03-28] | recover.py, refactor.py | Scripts one-time ya ejecutados | **FIXED** Eliminados
- [2026-03-28] | math/Quaternion.h:144,178 | rotateVector() y rotate() son identicos — codigo duplicado | **FIXED** rotateVector() ahora delega a rotate()
- [2026-03-28] | physics/PhysicsWorld3D.cpp:122 | std::vector<vector<int>> adj heap-allocated cada substep en hot loop | **FIXED** Reemplazado con flat arrays via FrameAllocator (CSR format)
- [2026-03-28] | core/Engine.cpp:140 | std::ostringstream construido cada segundo para FPS display | **FIXED** Reemplazado con snprintf a buffer stack. Tambien corregido titulo de ventana a "ALZE Engine"
- [2026-03-28] | core/JobSystem.h:79 | std::vector<pair<int,int>> chunks allocado en cada parallel_for | **FIXED** Eliminado vector, chunks calculados inline con aritmetica. body capturada por referencia
- [2026-03-28] | physics/*.cpp | V * -1.0f en vez de -V (pierde ruta SIMD del operator-) | **FIXED** Reemplazado por negacion unaria en PhysicsWorld3D.cpp y Constraints3D.cpp
- [2026-03-28] | renderer/ForwardRenderer.cpp:273 | (Zero - dir).normalized() en vez de (-dir).normalized() — temporal innecesario | **FIXED** Usar negacion unaria directa
- [2026-03-28] | math/MathConstants.h | Duplica PI y EPSILON de MathUtils.h | **FIXED** Constants::PI/TAU/HALF_PI/EPSILON ahora alias de MathUtils::* (unica fuente de verdad)
- [2026-03-28] | ~20 physics headers (Quantum, Nuclear, Relativity, etc.) | Formula reference sheets sin simulacion real ni integracion a PhysicsWorld3D | Documentar como "reference only" o mover a physics/reference/
- [2026-03-29] | core/UISystem.h + editor/Editor.cpp | API mismatch: drawRoundedRectFilled, drawRectFilled, drawLine, drawCircleFilled, drawText no existen en ShapeRenderer2D/TextRenderer | **FIXED** Renombrado a roundedRectFill/rectFill/line/circleFill; TextRenderer.draw requiere SpriteBatch2D* almacenado en UISystem::begin(); convertidores toShapeCol/toSpriteCol para math::Color->float; coordenadas centradas via x+w*0.5f,y+h*0.5f; Editor::render ahora acepta SpriteBatch2D& param
- [2026-03-28] | renderer/ForwardRenderer.cpp:159-165 | sortByMaterial llamaba .get() en raw Texture2D* — no compila | **FIXED** Usar reinterpret_cast directo sobre raw pointers
- [2026-03-29] | physics/PhysicsWorld3D.h:4 | #include "CollisionSolver3D.H" con extension uppercase — falla en Linux CI (case-sensitive filesystem) | **FIXED** Cambiado a "CollisionSolver3D.h" (lowercase). Windows no detecta el error por ser case-insensitive
