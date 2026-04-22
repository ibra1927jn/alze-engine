# Síntesis — motores gráficos + físicas para ALZE Engine

**Fecha:** 2026-04-22
**Input:** 7 agentes paralelos, 1242 líneas totales. 1 fallo menor en `_errors.md` (UE5: 2 URLs 403 + 2 PDFs SIGGRAPH >10MB — cubiertos por mirrors).

Target: `/root/repos/alze-engine` — C++17, no-RTTI, no-exceptions, SDL2+OpenGL 3.3, ~25-30K LOC, Fase 7 (PBR+ECS+Physics+Audio).

- [`ue5.md`](ue5.md) — Unreal Engine 5 (173 L, 28 URLs)
- [`unity_godot.md`](unity_godot.md) — Unity + Godot 4 (155 L, 20 URLs)
- [`ecs_engines.md`](ecs_engines.md) — Bevy + Flecs + EnTT (177 L, 32 URLs)
- [`aaa_engines.md`](aaa_engines.md) — id Tech + Source 2 + Frostbite + CryEngine + Decima + Northlight + Creation (178 L, papers con autor+año+venue)
- [`rendering_libs.md`](rendering_libs.md) — bgfx + Filament + Falcor + Magnum + Diligent + wgpu + Sokol + The Forge (180 L, 27 URLs)
- [`physics_3d_industry.md`](physics_3d_industry.md) — PhysX + Havok + Bullet + Jolt (191 L, 20+ primary refs)
- [`physics_specialized.md`](physics_specialized.md) — Rapier + Box2D v3 + MuJoCo + XPBD + FleX + PBD lib (188 L)

---

## Tabla comparativa (categoría × best-in-class)

