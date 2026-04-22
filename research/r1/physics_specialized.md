# ALZE Engine — Physics especializados (Rust / 2D / research / GPU / soft)

## Overview

Este documento cubre los motores y papers que el sibling agent (PhysX/Havok/Bullet/Jolt) NO toca:

- **Rust-nativo**: Rapier (dimforge), integrado con Bevy vía `bevy_rapier`.
- **2D canónico**: Box2D v3 (Erin Catto, rewrite SoA 2024).
- **Research / RL**: MuJoCo (DeepMind, soft-constraint, Apache 2.0 desde 2022).
- **GPU / soft bodies**: NVIDIA FleX, PhysX 5 FEM, XPBD (Macklin et al.), PositionBasedDynamics lib (Bender/Müller).
- **Cloth**: mass-spring (Provot 1995), PBD/XPBD distance+bending constraints, self-collision.

El hilo común es la familia **Position-Based Dynamics** — Müller 2006 → XPBD 2016 → Small-Steps 2019 → PhysX 5 soft bodies / Blender cloth / FleX — y el contraste con el solver **sequential-impulse** de Catto, que sigue siendo el patrón oro para rigid bodies en videojuegos 2D/3D.

## Rapier

- **Qué es**: motor físico 2D+3D en Rust puro, por [dimforge](https://dimforge.com). Apache-2.0 / MIT.
- **Arquitectura** (fuente: `ARCHITECTURE.md`, blog de lanzamiento 2020):
  - Pipeline con `IslandManager` + `DefaultBroadPhase` (SAP-like) + `NarrowPhase` + `CCDSolver`.
  - Joints separados en `ImpulseJointSet` (reducción 6-DoF generalizada) y `MultibodyJointSet` (Featherstone-style reduced coords).
  - **Solver**: variación de **PGS con dos pasadas velocity-based** (la primera con regularización tipo soft-constraint, la segunda sin ella para stiffness final). Impulse joints usan sequential-impulse.
  - **Parallelism**: feature `parallel` = rayon; trabajo partido por islands.
  - **Determinismo**: feature `enhanced-determinism` da bit-level cross-platform (IEEE 754-2008) — raro en físicas y muy valioso para multiplayer/rollback.
  - **SIMD**: feature `simd-stable` / `simd-nightly`.
- **Integración Bevy**: `bevy_rapier2d` / `bevy_rapier3d` se registran como plugin ECS; sincroniza `Transform`/`RigidBody`/`Collider` vía systems.
- **2D y 3D mismo codebase**: types genéricos sobre `DIM`; una sola rama de features lógicos.

## Box2D v3

- **Qué es**: rewrite completo de Box2D por Erin Catto (anunciado 2023, release **agosto 2024**). La v2 (OO-style, 2006–2023) fue *el* físico 2D canónico durante 18 años.
- **Cambios arquitectónicos clave** ([Releasing Box2D 3.0](https://box2d.org/posts/2024/08/releasing-box2d-3.0/)):
  - **Data-Oriented Design / SoA**: body state empacado en 32 bytes — cuadra con registros AVX2 de 256 bits. Contact manifolds, velocities, posiciones todo en structs-of-arrays en lugar de punteros a clases.
  - **SIMD contact solver**: SSE2 / NEON / AVX2; [SIMD Matters](https://box2d.org/posts/2024/08/simd-matters/) documenta el uso de **graph coloring** para agrupar contacts sin conflictos de escritura y resolverlos en lanes paralelos.
  - **Persistent islands**: no se recalculan cada frame; soportan add/remove incremental. Threaded island splitting.
  - **Solver**: `TGS Soft` (renombrado "Soft Step") ganó entre 8 candidatos evaluados en [Solver2D](https://box2d.org/posts/2024/02/solver2d/) — combina sub-stepping, soft constraints (Hertz+damping ratio en lugar de Baumgarte), relaxation passes, warm starting.
  - **Perf**: v3 es **>2×** más rápido que v2.4 single-threaded; escala con MT.
- **Referencias doctrinales**: [SequentialImpulses GDC 2006](https://box2d.org/files/ErinCatto_SequentialImpulses_GDC2006.pdf), [IterativeDynamics GDC 2005](https://box2d.org/files/ErinCatto_IterativeDynamics_GDC2005.pdf), [ContactManifolds GDC 2007](https://box2d.org/files/ErinCatto_ContactManifolds_GDC2007.pdf), Modeling and Solving Constraints (GDC 2009) — lectura obligada para cualquier implementador.

## MuJoCo

- **Origen**: Emo Todorov (UW, 2012 paper con Erez+Tassa). Adquirido por DeepMind 2021, **open-sourced Apache 2.0 mayo 2022**.
- **Modelo de contacto soft**: optimización convexa tipo **Gauss's principle** — minimiza desviación de la aceleración no-restringida sujeto a constraints suavizados. Evita LCP/complementarity rígido; contact cone puede ser piramidal o elíptico. Ver [MuJoCo: Computation](https://mujoco.readthedocs.io/en/stable/computation/index.html) y Todorov ICRA 2014 "Convex and analytically-invertible dynamics with contacts and constraints".
- **Articulaciones en coordenadas mínimas**: Featherstone-style generalized coordinates; inverse dynamics funciona incluso con contacts (rareza en engines de juegos). MuJoCo NO usa Featherstone O(n) forward dynamics porque la fase de impulsos requiere la matriz de inercia completa.
- **Escena**: XML **MJCF** (MuJoCo scene format) — compilado a representación runtime eficiente.
- **MuJoCo 3 (2023) + MJX**: port a JAX/XLA; millones de steps/segundo en GPU/TPU — base de `mujoco_playground` para RL sim-to-real.
- **Uso**: benchmarks de RL (Control Suite, Gymnasium), investigación robótica, biomecánica. NO es motor de juego: el soft-contact da soluciones continuas e invertibles (ideal para control óptimo y gradiente) pero se siente "blando" para impactos arcade.

## XPBD — eXtended Position Based Dynamics

- **Paper**: Macklin, Müller, Chentanez, *XPBD: Position-Based Simulation of Compliant Constrained Dynamics*, **Motion in Games (MIG) 2016**.
- **Motivación**: PBD clásico (Müller et al. VRIPhys 2006) tenía stiffness dependiente del número de iteraciones y del time step → pesadilla para artistas.
- **Idea central**: introduce un parámetro de **compliance** α̃ = α/Δt² (inverso de stiffness) que reemplaza el concepto de stiffness. El solver proyecta constraints con un multiplicador de Lagrange λ acumulado, llegando al mismo resultado que un integrador implícito backward-Euler en el límite de iteraciones — pero **estable incondicionalmente** con pasos grandes.
- **Aplicaciones unificadas**: cloth (distance + bending), soft bodies (distance + volume / shape-matching / FEM constraints), fluids (density constraints tipo PBF), hair (stretch-shear + bend-twist Cosserat rods). Mismo solver, distintas constraints.
- **Extensión práctica — Small Steps** (Macklin et al., SCA 2019, [mmacklin.com/smallsteps.pdf](https://mmacklin.com/smallsteps.pdf)): **n sub-steps con 1 iteración cada uno** supera a 1 step con n iteraciones. Reduce damping artificial y error de constraints. Es el patrón que usa PhysX 5 FEM soft bodies y muchas implementaciones modernas.
- **Adopción**: Blender cloth (desde 2.82), Roblox, Omniverse, múltiples engines indie; el paradigma "XPBD para todo lo deformable" está normalizado.

## PositionBasedDynamics (lib, Bender / Müller-style)

- **Repo**: [InteractiveComputerGraphics/PositionBasedDynamics](https://github.com/InteractiveComputerGraphics/PositionBasedDynamics), autor principal **Jan Bender**. C++, MIT.
- **Por qué importa**: implementación de referencia *leíble* de toda la familia PBD/XPBD. Si quieres copiar fórmulas sin equivocarte, lees aquí antes que el paper.
- **Cubre**: rigid bodies con joints (ball/hinge/universal/slider/distance/damper/motor), deformable solids (distance, bending, volume, shape-matching, FEM, strain-based), elastic rods (stretch-shear + bend-twist Cosserat), position-based fluids, collision detection con signed distance fields cúbicos.
- **Bindings Python** (`pyPBD`) para prototipado.
- **Survey acompañante**: Bender, Müller, Macklin, *Position-Based Simulation Methods in Computer Graphics*, Eurographics 2017 tutorial — la referencia unificadora.

## NVIDIA FleX

- **Paper fundacional**: Macklin, Müller, Chentanez, Kim, *Unified Particle Physics for Real-Time Applications*, **ACM TOG / SIGGRAPH 2014**. Ganó el **Test-of-Time 2025** de SIGGRAPH.
- **Idea**: TODO es partículas conectadas por constraints. Rigid bodies (shape-matching constraints), cloth (distance+bending), fluids (density), gases (vorticity), soft bodies (cluster constraints) — solver único paralelo PBD en GPU.
- **Implementación**: librería CUDA cerrada (`NvFlex.h` core + `NvFlexExt.h` open extensions). Usada por Killing Floor 2, Batman Arkham Knight (PhysX FleX cape), etc.
- **Status**: congelada como librería standalone; sus ideas absorbidas en **PhysX 5 GPU** (mismo equipo). Archivo histórico pero aún referenciado.

## PhysX 5 GPU soft body + cloth

- Continuación directa de FleX dentro de [PhysX 5](https://nvidia-omniverse.github.io/PhysX/physx/5.4.1/docs/SoftBodies.html).
- **Soft bodies**: **FEM con mallas tetraédricas**; dos mallas — *simulation mesh* (baja-res para dinámicas) y *collision mesh* (alta-res para respuesta precisa). Generador interno `Tet Maker` (voxeliza en 5–6 tets por voxel).
- **Cloth**: position-based / XPBD con distance+bending, self-collision BVH.
- **Requisito**: GPU CUDA (vendor lock-in).
- **Integrador**: XPBD small-steps para estabilidad.
- **Acoplamiento**: rigid + articulation + soft + cloth + particles bajo un solver unificado.

## Cloth simulation techniques

| Técnica | Año | Pros | Contras |
|---|---|---|---|
| Mass-spring + Provot strain limiting | 1995 | Simple, clásico | Stiffness explota con dt grande; bending necesita diagonales hack |
| Baraff-Witkin implicit | 1998 | Estable con dt grande | LCG/Krylov complejo, damping artificial |
| PBD distance | 2006 | Real-time, simple | Stiffness depende de #iter y dt |
| XPBD distance + dihedral/isometric bending | 2016+ | Stiffness independiente de iter; estable | Requires compliance tuning |
| FEM triangular | 2000s+ | Físicamente correcto | Pesado; mejor para film |

Para **ALZE** la apuesta dominante es **XPBD distance + isometric-bending + self-collision broadphase con BVH por triángulo**. Viento = fuerza externa simple. Attachments = pin constraints (distance con mass→∞).

## Broadphase + narrowphase en 2D (Box2D) vs 3D

- **2D**: Box2D v2 usaba **Dynamic Tree (DBT)**; v3 lo mantiene pero con threading. SAT para box-box es trivial en 2D (solo 2 ejes por box → 4 tests). GJK opcional para polígonos arbitrarios.
- **3D**: SAP (Sweep-And-Prune) es estándar en Rapier/Bullet; BVH dinámico (Dbvt) en Bullet/PhysX. SAT 3D tiene 15 ejes (3+3+9 cross products) entre boxes — más caro. GJK+EPA es el narrowphase universal para convexos arbitrarios.
- **Lección**: 2D puede usar estructuras más simples y baratas; no portar ciegamente el pipeline 3D al 2D pensando que "es solo Z=0".

## Solver comparison

| Solver | Usado por | Convergencia | Mass ratios grandes | Complejidad |
|---|---|---|---|---|
| **Sequential Impulse** (Catto 2006) | Box2D v2, Rapier (joints) | Buena, warm-start | Regular | Baja — implementable en 500 LOC |
| **PGS** (Projected Gauss-Seidel) | Bullet, Rapier, ODE | Lenta sin warm-start | Pobre | Baja |
| **TGS** (Temporal GS / Small Steps) | PhysX 4/5, Box2D v3 ("Soft Step") | Excelente vía sub-stepping | Muy buena | Media — requiere reintegrar pose cada sub-step sin rehacer broadphase |
| **XPBD** | PBD lib, PhysX 5 soft, Blender cloth, FleX | Excelente con compliance | Excelente | Baja para constraints, media para acoplar con rigid |
| **NGS Block** (non-linear GS) | Box2D v2.4 | Buena | Regular | Alta |

**Cuándo gana cada uno**:
- **SI (Catto)**: rigid-body puro para juegos, cuando quieres código simple y predecible. Default sensato.
- **TGS/Small-Steps**: simulaciones con mass ratios grandes (vehículo + plumón), stacking alto, soft bodies.
- **XPBD**: cualquier cosa deformable. Unifica cloth+soft+fluid bajo un solo paradigma.
- **PGS**: cuando ya está escrito y funciona; no es first-choice para nuevo código en 2026.

## En qué son buenos

- **Rapier**: Rust-safe, determinismo bit-level cross-platform (multiplayer rollback), 2D+3D compartiendo codebase, licencia permisiva, parallelism de primera clase.
- **Box2D v3**: referencia canónica 2D; rendimiento top-tier post-rewrite; documentación GDC de Catto es pedagógicamente inmejorable.
- **MuJoCo**: gold standard académico para RL/control; diferenciable; contact suave invertible; Apache 2.0; MJX GPU.
- **XPBD / PBD lib**: unifica cloth, soft, fluid en un solo formalismo; estable con dt grande; simple de implementar; compliance artística intuitiva.
- **FleX / PhysX 5 GPU**: performance masiva en GPU; acoplamientos cruzados "free" entre fluido-cloth-rigid-soft.

## En qué fallan

- **Rapier**: Rust — si tu engine es C++, integrarlo implica FFI no-trivial y el modelo de borrow/ownership choca con ECS C++. Ecosistema de tooling menor que PhysX.
- **Box2D**: estrictamente 2D. Si tu juego va a 3D algún día, no lo extiendes.
- **MuJoCo**: **no es un motor de juego**. Soft-contact suaviza impactos → vehículos y pelotas sienten "pegajosos" para gameplay arcade. Rendering mínimo. Overhead XML para escenas simples.
- **XPBD**: compliance tuning no-intuitivo la primera vez; cloth puede sentirse "squishy" si la compliance es alta; acoplar con rigid bodies masivos no es trivial.
- **FleX**: **CUDA-only, vendor-locked a NVIDIA**. Sin Intel/AMD/Apple/ARM. Closed-source core.
- **PhysX 5 soft**: requiere GPU; CPU path para soft inexistente.

## Qué podríamos copiar para ALZE Engine

1. **Patrón SoA de Box2D v3 para contacts y body-state** — struct-of-arrays con body-state alineado a 32 bytes (16 en 2D); itera cache-friendly, lista para SIMD cuando toque. Esto NO es invención sino disciplina.
2. **Sequential-Impulse (Catto) como solver default para rigid bodies 3D** — más simple que PGS/TGS, excelente track record (Box2D v2, Rapier joints), pedagogía pública abundante. Arrancar aquí y migrar a Small-Steps/TGS solo si aparecen casos duros (mass ratios extremos).
3. **XPBD como ÚNICO paradigma para cloth + soft bodies + ropes** — distance constraints, isometric bending (ver [Carmen Cincotti's guide](https://carmencincotti.com/2022-09-05/the-most-performant-bending-constraint-of-xpbd/)), shape-matching para soft blobs. Sustituye mass-spring en todas sus formas.
4. **Small-Steps sub-stepping** (Macklin 2019) para el loop XPBD — n sub-steps × 1 iter, no 1 step × n iter. Aplica también al solver rigid si separamos acumulador.
5. **Islands + graph coloring** (Box2D v3 / Rapier) — agrupar bodies interactuantes en islands independientes, asignar colores a contacts dentro de cada island, resolver colores en paralelo (rayon-equivalent en C++ = Intel TBB o std::execution::par).
6. **PositionBasedDynamics lib como referencia pedagógica** — cuando implementes una constraint, compara contra esa implementación antes del paper.
7. **MuJoCo-style soft contact opcional para wheels/tires/rag-dolls** (no para gameplay general) — si algún día ALZE quiere simular vehículos realistas o robots, el solver secundario soft vale la pena.
8. **Determinismo opcional à la Rapier** — feature flag que garantiza IEEE 754 + orden de operaciones estable; crítico si ALZE piensa en networking lockstep.

## Qué NO copiar

- **Dependencia CUDA de FleX** — mata portabilidad (AMD, Apple Silicon, consolas). ALZE debe ser CPU-first con GPU como aceleración opcional vía compute shaders portables (Vulkan/Metal/DX12), no vendor SDK.
- **FEM tetraédrico completo estilo PhysX 5** — demasiado peso (mesher, dos mallas, solver FEM) para el scope realista de ALZE. XPBD cloth + shape-matching soft cubren 95% del uso.
- **Modelo de ownership Rust de Rapier** — C++ tiene otros idiomas de seguridad (smart pointers, RAII, const-correctness); no forzar bordes Rust-style en API C++.
- **MuJoCo como engine principal de juego** — su contact model suave, aunque académicamente elegante, no es para impactos arcade.
- **PGS legacy como default** — en 2026 no hay razón para elegirlo sobre Sequential-Impulse o TGS Small-Steps para código nuevo.
- **NGS block solver de Box2D v2** — Catto mismo lo jubiló en v3 por Soft-Step; no lo resucites.

## Recomendación práctica para ALZE

Arquitectura de tres capas, toda C++17, cero dependencias vendor-lock:

1. **Broadphase**: SAP (3D) + Dynamic Tree (2D opcional). Paralelo por sweep axis.
2. **Rigid-body solver**: **Sequential-Impulse a la Catto** con warm-starting, baumgarte sustituido por soft-constraints (Hertz + damping ratio como Box2D v3). Islands + graph coloring + paralelismo por islands (std::execution::par o TBB).
3. **Deformable solver**: **XPBD small-steps** (n=4..8 sub-steps típico) con:
   - Distance constraints (ropes, cloth structural).
   - Isometric bending constraints (cloth).
   - Shape-matching clusters (soft bodies).
   - Contact constraints (body↔cloth, cloth self).
4. **Determinismo**: flag compile-time `ALZE_DETERMINISTIC` que fuerza orden de iteración y desactiva paralelismo no-determinista.
5. **Diferir** FEM tetraédrico, fluidos PBF, GPU offloading a fases futuras; escribir el API pensando en ellas pero no implementar ya.

Referencia pedagógica primaria: Erin Catto GDC 2006/2009/2014 + Macklin XPBD 2016 + Small-Steps 2019 + PositionBasedDynamics lib como código de verificación.

## Fuentes consultadas

- [dimforge/rapier](https://github.com/dimforge/rapier) + [ARCHITECTURE.md](https://github.com/dimforge/rapier/blob/master/ARCHITECTURE.md)
- [Announcing Rapier (Dimforge 2020)](https://dimforge.com/blog/2020/08/25/announcing-the-rapier-physics-engine/)
- [bevy_rapier](https://github.com/dimforge/bevy_rapier)
- [erincatto/box2d](https://github.com/erincatto/box2d)
- [Box2D: Releasing 3.0 (2024)](https://box2d.org/posts/2024/08/releasing-box2d-3.0/) · [SIMD Matters](https://box2d.org/posts/2024/08/simd-matters/) · [Solver2D](https://box2d.org/posts/2024/02/solver2d/) · [Simulation Islands](https://box2d.org/posts/2023/10/simulation-islands/)
- Erin Catto GDC PDFs: [SequentialImpulses 2006](https://box2d.org/files/ErinCatto_SequentialImpulses_GDC2006.pdf) · [IterativeDynamics 2005](https://box2d.org/files/ErinCatto_IterativeDynamics_GDC2005.pdf) · [ContactManifolds 2007](https://box2d.org/files/ErinCatto_ContactManifolds_GDC2007.pdf) · Modeling and Solving Constraints GDC 2009
- Macklin, Müller, Chentanez, *XPBD: Position-Based Simulation of Compliant Constrained Dynamics*, **MIG 2016** ([PDF](https://matthias-research.github.io/pages/publications/XPBD.pdf))
- Müller, Heidelberger, Hennix, Ratcliff, *Position Based Dynamics*, **VRIPhys 2006** / JVCIR 2007
- Macklin, Storey, Lu, Terdiman, Chentanez, Jeschke, Müller, *Small Steps in Physics Simulation*, **SCA 2019** ([PDF](https://mmacklin.com/smallsteps.pdf))
- Macklin, Müller, Chentanez, Kim, *Unified Particle Physics for Real-Time Applications*, **ACM TOG / SIGGRAPH 2014** (Test-of-Time 2025)
- Bender, Müller, Macklin, *Position-Based Simulation Methods in Computer Graphics*, **Eurographics 2017 Tutorial**
- [InteractiveComputerGraphics/PositionBasedDynamics](https://github.com/InteractiveComputerGraphics/PositionBasedDynamics) (Jan Bender)
- [MuJoCo](https://mujoco.org/) + [docs: Computation](https://mujoco.readthedocs.io/en/stable/computation/index.html) + [MJX](https://github.com/google-deepmind/mujoco/blob/main/mjx/README.md)
- Todorov, *Convex and analytically-invertible dynamics with contacts and constraints: Theory and implementation in MuJoCo*, **ICRA 2014**
- Todorov, Erez, Tassa, *MuJoCo: A physics engine for model-based control*, **IROS 2012**
- [NVIDIA FleX Developer](https://developer.nvidia.com/flex) + [FleX 1.1 manual](https://docs.nvidia.com/gameworks/content/gameworkslibrary/physx/flex/index.html) + [Macklin's blog](http://blog.mmacklin.com/project/flex/)
- [PhysX 5 Soft Bodies docs](https://nvidia-omniverse.github.io/PhysX/physx/5.4.1/docs/SoftBodies.html)
- [Carmen Cincotti XPBD series](https://carmencincotti.com/2022-08-08/xpbd-extended-position-based-dynamics/) — distance + isometric bending + most-performant bending
- Provot, *Deformation Constraints in a Mass-Spring Model to Describe Rigid Cloth Behavior*, **Graphics Interface 1995**
