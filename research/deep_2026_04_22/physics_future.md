# El futuro de los motores de física 2026-2032

**Target:** ingenieros de `/root/repos/alze-engine` — decisiones de roadmap Physics a 3-10 años.
**Fecha:** 2026-04-22.
**Relación con research previa:** complementa [`physics_3d_industry.md`](../physics_3d_industry.md) (PhysX/Havok/Bullet/Jolt, estado 2026) y [`physics_specialized.md`](../physics_specialized.md) (Rapier/Box2D v3/MuJoCo/XPBD/FleX). Foco aquí: **proyección**, **papers 2023-2025 y roadmaps de vendors**, **qué apuestas hace un equipo pequeño cuando el estado del arte cambia bajo sus pies**.

ALZE stack de referencia: C++17, `-fno-exceptions -fno-rtti`, SDL2+OpenGL 3.3, ~25-30K LOC, Fase 7 (PBR+ECS+Physics+Audio). Equipo small (1-3 ingenieros). Roadmap 3-10 años.

---

## TL;DR — las 8 apuestas que definen 2026-2032

1. **GPU-default physics llega primero a deformables, tarde a rigid.** PhysX 5 liberó todo el código GPU bajo BSD-3 en 2025 (rigid + fluid + soft + cloth unificados). Pero PhysX sigue siendo el único motor AAA con rigid-bodies GPU en producción; Jolt 5, Havok, Box2D no lo tienen ni en roadmap cercano. Para un engine pequeño: **no perseguir GPU rigid antes de 2028**. Sí considerar GPU particles (XPBD/PBF) porque el coste de no tenerlo crece si el juego quiere fluidos masivos.

2. **XPBD está siendo superado.** Vertex Block Descent (Chen et al. SIGGRAPH 2024) y Augmented VBD (Giles-Diaz-Yuksel SIGGRAPH 2025) resuelven los defectos más criticados de XPBD (stiffness-iteration coupling, mass-ratio extremes, constraint graph coloring subóptimo). Para ALZE: **apostar XPBD sigue siendo razonable en 2026-2027, pero diseñar la API pensando VBD como migración 2028+**.

3. **Differentiable physics no llega a game engines generalistas antes de 2030.** Brax, MuJoCo MJX, NVIDIA Warp, Newton (2025) son stacks Python-first orientados a robótica/RL. En games solo llega como **feature de herramientas offline** (fit de parámetros, animación AI) — no como pilar del runtime. Para ALZE: **ignorar como scope del core**. Exponer hooks para poder bolsillear un wrapper MJX más tarde si el juego lo necesita.

4. **Neural physics surrogates no reemplazan solvers clásicos en games — todavía.** GameNGen (Google 2024), Microsoft Muse (2025), Decart Minecraft (2025) son demos research de juego-entero-neural. Ningún AAA shipped sustituye su solver. GNS/MeshGraphNets (DeepMind 2020/2022) son para engineering, no gameplay. Para ALZE: **wait and see — revisar 2028-2030**.

5. **Determinism resurge, no por lockstep puro sino por rollback/replay.** Street Fighter 6, todos los fighting games post-2022, algunos RTS y multiplayer shooters ahora exigen simulación bit-idéntica para re-simular inputs. Rapier (`enhanced-determinism`), Box2D v3, Jolt (`CROSS_PLATFORM_DETERMINISTIC`) y **Havok 2025.1** invirtieron en esto. Para ALZE: **feature flag `ALZE_DETERMINISTIC` desde v1**, aunque el juego target no lo use.

6. **Affine Body Dynamics (Lan-Kaufman-Li-Jiang-Yang 2022) es el sleeper técnico.** IPC-based, intersection-free, 10,000× speedup sobre IPC vanilla. Aún no en PhysX ni Jolt pero es el frontrunner para stiff rigids con contact duro de alta calidad (bloques que se apilan sin penetrar). Para ALZE: **leer el paper, no implementar v1**; considerar como destino 2030+ si el juego necesita acaparación de masas rígidas de alta fidelidad.

7. **Jolt es la apuesta segura C++ para 2026-2030.** Horizon Forbidden West, Death Stranding 2, Godot add-on oficial, soft-bodies XPBD ya añadidos en 4.0. No tiene GPU pero el autor lo ha dicho como roadmap. Para ALZE: **si pivotamos de "write our own" a "use dependency", Jolt > PhysX > Bullet sin dudas**. Si mantenemos "write our own", **seguir el patrón de Jolt 4.x** (broadphase lock-free, islands, XPBD soft, TGS solver opcional).

8. **Robotics cross-over cambia físicas de juegos en los próximos 5 años.** Newton (NVIDIA+DeepMind+Disney+Linux Foundation 2025, sobre Warp) y MuJoCo Playground (2025) están empujando contact-aware differentiable sim a millones de steps/s en GPU. Isaac Lab entrena a todo humanoid robot que se vende. Este *gradient-first, GPU-parallel, contact-correct* paradigma eventually baja a games. Para ALZE: **no copiarlo ya; reservar espacio API para que "algo-tipo-Warp" pueda acelerar un subset de ALZE para R&D/animación AI en v3**.

---

## 1. GPU-parallel physics unification

### Estado 2026

El único stack AAA con **rigid + fluid + soft + cloth unificados en GPU** es PhysX 5.
Trayectoria técnica:

- **PhysX FleX** (Macklin-Müller-Chentanez-Kim 2014, *Unified Particle Physics for Real-Time Applications*, ACM TOG 33(4)). Todo-es-partícula + shape-matching + density constraints. Ganó **SIGGRAPH 2025 Test-of-Time**. Librería CUDA cerrada.
- **PhysX 4.0** (dic 2018): código CPU liberado BSD-3. GPU seguía precompilado.
- **PhysX 5.0** (nov 2022): API completa + FEM soft bodies + PBD particle system (heredero directo de FleX) + SDF collision. Solver TGS default desde 5.1.
- **PhysX 5.6 + Flow 2.2** (abril 2025): **TODO el código GPU liberado bajo BSD-3, +500 CUDA kernels**. Blast y Flow bundled. Fin del binary-blob.

Cobertura unificada en la misma GPU infra:
- Rigid bodies (TGS en GPU o CPU).
- FEM soft bodies con sim/collision mesh dual; acoplados con rigid vía contact.
- PBD particles → fluids, cloth, inflatables (paradigma FleX portado).
- SDF collision (dynamic triangle mesh con SDF, CPU también desde 5.4).

El solver PhysX GPU corre en **TGS sub-stepping**; la misma representación de constraint sirve rigid joints, FEM tetra, PBD distance — por eso "unificación" no es solo marketing.

### Proyección 2026-2030