| Capa | Ganador para ALZE | Por qué | Alternativa |
|---|---|---|---|
| **RHI (abstracción API)** | bgfx pattern (no linkear bgfx en sí) | opaque handles, sort-key frontend, OpenGL 3.3 hoy + Vulkan mañana sin tocar game code | Sokol si prefieres header-only C89 |
| **PBR shading** | Filament math (portar, no linkear) | GGX + split-sum IBL es canonical. Filament usa RTTI/exceptions → no enlazable | portar de Khronos PBR Neutral samples |
| **Shader toolchain** | HLSL → dxc → SPIRV → SPIRV-Cross → GLSL/MSL | un único author source para N APIs | Slang si quieres más modern; WGSL no vale la pena |
| **ECS storage** | Archetype SoA (Bevy/Flecs/Mass) | cache locality en iteración de sistemas; mejor que sparse-set (EnTT) para hot loops | sparse-set OK para worlds pequeños o add/remove-heavy |
| **ECS scheduler** | Automatic parallelism via ComponentAccess<R,W> (Bevy pattern) | detecta conflictos en tiempo de registro, paraleliza sin anotación manual | manual por fases si prefieres determinismo |
| **Scene graph** | PackedScene composable (Godot) + (relation, target) pairs (Flecs) | escenas son recursos reusables; relaciones son first-class no pointers | — |
| **Render graph** | Sort-key (bgfx) v1 → Frame Graph DAG (Frostbite/O'Donnell GDC 2017) v2 | DAG es overkill hasta >20 passes; sort-key cubre todo hasta ahí | UE5 Render Graph si necesitas auto-aliasing transient |
| **Physics rigid** | Sequential-Impulse (Catto) + Dbvt broadphase (Bullet) + GJK+EPA narrowphase + SAT fast path | Catto-style shipped en Box2D + Rapier; Dbvt es legible y rápido | pattern after Jolt architecturally (lock-free broadphase) |
| **Physics soft/cloth** | XPBD small-steps (Macklin SCA 2019) | consensus moderno para cloth/rope/soft; stable con timesteps grandes; single paradigm | PBD clásico si XPBD te parece squishy |
| **Parallelism** | Islands + graph coloring (Jolt + Rapier + Box2D v3) | grupos de bodies interactuantes se solven en paralelo sin locks | — |
| **Textures** | KTX2 + Basis Universal | compressed cross-API (BC7/ASTC/ETC2 transcode at load) | — |
| **Asset pipeline** | Content-hash DDC (UE5 pattern) | baked derived data reusable cross-machine + cross-platform | — |
| **Editor** | Dear ImGui + native windowing | NO custom UI framework | Blender-style para DCC si lo necesitas |
| **Determinismo opcional** | Rapier-style `enhanced-determinism` flag | bit-level cross-platform IEEE 754 si algún día haces lockstep multiplayer | — |

---

## Stack recomendado (pragma-first, con rutas upgrade)

### v1 (hoy mismo, compatible con OpenGL 3.3)
- **RHI**: envoltorio propio tipo bgfx con `Handle<T>` opacos (64-bit gen+index). Backend único GL3.3.
- **Shaders**: escribir en HLSL, compilar con dxc → SPIR-V → SPIRV-Cross → GLSL 330. Un script, un output per API.
- **ECS**: archetype SoA. Un `World` con `Archetype` por combinación única de component types. Manual `BlobVec` (size+align+count+ptr, sin templates cargados) para columns. ComponentId = u32 tipo-erased.
- **Systems**: lista de `System` con metadata `ComponentAccess<Read<T>...> + Write<U>...>`. En build time o registry, grafo de conflictos → paraleliza lo que no conflictúa.
- **Render**: deferred base pass + GGX forward for transparents, single pipeline. Sort-key 64-bit: `[layer:4][viewId:4][blend:2][shaderId:18][material:18][depth:18]`. Queue en frame, sort, emit drawcalls.
- **PBR**: Filament GGX + split-sum IBL portado a tu shader lib. IBL prefilter baker offline.
- **Physics**: Sequential-Impulse solver (leer Catto GDC 2006 "Fast and Simple Physics using Sequential Impulses"). Dbvt broadphase. GJK+EPA (Ericson RTCD cap. 9) + SAT para box-box. Islands + graph-coloring parallelism.
- **Cloth**: XPBD distance + bending + collision constraints. Small Steps (Macklin 2019) en lugar de iterations clásico.
- **Memory**: linear arenas per-frame + pool allocators per-type. No `new`/`delete` en hot paths.
- **Threading**: job system con dependency graph (patrón Frostbite). Main thread = game logic + command recording, worker threads = culling + physics step + audio + asset IO.

### v2 (cuando ALZE necesite más APIs o más performance)
- RHI backend **Vulkan** (manteniendo la API opaca). Validar con D3D12 y Metal si alcance permite.
- Render graph **DAG** (Frame Graph O'Donnell): passes declaran reads/writes de virtual resources, runtime asigna physical + inserta barriers.
- **Bindless materials** (Frostbite): descriptor table único, per-draw material index.
- **GPU-driven rendering** (Decima/UE5): cluster culling + `vkCmdDrawIndirect` + meshlet amplification si hw lo soporta.
- **Visibility buffer rendering** (Nanite-lite): triangle ID en G-buffer, material pass resolve en compute.

### v3 (si alcance crece a AAA)
- Async compute para post-FX + particle sim + compute AO
- Virtual textures (Decima-style megatexture reincarnated) — MUY delicado, dejar para v3
- Soft/hard RT unified (Lumen pattern) si hw baseline lo permite
- PSO precache pipeline para eliminar shader compilation stutter

---

## Temas cross-engine (patrones que se repiten)

Estas ideas aparecieron en **múltiples agentes independientes** — indicador de consenso de la industria:

### 1. Data-oriented / SoA es el default moderno
- Mass Entity (UE5): archetype SoA
- Bevy/Flecs: archetype SoA
- Box2D v3 (Aug 2024 rewrite): SoA completo con AVX2 lanes
- Jolt: SoA interno
- DOTS/Burst (Unity): SIMD-by-default jobs

**Lección**: si diseñas hoy un sistema hot-loop (ECS, physics solver, particle sim), **SoA primero**. AoS queda para cosas frías (config, metadata, UI state).

### 2. Job system + islands = paralelismo sin pain
- Jolt: islands + job system
- Rapier: islands + rayon
- Box2D v3: graph coloring + SIMD
- Frostbite (Andersson GDC 2009): task system multi-core
- DOTS: Jobs + dependency analysis

**Lección**: no escales threads manualmente. Define "islands" (grupos independientes) y deja que el scheduler paralelice.

### 3. XPBD está ganando para cloth/soft
- Matthias Müller/Macklin 2016, Small Steps 2019
- Blender adopted
- Position Based Dynamics library (canonical)
- PhysX 5 GPU cloth usa ideas XPBD
- Unified Particle Physics (FleX 2014 → SIGGRAPH Test-of-Time 2025)

**Lección**: si haces cloth/rope/deformables en ALZE, XPBD es el default hoy. No mass-spring, no FEM tetrahedra como v1.

### 4. Shader language → cross-compile es consensus
- HLSL authoring + dxc + SPIRV-Cross = stack de facto
- bgfx, Filament, Falcor, The Forge: todos emiten desde una source
- WGSL se normalizará en web pero nativo sigue HLSL

**Lección**: no pelees con GLSL directo para multi-API. Una source, N outputs.

### 5. Frame Graph / render graph para coordinar passes
- Frostbite FrameGraph (O'Donnell GDC 2017) — la referencia
- UE5 Render Dependency Graph (RDG)
- Falcor research renderer
- bgfx sort-key como versión lightweight

**Lección**: empezar con sort-key, upgrade a DAG cuando passes > 20 justifique la complejidad.

### 6. Bindless es el fin del descriptor ceremony
- Frostbite (2017+), UE5 modern, Nanite, DOTS hybrid renderer
- Acceso a material/texture via índice en un array global

**Lección**: Vulkan 1.2+ bindless descriptor indexing es el futuro. Diseña APIs pensando en eso aunque GL3.3 no lo soporte.

---

## Anti-patterns (13 trampas vistas en múltiples engines)

No repetir:

1. **Tres render pipelines paralelos** (Unity HDRP/URP/Built-in) — decidir uno y mantener
2. **GameObject.Find scatter + reflection** (Unity legacy) — todo lookup por handle
3. **GDScript/Blueprint first-class** — ALZE es C++, scripting es un plugin opcional (Lua/Wren/Angelscript), no corazón
4. **Custom UI framework** — usar Dear ImGui + SDL, no inventar
5. **Custom archive format** — glTF + KTX2 + own manifest.json, no inventar .pak
6. **Custom physics desde 0 complet** — OK implementar Sequential-Impulse como ejercicio, pero mantener opción de linkear Jolt/Bullet si ALZE crece
7. **Shader permutation explosion** (UE's stutter fame) — PSO precache from day 1
8. **EnTT sparse-set para worlds iteración-heavy** — archetype SoA gana (ver punto 1 arriba)
9. **IL2CPP-style transpilation** — complejidad no justificada en nuestro scope
10. **Rust borrow model portado a C++** — imposible sin lenguaje cooperativo. Usar convenciones + static analyzer + RAII
11. **Full FEM tetrahedra soft body** v1 — demasiado caro, XPBD cloth basta
12. **CUDA-only physics (FleX)** — vendor-lock, no portable
13. **Frostbite trauma cross-genre** — un engine optimizado para un género no sirve para otro sin rewrite doloroso. Definir el scope de ALZE (FPS? ARPG? sim?) antes de feature-creep
14. **Tooling rewrites cada 5 años** — pragmatic, adopta lo existente (Blender DCC, Tiled tilemap, Dear ImGui editor)
15. **Virtual textures en v1** — extremadamente complejo, Decima tardó años. Dejar para v3

---

## Librerías a considerar (no-NIH)

Adoptar en lugar de reinventar:

| Función | Librería | Licencia | Por qué |
|---|---|---|---|
| UI (editor + debug HUD) | Dear ImGui | MIT | standard de facto, no tiene rival |
| Asset loading (glTF) | cgltf | MIT | ya usado por ALZE |
| Image loading | stb_image | public domain | ya usado por ALZE |
| Windowing + input | SDL3 | Zlib | SDL2 ya usado, upgrade cuando esté estable |
| Audio | miniaudio | MIT/public | single-header, robusto |
| Mesh lib | meshoptimizer | MIT | index optim, LOD, cluster gen |
| Texture compression | Basis Universal | Apache 2 | KTX2 transcoder |
| Physics (si NO hacerlo propio) | Jolt | MIT | modern, shipped AAA, C++ limpio |
| Shader cross-compile | dxc + SPIRV-Cross | LLVM/MIT | standard toolchain |
| Math (SIMD) | glm o xsimd | MIT/BSD-3 | ALZE ya usa vectors propios con SSE2, decisión |
| Font/text | FreeType + HarfBuzz | FTL/MIT | render texto decente |
| Reflection (sin RTTI) | rttr o refl-cpp | MIT/ISC | para editor introspection |

---

## Ranking de densidad de ideas

De más a menos útil para ALZE según copy-ability real:

1. **ecs_engines.md** — ECS es core, y el archivo da blueprint C++17 concreto (archetype + BlobVec + scheduler). Aplicable YA.
2. **rendering_libs.md** — pick de RHI abstraction + recomendación "bgfx pattern + Filament port + HLSL toolchain" es directamente actionable. Aplicable YA.
3. **physics_specialized.md** + **physics_3d_industry.md** — juntas dan la receta completa (Sequential-Impulse + Dbvt + GJK/EPA/SAT + XPBD + islands). Aplicable en 8-12 semanas.
4. **aaa_engines.md** — Frame Graph + bindless + GPU-driven son v2/v3 pero el file los explica bien. Aplicable después de RHI v1.
5. **ue5.md** — Nanite + Lumen son aspiracionales para ALZE scope. Las ideas como visibility buffer + PSO precache + content-hash DDC sí aplican. Aplicable en v2.
6. **unity_godot.md** — Godot's Servers pattern + PackedScene son útiles; DOTS Burst es aspirational. Aplicable parcialmente YA.

---

## Qué sigue si ALZE entra en design real

1. **Decisión #1 (DEBE tomarse)**: scope del engine. ¿AAA aspiracional o sim-genre focado? Frostbite nos enseñó qué pasa cuando un engine ambicioso se fuerza a géneros distintos. Escribe un doc de 1 página.
2. **Decisión #2**: ECS storage (archetype vs sparse-set). Este commit afecta 10 años.
3. **Prototype v0**: RHI abstraction (bgfx pattern) con GL3.3 backend + HLSL→GLSL pipeline + 1 cubo PBR shading con IBL. 2 semanas.
4. **Prototype v1 ECS**: archetype storage + 3 systems + scheduler con ComponentAccess. 2 semanas.
5. **Prototype v1 physics**: Sequential-Impulse + Dbvt + 100 boxes cayendo. 4 semanas.
6. **Fase de integración**: ECS + Render + Physics comunican. Frame loop consolidado. 2 semanas.

**Total boceto**: ~10-12 semanas para un vertical slice defendible con las mejores ideas de esta research aplicadas. El prototype pondrá a prueba las decisiones de arquitectura antes de commit a scope completo.