- **2026-2027**: PhysX GPU sigue siendo NVIDIA-preferred. AMD/Intel via CUDA-on-ROCm y SYCL son experimentales. ALZE con GL3.3 no es cliente.
- **2027-2028**: Jolt añade GPU (rumoreado por Jorrit Rouwe en discussion #1263 del repo). Si llega, será primero soft-body (XPBD en compute shaders), después rigid.
- **2028-2030**: **Newton physics engine** (NVIDIA+DeepMind+Disney 2025, Linux Foundation) se convierte en el stack "GPU physics portable" porque está sobre Warp, no CUDA directo. Warp emite a CUDA, HIP (AMD), y el backend Metal está en roadmap.
- **2030+**: expectativa razonable: PhysX GPU port de Warp-style (no-CUDA) disponible; Jolt GPU estable; Havok Particles GPU expandido a rigid.

### Por qué ALZE no adopta GPU physics ya

- GL3.3 core no tiene compute shaders estables (extensión `ARB_compute_shader` existe pero la adopción en drivers antiguos es desigual).
- Scope: **~25-30K LOC** no aguanta el tax de dual-path (CPU/GPU) que PhysX paga.
- El 90% de juegos indie/AA no necesita GPU physics; la hot path es <5K rigids + cloth <50K verts + fluid opcional. Todo cabe en CPU SIMD si el solver está bien escrito (ver Box2D v3 +2× sobre v2 single-threaded).

**Decisión**: CPU-first con hooks para offload de particles/cloth a compute shaders cuando el backend RHI soporte Vulkan/D3D12 (v2 ALZE).

### Fuentes clave

- Macklin et al., *Unified Particle Physics for Real-Time Applications*, ACM TOG 33(4), 2014. <https://mmacklin.com/uppfrta_preprint.pdf>
- NVIDIA Developer blog, *Open Source Simulation Expands with NVIDIA PhysX 5 Release*, 2022 y 2025. <https://developer.nvidia.com/blog/open-source-simulation-expands-with-nvidia-physx-5-release/>
- *NVIDIA Makes PhysX & Flow GPU Code Open-Source*, Phoronix 2025. <https://www.phoronix.com/news/NVIDIA-OSS-PhysX-Flow-GPU>
- PhysX 5.4 docs, *GPU Simulation*. <https://nvidia-omniverse.github.io/PhysX/physx/5.4.1/docs/GPURigidBodies.html>

---

## 2. Differentiable physics

### Qué es y por qué importa

Un simulador es *differentiable* si para cada output y(θ) (trayectoria, pose final, coste) puede computar ∂y/∂θ por reverse-mode autodiff a través del solver. Esto habilita:

- **System identification**: ajustar parámetros (friction, elasticity, mass) para que sim match datos reales.
- **Policy gradient RL**: backpropagar el reward hasta la acción del agente *a través* de la física, no solo vía score function estimator.
- **Inverse design**: "qué geometría minimiza drag?" resuelto por descenso de gradiente sobre la geometría.
- **Motion fitting / animation AI**: resolver control para que un humanoid haga un front-flip sin mocap.

### Papers y stacks 2021-2025

- **Brax** — Freeman-Frey-Raichuk-Bachem 2021, arXiv 2106.13281, NeurIPS Datasets+Benchmarks. JAX. 100-1000× training speedup vs workstation. Rigid bodies masivos en accelerator. <https://arxiv.org/abs/2106.13281>
- **MuJoCo XLA (MJX)** — Google DeepMind 2023, port JAX de MuJoCo. Corre en NVIDIA+AMD GPU, Apple Silicon, TPU. **Gradient computation via autodiff** (pero el contact soft smoothing limita accuracy). <https://mujoco.readthedocs.io/en/stable/mjx.html>
- **DiSECt** — Heiden-Macklin et al. RSS 2021, *A Differentiable Simulation Engine for Autonomous Robotic Cutting*; versión journal Autonomous Robots 2023. FEM + SDF contact + damage springs. Diferenciable para cutting trajectory optimization. <https://diff-cutting-sim.github.io/>
- **NVIDIA Warp** — Macklin 2022+, framework Python DSL → CUDA. Autogenera kernels forward+adjoint. Interop PyTorch/JAX. SciPy 2024 / GTC 2024 sesión S63345. Benchmarks: 669× over CPU, >250× over JAX. <https://github.com/NVIDIA/warp>
- **Dojo** — Howell et al. Stanford 2022, arXiv 2203.00806. Differentiable physics con **primal-dual interior-point** para nonlinear complementarity problem + second-order cone; hard contact + friction. Julia. <https://arxiv.org/abs/2203.00806>
- **DiffMJX / Hard Contacts with Soft Gradients** — 2025, arXiv 2506.14186. Refina MJX para que hard contacts tengan gradientes útiles sin smoothing artificial. <https://arxiv.org/html/2506.14186v1>
- **Newton** — NVIDIA+DeepMind+Disney, sept 2025, Linux Foundation. Sobre Warp. Multiphysics differentiable. GPU. OpenUSD asset format. <https://developer.nvidia.com/blog/announcing-newton-an-open-source-physics-engine-for-robotics-simulation/>
- **PixelBrax** — Bamford 2025, arXiv 2502.00021. Brax + pixels end-to-end on GPU. RL from vision. <https://arxiv.org/html/2502.00021v1>
- **Single-Level Differentiable Contact Simulation** — Le Lidec et al. IEEE RA-L 2023. <https://ieeexplore.ieee.org/document/10105986/>

### Qué es diferenciable hoy

| Componente | Rigid joints | Hard contact | Soft FEM | Fluid PBF | Cutting/fracture |
|---|---|---|---|---|---|
| Brax | sí | smoothed | — | — | — |
| MJX | sí | smoothed | limitado | — | — |
| Warp | sí | sí (con trucos) | sí | sí | — |
| DiSECt | sí | sí (FEM SDF) | sí | — | sí |
| Dojo | sí | sí (hard via primal-dual) | — | — | — |
| Newton 1.0 | sí | sí | sí | sí (en progreso) | — |

**Hard contact differentiation** es el problema duro. Contact is discontinuous in configuration space; derivatives no existen en colisión. Soluciones:
- **Smoothing** (MJX, Brax v1): reemplaza contact hard por barrier suave. Gradientes OK, física "pegajosa".
- **Implicit function theorem** (Dojo, Single-Level): resuelve el forward con IP method, diferencia a través del optimum con IFT. Gradientes más caros pero sin artifacts.
- **Randomized smoothing** (ContactNets, Pfrommer 2021): sampling. Caro pero general.

### Proyección 2026-2032

- **2026-2027**: differentiable physics se consolida en robótica (Newton + Isaac Lab + MJX). NO entra en game engines.
- **2027-2028**: animación AI-driven en games usa differentiable sim offline para fit de controllers. Unity Muse-style herramientas. Pero el runtime del juego ejecuta un solver clásico.
- **2028-2030**: posible primer game engine con tooling differentiable integrado (Unity o UE) para inverse design de ragdolls y physics-based animation. Runtime sigue clásico.
- **2030-2032**: diferenciable podría empezar como feature de runtime para R&D features (difficulty tuning AI, physics-based adaptive opponents), no como pilar.

### Para ALZE

- **No implementar** differentiable sim en el core. El coste (dual-pass forward/adjoint, memory tape) cuadruplica LOC.
- **Reservar arquitectura**: API del solver debe permitir extraer snapshots state antes/después de cada step sin modificar `PxScene`-style state interno. Esto deja abierto el wrap con autograd si algún día hace falta.
- Si ALZE quiere animación AI estilo DeepMimic/AMP/ASE, **entrenar offline con Brax o Warp**, deploy runtime-only en C++.

### Fuentes

- Freeman et al., Brax. <https://arxiv.org/abs/2106.13281>
- Peng et al., DeepMimic, ACM TOG 37(4), 2018. <https://xbpeng.github.io/projects/DeepMimic/>
- Peng et al., AMP: Adversarial Motion Priors, SIGGRAPH 2021. <https://xbpeng.github.io/projects/AMP/>
- Peng et al., ASE: Adversarial Skill Embeddings, SIGGRAPH 2022. <https://xbpeng.github.io/projects/ASE/>
- Howell et al., Dojo, 2022. <https://arxiv.org/abs/2203.00806>
- NVIDIA Warp publications list. <https://github.com/NVIDIA/warp/blob/main/PUBLICATIONS.md>
- Heiden et al., DiSECt, RSS 2021. <https://diff-cutting-sim.github.io/>

---

## 3. Neural physics surrogates

### El espectro

Dos regímenes:
- **Augmentation**: neural net predice correcciones / acelera partes del solver (e.g. preconditioner neural, surrogate subspace, neural material model).
- **Full replacement**: red reemplaza el solver entero; input = state t, output = state t+1.

### Papers + demos 2020-2025

- **Learning to Simulate** — Sanchez-Gonzalez et al., DeepMind, ICML 2020. *Graph Network-based Simulators (GNS)*. Encoder-processor-decoder con message passing sobre partículas. Generaliza a 10× más partículas de las vistas en entrenamiento. Fluidos, goops, rígidos. <https://arxiv.org/abs/2002.09405>
- **MeshGraphNets** — Pfaff et al. DeepMind, ICLR 2021. GNN sobre mesh, no partículas. Aerodynamics, cloth, structural. <https://openreview.net/forum?id=roNqYL0_XP>
- **X-MeshGraphNet** — NVIDIA 2024, arXiv 2411.17164. Multi-scale GNN para engineering scale CFD surrogates. <https://arxiv.org/pdf/2411.17164>
- **PhysGaussian** — Xie et al. CVPR 2024 Highlight. 3D Gaussian Splatting + MPM. Gaussian kernels son las partículas. Elastic, plastic, non-Newtonian, granular. Rendering y sim misma representación. <https://xpandora.github.io/PhysGaussian/>
- **GASP** — Borycki et al. 2024, arXiv 2409.05819. Gaussian Splatting + física más clásica. <https://arxiv.org/html/2409.05819>
- **SNUG** — Santesteban-Otaduy-Casas CVPR 2022. Self-supervised neural garments. 2 órdenes magnitud más rápido que supervised. Calidad interactive, no AAA. <https://arxiv.org/abs/2204.02219>
- **Neural Cloth Simulation** — Santesteban et al. 2022+. <https://ar5iv.labs.arxiv.org/html/2212.11220>
- **GameNGen** — Valevski et al. Google, arXiv 2408.14837, ICLR 2025 poster. Diffusion model genera frames de DOOM en 20 fps. Es un juego-neural, no un solver-neural. <https://arxiv.org/abs/2408.14837>
- **Microsoft Muse** — MS Research + Ninja Theory, feb 2025. Generative model de Bleeding Edge. Simula comportamiento de juego completo. Research tool, no shipping. <https://www.microsoft.com/en-us/research/blog/introducing-muse-our-first-generative-ai-model-designed-for-gameplay-ideation/>
- **Decart Minecraft** — march 2025. Diffusion + transformer simula Minecraft neural.

### Cost vs accuracy hoy

GNS-style surrogates típicamente:
- Training: días-semanas en 4-8 GPUs.
- Inference: 10-100× faster que solver clásico si particle count es fijo. Peor si escala cambia.
- Accuracy: ~cm-level error en 1000 steps para fluidos. Useful para animation, inaceptable para engineering precisión.

Neural full-replacement (GameNGen):
- TPU dedicado. $1-4/hora de DOOM. No comercial.
- Stability multi-minuto, degrada a largo plazo.
- **0 games shipped con este paradigma en 2026**.

### Proyección 2026-2032

- **2026-2028**: neural surrogates ship como *aceleradores* de partes no-crítica (background cloth, distant water). Nunca el pilar.
- **2028-2030**: neural materiales (DiffHand, NCA-physics) entran en tools DCC para "paint a material, model learned it". Runtime sigue clásico.
- **2030-2032**: posible primer juego full-neural-engine en un género especializado (narrative adventure, puzzle) donde physics fidelity no es crítica. Mainstream action/FPS sigue solvers clásicos por latencia y determinismo.

### Para ALZE

- **Ignorar como core**. Demasiado research-grade.
- **API hook**: dejar el solver factorizado para poder reemplazar cloth sub-solver con un modelo neural offline-trained (rare case, si el juego lo justifica).
- **PhysGaussian como inspiración** para rendering-sim coupling en futuro lejano, NO ahora.

---

## 4. XPBD evolution → VBD/AVBD

### La saga Macklin

- **PBD original** — Müller-Heidelberger-Hennix-Ratcliff, VRIPhys 2006 / JVCIR 2007. *Position Based Dynamics*.
- **XPBD** — Macklin-Müller-Chentanez, MIG 2016. Compliance α̃ = α/Δt² desacopla stiffness de iter count. <https://mmacklin.com/xpbd.pdf>
- **Small Steps** — Macklin-Storey-Lu-Terdiman-Chentanez-Jeschke-Müller, SCA 2019. n sub-steps × 1 iter >> 1 step × n iter. <https://mmacklin.com/smallsteps.pdf>
- **XPBD Detailed Rigid Bodies** — Müller et al. 2020. Rigid body simulation con XPBD puro (no sequential-impulse). <https://matthias-research.github.io/pages/publications/PBDBodies.pdf>
- **Stable Neo-Hookean** — Macklin-Müller 2021. *A Constraint-based Formulation of Stable Neo-Hookean Materials*. Soft FEM-equivalent vía XPBD constraints. Estable con poisson ratio → 0.5.
- **XPBI** — 2024, arXiv 2405.11694. XPBD + smoothing kernels para continuum inelasticity. <https://arxiv.org/html/2405.11694v2>

### La amenaza: Vertex Block Descent

- **VBD** — Chen-Macklin-Müller-Kim-Jiang, **SIGGRAPH 2024**, ACM TOG 43(4). Block coordinate descent sobre la variational form de implicit Euler. Vertex-level Gauss-Seidel iterations. Opera directamente sobre fuerzas, sin convertir a constraints. <https://graphics.cs.utah.edu/research/projects/vbd/vbd-siggraph2024.pdf>
  - Mejor convergencia que XPBD con igual budget iter.
  - Parallelism via vertex graph coloring (menos colores que constraint graph de XPBD → mejor paralelismo).
  - Sin el "approximation error" de XPBD que diverge a Δt grandes y mass ratios extremos.
  - Unconditionally stable.
  - **Roblox lo publicó**: están considerando/usando VBD. <https://about.roblox.com/publications/vertex-block-descent>

- **Augmented VBD** — Giles-Diaz-Yuksel, **SIGGRAPH 2025**, ACM TOG 44(4). Extiende VBD con Augmented Lagrangian para hard constraints (contacts, stacking). Real-Time Live! 2025 demo. <https://graphics.cs.utah.edu/research/projects/avbd/Augmented_VBD-SIGGRAPH25.pdf>
  - El "rigid-body stacking" que XPBD nunca consiguió de verdad.
  - GPU-friendly.
  - Chris Giles: real-time GPU performance demostrado.

- **Going Further With Vertex Block Descent** — Saillant 2025, Wiley CAVW. Extensions. <https://onlinelibrary.wiley.com/doi/10.1002/cav.70039>

### Alternativas no-XPBD

- **Projective Dynamics** — Bouaziz-Martin-Liu-Kavan-Pauly 2014. Local/global alternating solver. Bridge entre FEM nodal y PBD. Real-time para cloth/solids/shells. <https://users.cs.utah.edu/~ladislav/bouaziz14projective/bouaziz14projective.pdf>
  - Mil2 (Yang et al. 2024) y Subspace-Preconditioned GPU PD (Xu et al. 2023) empujan PD a GPU con contacts no-distance barriers. <https://arxiv.org/html/2403.19272v4>
- **Affine Body Dynamics** — Lan-Kaufman-Li-Jiang-Yang, ACM TOG 41(4), SIGGRAPH 2022. Relaja constraint rigid por stiff orthogonality potential; mantiene IPC guarantees (intersection-free, inversion-free). **10,000× speedup** vs IPC vanilla. <https://arxiv.org/abs/2201.10022>

### Para ALZE

Trayectoria de implementación recomendada:

1. **2026-2027 (v1 ALZE)**: XPBD Small-Steps para cloth + rope + soft blobs. Distance + isometric-bending + shape-matching constraints. API limpia.
2. **2027-2028 (v1.5)**: añadir Stable Neo-Hookean (Macklin-Müller 2021) como constraint set opcional para jellyfish/meat-style deformables.
3. **2028-2029 (v2)**: prototipar VBD en branch paralelo. Si el vertex-coloring + gauss-seidel da >2× throughput al mismo quality → migrar cloth a VBD, mantener XPBD para rope/soft que ya funciona.
4. **2030+ (v3)**: si el juego escala a stacking masivo rigid (Tetris-style destruction pile), considerar AVBD para la capa rigid-stiff.

**Regla**: XPBD no es error en 2026. Es la única implementación completa con código público leíble (PositionBasedDynamics lib, Bender). VBD es mejor **en 2028** cuando el paper tenga 2-3 implementaciones open y sus edge cases estén mapeados.

### Fuentes

- Chen et al., *Vertex Block Descent*, SIGGRAPH 2024. <https://dl.acm.org/doi/10.1145/3658179>
- Giles-Diaz-Yuksel, *Augmented Vertex Block Descent*, SIGGRAPH 2025. <https://dl.acm.org/doi/10.1145/3731195>
- Lan et al., *Affine Body Dynamics*, SIGGRAPH 2022. <https://dl.acm.org/doi/10.1145/3528223.3530064>
- Bouaziz et al., *Projective Dynamics*, SIGGRAPH 2014. <https://dl.acm.org/doi/10.1145/2601097.2601116>
- Müller et al., *Detailed Rigid Body Simulation with Extended Position Based Dynamics*, 2020. <https://matthias-research.github.io/pages/publications/PBDBodies.pdf>

---

## 5. Fluids next-gen

### Taxonomía 2026

- **Grid-based Eulerian**: MAC grids, pressure Poisson solve. Stam's *Stable Fluids* 1999 base. Offline VFX dominante. No games.
- **SPH Lagrangian**: partículas vecinos kernel. Real-time factible.
  - **WCSPH** (Becker-Teschner 2007): weakly compressible. Legacy.
  - **PCISPH** (Solenthaler-Pajarola 2009): predictive-corrective incompressible. Mejor que WCSPH.
  - **IISPH** (Ihmsen et al. 2013): implicit incompressible. Stable timestep grande.
  - **DFSPH** — Bender-Koschier SCA 2015, *Divergence-Free Smoothed Particle Hydrodynamics*; TVCG 2017 para incompressible+viscous. **20× speedup** sobre state-of-art SPH. Density-invariant + divergence-free dos pressure solvers combinados. <https://animation.rwth-aachen.de/media/papers/2015-SCA-DFSPH.pdf> · <https://www.animation.rwth-aachen.de/media/papers/2017-TVCG-ViscousDFSPH.pdf>
- **PBF — Position-Based Fluids** — Macklin-Müller SIGGRAPH 2013. Density constraint via XPBD-esque solver. Fluids + PBD todo en un pipeline. Core de FleX y PhysX 5 particles. <https://mmacklin.com/pbf_sig_preprint.pdf>
- **Hybrid Eulerian-Lagrangian**:
  - **PIC** (Harlow 1964): particles advect, grid pressure. Dissipative.
  - **FLIP** (Brackbill-Ruppel 1986): reintroducido gaming por Zhu-Bridson SIGGRAPH 2005. Less dissipative, más noisy.
  - **APIC** — Jiang-Schroeder-Selle-Teran-Stomakhin, SIGGRAPH 2015, *The Affine Particle-In-Cell Method*. Conserva momento lineal+angular. No acumula velocity modes. <https://dl.acm.org/doi/10.1145/2766996>
  - **PolyPIC** — Fu et al. SIGGRAPH Asia 2017. Polynomial extension. <https://dl.acm.org/doi/10.1145/3130800.3130878>
- **MPM — Material Point Method**: grid-based continuum transfer + particle state. Stomakhin-Schroeder-Jiang-Chai-Selle-Teran, **Disney Frozen 2013** (snow). Big Hero 6, Zootopia. Ahora papers: multi-material, sand, foam, elastoplastics. Enhanced MPM con affine projection stabilizer 2025 (Visual Computer). <https://link.springer.com/article/10.1007/s00371-025-03953-2>

### Games shipped con fluids next-gen

- **PhysX PBD particles**: Batman Arkham Knight (cape), Killing Floor 2, Polynormal Reforged. Todos con GPU PhysX path.
- **Teardown** (Tuxedo Labs 2020-2022): voxel-based destruction + water. Custom engine.
- **Noita** (Nolla Games 2020): falling-sand cellular automata, no MPM/SPH. 1M pixels simulados.
- **MPM en games AAA 2024-2025**: **ninguno documentado** a scale Disney-Frozen. Offline VFX solo. El coste computacional CPU de MPM denso (APIC transfer + grid solve) es ~10-100× SPH para similar visual. GPU MPM está research-grade, no engine de juegos.

### Proyección 2026-2032

- **2026-2028**: PBF/XPBD particles es el estándar games. DFSPH entra en engines mid-tier si alguien lo porta con GPU compute.
- **2028-2030**: primer AAA con MPM running (probablemente UE plugin Niagara-MPM, o plugin custom en un Rockstar/Naughty Dog). Escenas acotadas: una waterfall, una pile de arena, no open world.
- **2030-2032**: MPM mainstream games si GPU memory bandwidth duplica y motores abstraen el pipeline lo suficiente.

### Para ALZE

- **v1**: NO implementar fluids. Scope-kill.
- **v2**: particles con XPBD/PBF si el juego lo justifica. Distance + density constraints. CPU primero, GPU-compute cuando RHI tenga Vulkan.
- **v3**: MPM solo si: (a) juego es sim de arena/nieve/líquidos muy central; (b) hay 1+ ingeniero full-time dedicado 6+ meses.

---

## 6. Cloth / hair / deformables shipped AAA

### Estado de hair

- **NVIDIA HairWorks** — GameWorks legacy. Strand-based + TressFX-like. Descontinuado en UE4.25+. <https://www.fxguide.com/fxfeatured/ue4-real-time-hair-advances/>
- **AMD TressFX** — open alternative. Tomb Raider (2013) showcase; rara vez shipped AAA directo.
- **UE5 Groom** — strand-based, pipeline completo. Niagara-integrated. Metahuman está sobre Groom. Cinematic quality.
- **Frostbite hair** — proprietary EA, Charles de Rousiers GDC + SIGGRAPH talks. <https://www.ea.com/frostbite/news>
- **Metahuman groom** — UE plugin, estándar para next-gen character cinematics. Shipped en cinematic modes, no siempre gameplay-quality por coste.

Hair sim core es **Cosserat rods** (stretch-shear + bend-twist) o mass-spring con stiffness tricks. Real-time viable hasta ~50K strands con LOD + interpolated guides. AAA 2024-2025 shipping: Alan Wake 2 (Northlight), Hellblade II (UE5 Groom), Senua's Saga.

### Estado de cloth

Techniques en AAA 2024-2026:
- **XPBD/PBD** dominante. UE5 Chaos Cloth, PhysX, Jolt 4.0 soft.
- **Subspace methods** (Kim-Pollard 2019, Brandt 2018): offline cloth, real-time playback de trained modes. Shipped en VFX pipelines, no games.
- **Learned cloth**:
  - **SNUG** (Santesteban et al. CVPR 2022): self-supervised neural dynamic garments. 2 orders magnitud faster than supervised. No shipped games, solo VR/avatar demos. <https://mslab.es/projects/SNUG/>
  - **Neural Cloth Simulation** (Santesteban et al. 2022+). <https://ar5iv.labs.arxiv.org/html/2212.11220>
  - **HOOD** (Grigorev et al. CVPR 2023): recurrent graph NN para cloth on bodies.

### Thin-shell elasticity

- **Grinspun et al. 2003** — Discrete Shells. Hinge bending energy sobre triangle mesh. Textbook foundation.
- **Bergou et al. 2008** — Discrete Elastic Rods. Cosserat rods discrete.
- Aplicación games: UE5 Chaos hair, Houdini Vellum, Blender cloth.

### Para ALZE

- **v1**: cloth XPBD distance + isometric bending + self-collision BVH triangulos. Meta: 5K tris interactive, 50K offline-quality.
- **v1.5**: ropes Cosserat rods (o XPBD distance+twist approximation).
- **v2**: hair strands. Estructura ID-array de strands, cada strand = cadena de XPBD distance + bend constraints. LOD via guide interpolation. No ship ≥ AAA-Metahuman calidad.
- **No** implementar learned cloth. Entrenarlo requiere capacity que ALZE no tiene, y el runtime GPU-compute es v2+.

### Fuentes

- Santesteban et al., SNUG, CVPR 2022. <https://arxiv.org/abs/2204.02219>
- UE5 Groom docs. <https://dev.epicgames.com/documentation/en-us/unreal-engine/groom-for-hair-in-unreal-engine>
- Grinspun et al., Discrete Shells, SCA 2003.
- Bergou et al., Discrete Elastic Rods, SIGGRAPH 2008.

---

## 7. Soft body / rigid hybrid: cuándo gana cada uno

Tabla de decisión 2026:

| Técnica | Cost/vertex | Stability | Mass ratios | Code volume | Sweet spot |
|---|---|---|---|---|---|
| Mass-spring legacy | bajo | mal sin damping explícito | pobre | ~500 LOC | 1990s nostalgia |
| FEM tetrahedral (Baraff-Witkin, Neo-Hookean) | alto | buena con implicit | buena | ~5-10K LOC | VFX offline, medical |
| XPBD | bajo | unconditional | regular | ~2-3K LOC | games 2020-2027 dominante |
| Shape-matching (Müller 2005) | bajo | buena | regular | ~1K LOC | blobs, jelly, deformable debris |
| Projective Dynamics | medio | buena | buena | ~3-5K LOC | cloth real-time, soft solids |
| IPC (Li et al. 2020) | muy alto | garantías formales | excelente | ~15-20K LOC | research, Houdini, IPC-sim |
| Affine Body Dynamics | medio | IPC-level | excelente | ~8-10K LOC | stiff rigids next-gen |
| VBD (Chen 2024) | bajo | unconditional | buena | ~2-3K LOC estimado | cloth+soft 2028+ |
| AVBD (Giles 2025) | bajo-medio | IPC-level | buena | ~4-5K LOC estimado | rigid stacking 2028+ |

Para ALZE scope (indie-AA, small team): **XPBD soft + shape-matching + rigid clásico** cubre 90%. FEM tetra es trampa (Disney/Pixar budget). IPC es trampa (1 ingeniero full-time años).

### Fuentes

- Müller et al., *Meshless Deformations Based on Shape Matching*, SIGGRAPH 2005.
- Li et al., *Incremental Potential Contact*, SIGGRAPH 2020. <https://ipc-sim.github.io/>
- Lan et al., *Affine Body Dynamics*, SIGGRAPH 2022.

---

## 8. Rigid body solver futures

### Solver taxonomy refresh

| Solver | Inventor | Usado por | Estado 2026 |
|---|---|---|---|
| Projected Gauss-Seidel (PGS) | Academia 1990s | Bullet legacy, ODE | Legacy |
| Sequential Impulses (Catto 2006) | Erin Catto, GDC 2006 | Box2D v2, Rapier joints | Dominante games |
| Temporal Gauss-Seidel (TGS) | NVIDIA | PhysX 5 default since 5.1 | Creciendo |
| TGS Soft / Soft Step | Catto Solver2D 2024 | Box2D v3 default | Creciendo |
| XPBD rigid | Müller 2020 | PBD lib, Roblox | Minoritario |
| Featherstone O(n) | Featherstone 1983 | PhysX articulation, MuJoCo | Robotics |
| IPC barrier | Li et al. 2020 | IPC-sim, Houdini | Offline solo |
| ABD | Lan 2022 | research | Futuro |
| AVBD | Giles 2025 | research, demos | Futuro |

### Catto's Solver2D project (2024)

Erin Catto publicó **Solver2D** testbed en 2024 comparando 8 solvers (PGS, NGS, NGS-block, TGS, TGS Soft, PBD, XPBD, SI). Conclusión: **TGS Soft** (sub-stepping + soft Hertz/damping + relaxation + warm-starting) superó a todos los demás incluido XPBD en tareas que XPBD dominaba. Box2D v3 (ago 2024) shippeó TGS Soft como default. <https://box2d.org/posts/2024/02/solver2d/>

Este es el dato más importante para small teams: **el "XPBD para todo" paradigma está siendo superado por TGS-substepping con soft constraints** para rigid bodies. XPBD sigue ganando para deformables.

### Convex optimization approaches

MuJoCo (Todorov 2012, ICRA 2014) resuelve contact + constraints como **convex QP** con Gauss principle. Trade-off: soluciones continuas invertibles ideales para differentiable, **soft feel** para game contact. Usable en games solo donde ese feel es aceptable (cars stylized, ragdolls soft).

### Neural constraint solvers

Research 2023-2025:
- Learning-based preconditioners (Chen et al. SIGGRAPH Asia 2023).
- Neural-ODE dynamics.
- Ninguna adoption en engines games.

### Para ALZE

- **v1**: Sequential Impulse (Catto GDC 2006) con warm-starting y persistent contacts. 500-1000 LOC core. Bien entendido.
- **v1.5**: migrar a TGS Soft (replicar Box2D v3 approach en 3D). +500 LOC.
- **v2**: XPBD Small-Steps para soft (ya cubierto en sección 4).
- **v3 opcional**: AVBD para stiff rigid stacking si la hot path del juego lo necesita.
- **Featherstone**: diferir a fase posterior cuando articulations complejas (ragdoll vehicles, arachnid bosses) justifiquen.

### Fuentes

- Catto, *Solver2D*, 2024. <https://box2d.org/posts/2024/02/solver2d/>
- Catto, *Releasing Box2D 3.0*, 2024. <https://box2d.org/posts/2024/08/releasing-box2d-3.0/>
- Catto, *SIMD Matters*, 2024. <https://box2d.org/posts/2024/08/simd-matters/>
- Todorov et al., MuJoCo, IROS 2012; Todorov ICRA 2014.

---

## 9. Large-scale destruction + fracture

### Toolkits

- **Chaos Destruction** (UE5) — Geometry Collections, Voronoi pre-fracture, clustering strain. UE5.6 optimizations (Niagara Data Channels, fracture anchoring). 2025 GDC talk *Dynamic Destruction in UE5 with the Chaos Destruction System*. <https://gdcvault.com/play/1035357/Dynamic-Destruction-in-UE5-with>
- **NVIDIA Blast SDK** — replaza APEX Destruction. 3 layers (NvBlast, NvBlastTk, NvBlastExt). Voronoi sites, FractureTool, runtime fracture extension. Bundled con PhysX 5.6 (2025). <https://github.com/NVIDIAGameWorks/Blast>
- **Havok Destruction** — closed middleware. Used historically in Alan Wake, Red Faction legacy.

### Techniques

- **Voronoi static pre-fracture**: divide mesh en cells por semillas random, store chunks. Runtime: break cell-graph constraints when force exceeds threshold.
- **Dynamic Voronoi runtime**: generate cells on impact point. Caro, raramente shipped; Teardown-style es voxel-based, no triangle Voronoi.
- **Boolean/CSG runtime**: subtract impact shape. libigl, Blender Bool. Real-time limited.
- **FEM fracture**: simulate crack propagation via FEM stress + cohesive zone elements. Offline VFX (Houdini), no games.

### Games shipped 2024-2026

- **Alan Wake 2** (Remedy/Northlight 2023): dynamic destruction en scripted scenes.
- **The Finals** (Embark 2023): UE5 + Chaos, heavy destruction gameplay pillar.
- **Teardown** (Tuxedo 2020): voxel-based, custom.
- **Armored Core VI** (FromSoft 2023): mesh-based impact Voronoi, proprietary.
- **Battlefield series** (DICE/Frostbite): Levolution/Destruction 2.0, proprietary.

### Para ALZE

- **v1**: no destruction.
- **v2**: pre-fracture Voronoi via offline tool (igl, V-HACD-style). Runtime: constraint graph between chunks, break on impulse > threshold. No dynamic Voronoi.
- **v3**: si el juego es destruction-centric, adoptar Blast SDK (BSD-3, bundled PhysX 5.6) como dependency, evitar reinventar.

### Fuentes

- UE5 Chaos Destruction docs. <https://dev.epicgames.com/documentation/en-us/unreal-engine/chaos-destruction-in-unreal-engine>
- NVIDIA Blast GitHub. <https://github.com/NVIDIAGameWorks/Blast>
- Bao-James-Marchner SIGGRAPH 2007 *Fracturing Rigid Materials* (textbook paper).

---

## 10. Determinism + lockstep multiplayer resurgence

### Por qué vuelve

- **Rollback netcode** es el estándar fighting-games desde GGPO (Cannon 2006, shipped Skullgirls, Street Fighter V Beta, **Street Fighter 6 2023**). Requiere **re-simulación**: guarda state, reproduce inputs futuros al llegar. Determinism bit-identical es dura requirement.
- **RTS resurge**: Stormgate, Immortal: Gates of Pyre (2024-2025). StarCraft heritage. Lockstep puro o hybrid.
- **Multiplayer physics co-op** (Satisfactory, Palworld 2024): no lockstep, pero snapshot+authority. No demanda bit-identical.
- **Replay systems**: CS2, Valorant. Demanda repro exacta a partir de seed+inputs.

### Estado de engines 2026

| Engine | Determinism | Modo |
|---|---|---|
| Havok Physics | **bitwise cross-platform** | Stateful, default |
| Rapier | `enhanced-determinism` feature | IEEE 754-2008, bit-level cross-platform |
| Jolt | `CROSS_PLATFORM_DETERMINISTIC` compile flag | Controlled FP order |
| Box2D v3 | Single-thread deterministic | Documented |
| PhysX 5 | Same-hardware only | Not cross-platform |
| Bullet | Single-thread single-platform | Legacy |

Havok 2025.1 (julio 2025) **dobló down en determinism** como marketing angle. Talk con practical tips para cross-platform: fast-math ordering, multi-threaded model, compiler bugs. <https://www.windowscentral.com/gaming/the-havok-physics-engine-is-making-a-pitch-to-developers-in-2025>

### Para ALZE

Feature flag `ALZE_DETERMINISTIC` desde v1:
- Fixed iteration order en solver (sort by stable body ID).
- No `std::unordered_map` en hot path (hash orden inestable).
- Single-thread solver pass en deterministic mode (sacrifica throughput).
- `#pragma STDC FENV_ACCESS ON` + disable fast-math en determ build.
- Documentar el contract: "bajo flag X, dos máquinas IEEE 754-2008 producen bit-idéntico frame 60s después".

Esto es barato de construir de entrada y carísimo de retrofit. Si ALZE jamás hace lockstep, no pierdes nada. Si lo necesitas año 5, pagaste 1 semana en año 1.

### Fuentes

- Cannon, GGPO docs. <https://www.ggpo.net/>
- Rapier determinism docs. <https://rapier.rs/docs/user_guides/rust/determinism/>
- Jolt deterministic sim docs. <https://jrouwe.github.io/JoltPhysics/md__docs_2_determinism.html>
- Gaffer on Games, *Deterministic Lockstep*. <https://gafferongames.com/post/deterministic_lockstep/>
- Havok 2025.1 pitch. <https://www.havok.com/blog/>

---

## 11. Robotics crossover

### Lo que robotics aporta a games

- **Contact solvers rigurosos**: IPC, ABD, Dojo. Hard contact con garantías formales. Games tradicionalmente han aceptado penetración/jitter; robotics no.
- **Differentiable pipelines**: fit de controllers, material estimation.
- **Scale**: Isaac Lab corre miles de agentes paralelos en una GPU. Games con un cliente no lo necesitan, pero servidores de MMO/simulations sí.
- **OpenUSD assets**: Newton + Isaac Lab + Omniverse forman un ecosystem unificado. Pipeline de assets es más rico que cualquier game-engine exclusive format.

### Stacks robotics 2026

- **MuJoCo** (DeepMind, Apache 2.0). Modelo contact soft, convex.
- **MuJoCo Playground** (2025). <https://arxiv.org/html/2502.08844v1>
- **Isaac Sim** / **Isaac Lab** (NVIDIA). Built on PhysX + Omniverse. Sim-to-real para humanoids (1X, Agility, Boston Dynamics, Unitree, Fourier, Galbot, XPENG).
- **Newton** (NVIDIA+DeepMind+Disney 2025, Linux Foundation). GPU diferenciable sobre Warp. Multiphysics. <https://developer.nvidia.com/newton-physics>
- **Drake** (MIT TRI). Multibody dynamics + optimization.
- **Dojo** (Stanford). Differentiable Julia.
- **ROS 2 Gazebo** (Classic): sim + ROS bridge. Legacy.

### Qué pueden games adoptar

- **Warp-style DSL**: ALZE no necesita DSL, pero el concepto de "emit kernels CPU+GPU from one source" es relevante si ALZE quiere compute shaders portables.
- **Contact-aware constraint frameworks**: si un futuro ALZE quiere vehicles realistas, el MuJoCo soft-constraint approach es 20% del trabajo para 80% del feel.
- **OpenUSD asset pipeline**: ALZE usa glTF+KTX2 actualmente. USD es mucho más rico (variants, compositing, references). Over-engineering en 2026, opción 2030+.

### Para ALZE

- **No copiar robotics stacks** directamente. Son research-grade overhead.
- **Consumir sus papers**: ABD, IPC, Dojo, DiSECt dan ideas de algoritmos concretos adaptables.
- **Asset pipeline**: mantener glTF+KTX2. Re-evaluar USD en 2029 cuando adoption en games consolide.

### Fuentes

- MuJoCo Playground, arXiv 2502.08844, 2025. <https://arxiv.org/html/2502.08844v1>
- Newton announcement. <https://blogs.nvidia.com/blog/newton-physics-engine-openusd/>
- Isaac Lab. <https://developer.nvidia.com/isaac-lab>

---

## 12. AI-driven animation + physics blend

### Techniques timeline

- **Motion Matching** (Clavet, Ubisoft GDC 2016): no ML, matching de frames mocap. Shipped Assassin's Creed Origins+, Ghost Recon.
- **PFNN — Phase-Functioned Neural Networks** (Holden-Komura-Saito SIGGRAPH 2017): 4 networks blended by cycle phase. <https://theorangeduck.com/media/uploads/other_stuff/phasefunction.pdf>
- **Learned Motion Matching** (Büttner et al., Ubisoft SIGGRAPH 2020): compresión NN de motion matching. 70× memory improvement. <https://dl.acm.org/doi/abs/10.1145/3386569.3392440>
- **DeepMimic** (Peng-Abbeel-Levine-van de Panne SIGGRAPH 2018): RL imitation learning + physics simulation. Ragdoll front-flip. <https://xbpeng.github.io/projects/DeepMimic/>
- **AMP** (Peng et al. SIGGRAPH 2021): Adversarial Motion Priors, GAN discriminator. Style preservation.
- **ASE** (Peng et al. SIGGRAPH 2022): Adversarial Skill Embeddings. Latent skill space. Used in Isaac Gym demos.
- **MaskedMimic** (Peng et al. ACM TOG 2024, SIGGRAPH Asia): Unified physics-based character control through masked motion inpainting. <https://dl.acm.org/doi/10.1145/3687951>
- **C·ASE** (2023): Conditional ASE. <https://dl.acm.org/doi/10.1145/3610548.3618205>

### Shipped

- **Motion Matching** (2016-2024): estándar AAA. Ubisoft, 4A, Arkane, Guerrilla.
- **Learned Motion Matching**: Ubisoft shipped en For Honor 2022 update, The Division 2 pipelines.
- **Physics-based RL character (AMP/ASE)**: NO shipped en AAA juego mainstream 2024. Research demos (NVIDIA Isaac Gym, Meta VR), no gameplay.
- **Active Ragdoll + procedural**: Gang Beasts, Human Fall Flat (2016-2020), Fall Guys. Simple PD controllers + IK. No ML.

### Por qué no ha llegado ASE/AMP a games

1. **Training cost** enorme (días en 8 GPUs per character).
2. **Retargeting a new rigs** is open problem.
3. **Authorable style**: animators quieren control fino, RL-trained gives opaque behavior.
4. **Latency**: inference + physics sim per frame caro.

### Proyección 2026-2032

- **2026-2028**: Motion Matching + Learned MM siguen siendo el default. Physics-based RL entra en prototype/experimental features (secondary NPCs, ragdoll reaction polish).
- **2028-2030**: primer AAA shipping con AMP/ASE-style physics controller en un subset de characters (bosses, enemies con unique locomotion).
- **2030-2032**: pipeline maduro donde animators autorean con mocap + RL lo entrena + runtime physics-driven. La frontera animación/física se difumina.

### Para ALZE

- **v1**: NO intentar ML character control. Motion-matching tabular (Clavet 2016) es razonable, puro C++ sin ML.
- **v2**: exposer API "driven ragdoll" con PD controllers + IK. Animator-friendly blend entre kinematic anim y physics reaction.
- **v3 opcional**: si ALZE consigue R&D bandwidth, entrenar AMP offline y deploy un runtime-only inference (ONNX Runtime C++) como feature de subset characters.

### Fuentes

- Clavet, *Motion Matching and The Road to Next-Gen Animation*, GDC 2016.
- Holden-Komura-Saito, PFNN, SIGGRAPH 2017.
- Peng et al., DeepMimic SIGGRAPH 2018; AMP SIGGRAPH 2021; ASE SIGGRAPH 2022; MaskedMimic 2024.
- Büttner et al., Learned Motion Matching, SIGGRAPH 2020.

---

## 13. Tabla comparativa engines 2026 vs proyección 2030

Columnas: estado hoy (2026) | apuesta 2028 | apuesta 2030.

| Engine | 2026 | 2028 proyección | 2030 proyección | Licencia |
|---|---|---|---|---|
| **PhysX 5** | GPU unified + open-source full (5.6). TGS default. FEM soft, PBD particles, SDF. | PhysX 6 con Warp integration. GPU-portable (non-CUDA). Newton como branch robotics. | PhysX mainline sobre Warp DSL. Jolt-quality clean code. AMD/Apple GPU support. | BSD-3 |
| **Havok** | Closed. 2025.1 determinism push. Indie pricing. Still AAA-dominant (Destiny, Helldivers). | Microsoft tiene incentivo de open-source parcial contra PhysX (Xbox native). Determinism moat. | Havok Physics Particles matures; Havok Cloud Simulation para MMO servers. | Proprietary |
| **Bullet 3** | Mantenimiento Coumans (NVIDIA). No major release post-2019. PyBullet 3.2.7 2025 para RL/robotics. | Quasi-abandono como game engine; vive en robotics Python. | Historical artifact. Replaced por Newton/Jolt. | Zlib |
| **Jolt** | 5.x con soft bodies XPBD, TGS opcional. Godot endorsed addon. Shipped HFW, DS2. No GPU. | Jolt 6 con GPU compute (soft body primero, rigid después). Establish Rolls-Royce small-team choice. | Jolt dominante entre AA/indie. AAA sigue Havok/PhysX por scale. | MIT |
| **Box2D v3** | 2024 rewrite SoA+SIMD, TGS Soft default. 2× v2. Determinism single-threaded. | v3.x con improved multithreading, WebAssembly first-class. | Canonical 2D. No 3D extension. | MIT |
| **Rapier** | 3D+2D, Rust. `enhanced-determinism`. bevy_rapier. Avian XPBD fork. | Rapier 1.0 stable. C bindings más maduros. Posible GPU compute via wgpu. | Rust games consolidan; Rapier standard. C++ AAA sigue Jolt/PhysX. | Apache/MIT |
| **Chaos (UE5)** | UE5-only. Geometry Collections, Chaos Cloth, Chaos Vehicles. | UE6 Chaos con Niagara deeper integration. GPU physics expansion. | UE-locked. Not usable fuera del engine. | UE EULA |
| **MuJoCo** | DeepMind Apache 2.0. MJX on JAX/XLA. MuJoCo Playground 2025. Robotics gold standard. | MJX-Warp merged. DiffMJX standardized. | Multi-backend (JAX+Warp+native). Runs robotics ecosystem. No games. | Apache 2.0 |
| **Newton** | v1.0 sept 2025. NVIDIA+DeepMind+Disney. Warp-based, diferenciable, GPU. | Isaac Lab default. Replaces PhysX in Isaac Sim. | Consumido por multiple downstream (incluido PhysX rendering-sim coupling). | Apache 2.0 |

### Para ALZE: recomendación 2026-2032

- **Si ALZE permanece standalone C++17 write-your-own**: pattern-after-Jolt 5.x arquitectura (broadphase lock-free, islands, sequential-impulse → TGS Soft, XPBD soft). No adopt PhysX (SDK tax demasiado alto para scope). No adopt Rapier (FFI C++/Rust es coste permanente).
- **Si ALZE pivotea a "use dependency"**: Jolt 5.x es la única respuesta sensata. MIT, C++17 clean, no exceptions/RTTI, shipped AAA. Fallback a PhysX 5.6 si ALZE necesita GPU particles/fluids antes que Jolt GPU.
- **Si ALZE añade networking lockstep**: Rapier-style determinism flag C++. No ship sin validation multi-platform bit-identical.
- **Si ALZE quiere research AI-driven animation**: train offline con Warp/MJX, deploy C++ runtime.

---

## 14. Si alze-engine apuesta X, acierta/falla porque...

### Apuesta A: "XPBD universal, sequential-impulse rigid"

- **Acierta si**: el scope queda en AA/indie con cloth/rope/soft + rigid moderado (<2K bodies active). Cubre 85% de games sin ML animation. Código público abundante. Small team puede mantener.
- **Falla si**: el juego escala a stacking masivo rigid (Red Faction Guerrilla, Teardown). XPBD rigid no es competitivo con TGS Soft 3D. Si ALZE target es destruction-heavy, pivotar a TGS Soft para rigid desde v1.

### Apuesta B: "Keep Sequential-Impulse, add TGS Soft later"

- **Acierta si**: v1 prioriza tiempo-a-markets. Catto 2006 SI es textbook, portable, 500 LOC. Upgrade a TGS Soft año 2 si benchmark lo demanda.
- **Falla si**: benchmarks v1 tumban ALZE por drift/mass-ratio. Catto mismo migró Box2D v3 a TGS Soft porque SI puro ya no competía.

**Mi call**: B. Sequential-Impulse v1, TGS Soft v1.5 (año 2).

### Apuesta C: "GPU physics obligatorio pronto"

- **Acierta si**: ALZE target es PC/console AAA con open-world physics (Watch Dogs, GTA-style). GPU es el único way a 100K+ bodies sim.
- **Falla si**: ALZE es 25-30K LOC engine indie-AA. GPU physics tax = 3-5× LOC, dual codepath CPU/GPU, driver bugs, Vulkan/D3D12 requirement. No justificable antes 2028-2029.

**Mi call**: GPU physics no antes de 2028. Pre-2028 solo GPU *particles* (fluidos/cloth PBF) opcional cuando RHI tenga Vulkan.

### Apuesta D: "Adopt Jolt como dependency"

- **Acierta si**: ALZE quiere shippear un juego real en 12-18 meses. Jolt es MIT, C++17, no-exceptions-no-RTTI-friendly, 20K LOC shipped AAA. Saves 2-3 años de physics R&D.
- **Falla si**: ALZE es proyecto pedagógico ("own every line"). O si Jolt decisions (lock-free broadphase no-world-bounds, fat samples) no caben en ALZE job system.

**Mi call**: híbrido. Implementar subset rigid-body propio (broadphase + GJK/EPA/SAT + SI) pedagogicamente. Cuando toque cloth/soft — **NO reimplementar, bolt on Jolt 5.x como dep solo para soft-body module** (Jolt soft es XPBD-based, exactamente lo que ALZE querría de todos modos).

### Apuesta E: "Differentiable physics como feature"

- **Acierta si**: ALZE es herramienta I+D para estudio que hace motion AI + tooling. NVIDIA Warp o Brax como offline pipeline.
- **Falla si**: ALZE es game engine. Differentiable introduce tape memory, dual forward/adjoint, autograd complexity. Escalando linealmente, ALZE duplica LOC.

**Mi call**: rabbit hole. No core. Hook API para que usuario pueda implementar su wrapper Warp-style si necesita.

### Apuesta F: "Determinism desde v1"

- **Acierta siempre**. Costo: 1 semana diseño. Beneficio: multi-year optionality para lockstep, replay, testing.
- **Falla solo**: si la feature flag nunca se testea y bit-rots. Solución: 1 unit test CI que verifica bit-identical resultado de 1000-step sim between two runs.

**Mi call**: YES. `ALZE_DETERMINISTIC` flag + 1 CI test.

### Apuesta G: "Neural physics surrogates como feature"

- **Acierta si**: juego tiene cinematics cloth masivas offline-quality-in-runtime. SNUG o similares.
- **Falla si**: training cost, retargeting, inference latency. Games 2024-2026 shipping con neural physics = 0.

**Mi call**: No antes 2030.

### Apuesta H: "MPM / FEM tetrahedral v1"

- **Acierta si**: juego sim-genre snow/granular central. Disney-style pipeline + 1 PhD.
- **Falla si**: cualquier otra cosa. Scope creep asesino.

**Mi call**: No. Ever. Hasta v3 y justificación extrema.

---

## 15. Roadmap final ALZE 2026-2032

**2026 v1 (+9 meses)**
- Sequential-Impulse solver (Catto 2006). Dbvt broadphase (Bullet-style) o 4-ary lock-free tree (Jolt-style).
- GJK + EPA narrowphase + SAT fast paths box/capsule.
- Warm-started persistent contacts, islands + graph coloring para parallel.
- `ALZE_DETERMINISTIC` flag desde día 1.
- XPBD Small-Steps solver para cloth + ropes + blobs.
- Joints: fixed, distance, hinge, slider. No Featherstone.

**2027 v1.5**
- TGS Soft migration para rigid (replicar Box2D v3 en 3D).
- Heightfield + triangle-mesh-BVH shapes.
- Shape-matching soft bodies.

**2028 v2**
- RHI upgrade a Vulkan. Compute shaders compute-portable.
- Optional GPU particles (XPBD/PBF) via compute.
- Evaluar VBD vs XPBD en branch paralelo.
- CCD conservativa-advance.

**2029-2030 v2.5**
- Dear-ImGui physics debug viewer.
- Optional Featherstone para articulation (si gameplay lo pide).
- Pre-fracture Voronoi tooling offline + runtime constraint graph.
- Snapshot/replay format (for netcode y testing).

**2030-2032 v3**
- VBD soft body replacement (si papers consolidan).
- AVBD para stiff rigid stacking (si benchmark lo demanda).
- Evaluate Jolt GPU como dep para specific subsystems.
- Optional Warp-style DSL experimental branch para R&D.

---

## Gaps de esta research

Cosas que NO pude confirmar y merecen round 2 si alguien las necesita:

1. **PhysX 6 roadmap**: NVIDIA no publica fechas. Asumí PhysX 5.x continúa hasta 2028, PhysX 6 con Warp-integration en 2028-2030. No confirmed.
2. **Jolt GPU timeline**: Jorrit Rouwe mencionó interés en GitHub Discussion #1263 pero no dio ETA. Asumí 2027-2028.
3. **Games shipped con MPM 2024-2025**: no encontré ninguno documentado. Pude estar perdiendo un lanzamiento Disney-relationship o plugin UE/Unity nicho.
4. **AVBD production-ready code**: paper 2025 + SIGGRAPH Real-Time Live! demo, pero no public reference implementation con tests/benchmarks que vi. Riesgo de "paper-only".
5. **Newton 1.0 traction en games (no robotics)**: 0 ejemplos vi 2025. Proyección 2028-2030 es speculation informada.
6. **Havok GPU path**: Microsoft podría anunciar Havok-GPU-Xbox-native. No confirmed.
7. **TressFX shipped games 2024+**: HairWorks legacy, Groom UE5 dominante, pero no encontré AAA 2024+ usando TressFX explícitamente.
8. **Havok cross-platform determinism technical specifics**: documentado que ship, pero no hay paper público. Microsoft proprietary.

---

## Referencias consolidadas (≥30 fuentes primarias)

### Papers académicos (SIGGRAPH / SCA / CVPR / ICLR)

1. Macklin, Müller, Chentanez, Kim. *Unified Particle Physics for Real-Time Applications*. ACM TOG 33(4), SIGGRAPH 2014. **Test-of-Time 2025**. <https://mmacklin.com/uppfrta_preprint.pdf>
2. Macklin, Müller, Chentanez. *XPBD: Position-Based Simulation of Compliant Constrained Dynamics*. MIG 2016. <https://mmacklin.com/xpbd.pdf>
3. Macklin et al. *Small Steps in Physics Simulation*. SCA 2019. <https://mmacklin.com/smallsteps.pdf>
4. Müller et al. *Detailed Rigid Body Simulation with Extended Position Based Dynamics*. 2020. <https://matthias-research.github.io/pages/publications/PBDBodies.pdf>
5. Macklin, Müller. *A Constraint-based Formulation of Stable Neo-Hookean Materials*. 2021.
6. Li, Ferguson, Schneider, Langlois, Zorin, Panozzo, Jiang, Kaufman. *Incremental Potential Contact: Intersection- and Inversion-free Large-Deformation Dynamics*. ACM TOG 39(4), SIGGRAPH 2020. <https://ipc-sim.github.io/file/IPC-paper-350ppi.pdf>
7. Lan, Kaufman, Li, Jiang, Yang. *Affine Body Dynamics: Fast, Stable & Intersection-free Simulation of Stiff Materials*. ACM TOG 41(4), SIGGRAPH 2022. <https://arxiv.org/abs/2201.10022>
8. Chen, Macklin, Müller, Kim, Jiang. *Vertex Block Descent*. ACM TOG 43(4), SIGGRAPH 2024. <https://graphics.cs.utah.edu/research/projects/vbd/vbd-siggraph2024.pdf>
9. Giles, Diaz, Yuksel. *Augmented Vertex Block Descent*. ACM TOG 44(4), SIGGRAPH 2025. <https://graphics.cs.utah.edu/research/projects/avbd/Augmented_VBD-SIGGRAPH25.pdf>
10. Bouaziz, Martin, Liu, Kavan, Pauly. *Projective Dynamics: Fusing Constraint Projections for Fast Simulation*. ACM TOG 33(4), SIGGRAPH 2014. <https://users.cs.utah.edu/~ladislav/bouaziz14projective/bouaziz14projective.pdf>
11. Macklin, Müller. *Position Based Fluids*. ACM TOG 32(4), SIGGRAPH 2013. <https://mmacklin.com/pbf_sig_preprint.pdf>
12. Bender, Koschier. *Divergence-Free Smoothed Particle Hydrodynamics*. SCA 2015; TVCG 2017 extended. <https://animation.rwth-aachen.de/media/papers/2015-SCA-DFSPH.pdf>
13. Jiang, Schroeder, Selle, Teran, Stomakhin. *The Affine Particle-In-Cell Method*. ACM TOG 34(4), SIGGRAPH 2015. <https://dl.acm.org/doi/10.1145/2766996>
14. Stomakhin et al. *A Material Point Method for Snow Simulation*. ACM TOG 32(4), SIGGRAPH 2013 (Disney Frozen).
15. Holden, Komura, Saito. *Phase-Functioned Neural Networks for Character Control*. ACM TOG 36(4), SIGGRAPH 2017.
16. Peng, Abbeel, Levine, van de Panne. *DeepMimic*. ACM TOG 37(4), SIGGRAPH 2018. <https://xbpeng.github.io/projects/DeepMimic/>
17. Peng et al. *AMP: Adversarial Motion Priors for Stylized Physics-Based Character Control*. ACM TOG 40(4), SIGGRAPH 2021.
18. Peng et al. *ASE: Large-Scale Reusable Adversarial Skill Embeddings*. ACM TOG 41(4), SIGGRAPH 2022. <https://arxiv.org/abs/2205.01906>
19. Peng et al. *MaskedMimic*. ACM TOG, SIGGRAPH Asia 2024. <https://dl.acm.org/doi/10.1145/3687951>
20. Büttner, Holden, et al. *Learned Motion Matching*. ACM TOG, SIGGRAPH 2020. <https://dl.acm.org/doi/10.1145/3386569.3392440>
21. Santesteban, Otaduy, Casas. *SNUG: Self-Supervised Neural Dynamic Garments*. CVPR 2022 Oral. <https://arxiv.org/abs/2204.02219>
22. Xie et al. *PhysGaussian: Physics-Integrated 3D Gaussians for Generative Dynamics*. CVPR 2024 Highlight. <https://xpandora.github.io/PhysGaussian/>
23. Sanchez-Gonzalez et al. *Learning to Simulate Complex Physics with Graph Networks*. ICML 2020. <https://arxiv.org/abs/2002.09405>
24. Pfaff et al. *Learning Mesh-Based Simulation with Graph Networks*. ICLR 2021. <https://openreview.net/forum?id=roNqYL0_XP>
25. Freeman, Frey, Raichuk, Bachem et al. *Brax — A Differentiable Physics Engine for Large Scale Rigid Body Simulation*. NeurIPS 2021 Datasets. <https://arxiv.org/abs/2106.13281>
26. Howell, Le Cleac'h, Brüdigam, Kolter, Schwager, Manchester. *Dojo: A Differentiable Simulator for Robotics*. 2022+. <https://arxiv.org/abs/2203.00806>
27. Heiden, Macklin et al. *DiSECt: A Differentiable Simulation Engine for Autonomous Robotic Cutting*. RSS 2021; Autonomous Robots 2023. <https://diff-cutting-sim.github.io/>
28. Todorov, Erez, Tassa. *MuJoCo: A Physics Engine for Model-Based Control*. IROS 2012.
29. Todorov. *Convex and Analytically-Invertible Dynamics with Contacts and Constraints*. ICRA 2014.
30. Valevski et al. *Diffusion Models Are Real-Time Game Engines (GameNGen)*. arXiv 2408.14837, ICLR 2025 poster. <https://arxiv.org/abs/2408.14837>
31. MuJoCo Playground tech report. arXiv 2502.08844, 2025. <https://arxiv.org/html/2502.08844v1>
32. Catto. *Fast and Simple Physics using Sequential Impulses*. GDC 2006. <https://box2d.org/files/ErinCatto_SequentialImpulses_GDC2006.pdf>
33. Catto. *Modeling and Solving Constraints*. GDC 2009. <https://box2d.org/files/ErinCatto_ModelingAndSolvingConstraints_GDC2009.pdf>
34. Catto. *Continuous Collision*. GDC 2013. <https://box2d.org/files/ErinCatto_ContinuousCollision_GDC2013.pdf>
35. Bender, Müller, Macklin. *Position-Based Simulation Methods in Computer Graphics*. Eurographics 2017 Tutorial. <https://mmacklin.com/EG2015PBD.pdf>

### Industria / blogs / docs de vendor

36. Rouwe. *Architecting Jolt Physics for Horizon Forbidden West*. GDC 2022. <https://jrouwe.nl/architectingjolt/>
37. Jolt Physics repo + docs. <https://github.com/jrouwe/JoltPhysics> · <https://jrouwe.github.io/JoltPhysics/>
38. Catto. *Releasing Box2D 3.0*. Aug 2024. <https://box2d.org/posts/2024/08/releasing-box2d-3.0/>
39. Catto. *SIMD Matters*. Aug 2024. <https://box2d.org/posts/2024/08/simd-matters/>
40. Catto. *Solver2D*. Feb 2024. <https://box2d.org/posts/2024/02/solver2d/>
41. NVIDIA PhysX 5 docs. <https://nvidia-omniverse.github.io/PhysX/physx/5.4.1/>
42. NVIDIA. *Open Source Simulation Expands with NVIDIA PhysX 5 Release*. 2022 y 2025. <https://developer.nvidia.com/blog/open-source-simulation-expands-with-nvidia-physx-5-release/>
43. *NVIDIA Makes PhysX & Flow GPU Code Open-Source*. Phoronix, 2025. <https://www.phoronix.com/news/NVIDIA-OSS-PhysX-Flow-GPU>
44. NVIDIA Warp docs + publications. <https://github.com/NVIDIA/warp> · <https://github.com/NVIDIA/warp/blob/main/PUBLICATIONS.md>
45. NVIDIA. *Creating Differentiable Graphics and Physics Simulation in Python with NVIDIA Warp*. Technical blog. <https://developer.nvidia.com/blog/creating-differentiable-graphics-and-physics-simulation-in-python-with-nvidia-warp/>
46. NVIDIA. *Announcing Newton, an Open-Source Physics Engine for Robotics Simulation*. 2025. <https://developer.nvidia.com/blog/announcing-newton-an-open-source-physics-engine-for-robotics-simulation/>
47. Linux Foundation. *Newton contribution*. 2025. <https://www.linuxfoundation.org/press/linux-foundation-announces-contribution-of-newton-by-disney-research-google-deepmind-and-nvidia-to-accelerate-open-robot-learning>
48. MuJoCo MJX docs. <https://mujoco.readthedocs.io/en/stable/mjx.html>
49. Rapier docs. <https://rapier.rs/> · Rapier determinism guide. <https://rapier.rs/docs/user_guides/rust/determinism/>
50. Dimforge blog. *Announcing the Rapier physics engine*. 2020. <https://dimforge.com/blog/2020/08/25/announcing-the-rapier-physics-engine/>
51. Havok 2025.1 announcement. <https://www.havok.com/blog/>
52. *The Havok Physics engine is making a pitch to developers in 2025*. Windows Central, 2025. <https://www.windowscentral.com/gaming/the-havok-physics-engine-is-making-a-pitch-to-developers-in-2025>
53. Epic. *Dynamic Destruction in UE5 with the Chaos Destruction System*. GDC 2025. <https://gdcvault.com/play/1035357/Dynamic-Destruction-in-UE5-with>
54. UE5 Chaos Destruction docs. <https://dev.epicgames.com/documentation/en-us/unreal-engine/chaos-destruction-in-unreal-engine>
55. NVIDIA Blast GitHub + docs. <https://github.com/NVIDIAGameWorks/Blast>
56. Bullet Physics repo. <https://github.com/bulletphysics/bullet3>
57. Roblox. *Vertex Block Descent publication*. 2024. <https://about.roblox.com/publications/vertex-block-descent>
58. PositionBasedDynamics lib (Bender). <https://github.com/InteractiveComputerGraphics/PositionBasedDynamics>
59. Gaffer On Games. *Deterministic Lockstep*. <https://gafferongames.com/post/deterministic_lockstep/>
60. Cannon. *GGPO*. <https://www.ggpo.net/>
61. Simulately Wiki. *Differentiable Simulators comparison*. <https://simulately.wiki/docs/domain/differentiable/>

---

**Fin.** Este documento debería informar decisiones de roadmap ALZE hasta 2032. Re-evaluar cada 6 meses — el campo se mueve rápido (especialmente diferenciable + VBD + Newton).
