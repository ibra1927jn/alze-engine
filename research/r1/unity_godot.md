# Unity vs Godot — Comparative Notes for ALZE Engine

Investigación 2026-04-21. Contexto: ALZE Engine es un motor C++17 (sin RTTI, sin excepciones), SDL2 + OpenGL 3.3, ~25–30 KLOC, actualmente en Fase 7 (PBR + ECS + Física + Audio). Objetivo: extraer ideas concretas implementables desde los dos motores de propósito general más usados fuera del ámbito UE.

## Overview

**Unity** nació en Copenhague en agosto de 2004 como *Over the Edge I/S* (DBA *Over the Edge Entertainment*), fundada por David Helgason, Nicholas Francis y Joachim Ante. El primer juego propio, *GooBall* (2005), fracasó comercialmente, pero el engine subyacente se presentó en Apple WWDC 2005 y la empresa pivotó para vender herramientas. Licencia propietaria; el episodio de 2023 (Runtime Fee per-install) dañó la confianza de la comunidad; Unity canceló el esquema en 2024 tras la salida del CEO John Riccitiello. Target: estudios medianos, mobile, XR, sectores no-juego (arquitectura, automoción, simulación).

**Godot** nació en 2014 como proyecto open-source de Juan "reduz" Linietsky y Ariel Manzur (OKAM Studio, Argentina); basado en un engine interno que Linietsky llevaba iterando desde ~2001. Liberado bajo MIT, gobernado por la Godot Foundation. Target: indies, 2D, proyectos donde el costo/licencia importa, hobbyistas, cada vez más estudios pequeños haciendo 3D. Godot 4 (2023) es el rewrite sobre Vulkan.

## Unity architecture

- **GameObject/Component** es el modelo legacy: todo en la escena es un `GameObject` con una lista de `Component`s (`Transform`, `MeshRenderer`, scripts `MonoBehaviour`). Flexible pero cache-unfriendly — cada GameObject es una alloc en heap, componentes dispersos, `GameObject.Find`, `GetComponent<T>()` y mensajes tipo `Update()` son reflective y fragmentados en memoria. El hot path de un frame típico toca punteros de estructuras *managed* por el GC.
- **DOTS** (Data-Oriented Technology Stack) es la respuesta moderna, compuesta por tres piezas ortogonales pero que brillan juntas:
  - **Entities / ECS**: entidades sólo-ID (`Entity = {index, version}`), componentes `struct` POD (`IComponentData`), sistemas (`SystemBase`, `ISystem`) iteran *chunks* contiguos de 16 KB agrupados por *archetype*. SoA-friendly, cache-coherent por diseño. `EntityManager` maneja alloc/dealloc.
  - **C# Job System**: API de jobs (`IJob`, `IJobParallelFor`, `IJobChunk`) con dependencias explícitas vía `JobHandle`, chequeo estático de race conditions vía `[ReadOnly]` / `[WriteOnly]` y *safety handles* en colecciones nativas (`NativeArray<T>`, `NativeList<T>`, `NativeHashMap<K,V>`) que detectan uso concurrente ilegal en editor.
  - **Burst compiler**: subset de C# (no GC allocs, no virtual calls, no exceptions) traducido a LLVM IR → código nativo con auto-vectorización SIMD (SSE/AVX2/AVX-512 en x86, NEON en ARM). `Unity.Mathematics` expone `float4`/`float3`/`int4` que mapean 1:1 a registros SIMD. Mejoras típicas 10–100× en compute hotspots; `[BurstCompile]` sobre una función basta.
- **Render pipelines**: tres caminos incompatibles:
  - **Built-in (BRP)**: legacy, C++ hardcoded, difícil de modificar.
  - **URP** (Universal RP): SRP forward/forward+/deferred, target mobile-to-console, shaders escritos en Shader Graph o HLSL.
  - **HDRP** (High Definition RP): deferred por defecto, PBR avanzado, volumetrics, ray tracing, SSR, target solo desktop/consola.
  - **SRP** (Scriptable Render Pipeline): el framework en C# que permite escribir pipelines custom. URP/HDRP son implementaciones que Unity mantiene.
- **Scripting runtime**:
  - **Mono** (JIT, fork del Mono open-source) para dev/editor.
  - **IL2CPP** (AOT): Roslyn compila C# → IL → IL2CPP transpila a C++ → compilador nativo del target. Mandatorio en iOS/consolas, opcional en desktop. Startup más rápido, code más opaco, build times largos.
- **Assembly Definitions** (`.asmdef`): permiten partir el código en DLLs aisladas con dependencias explícitas, acelerando compilación incremental y forzando arquitectura por capas.
- **Asset pipeline**: cada asset genera un `.meta` sidecar con un GUID estable + import settings serializados. Las referencias entre assets se guardan por GUID, no por path — mover/renombrar no rompe refs mientras el `.meta` viaje con el asset.
- **AssetBundle vs Addressables**: AssetBundles es la API low-level (bundle name + asset name). **Addressables** está construida encima: referencia por string-address, dependency graph automático, remote hosting, memory tracking, content update flow. En 2026 Addressables es lo recomendado; AssetBundle raw queda para casos específicos.

## Godot architecture

- **Node + SceneTree**: "everything is a Node". Cada objeto (`Sprite2D`, `RigidBody3D`, `AudioStreamPlayer`, `Label`) hereda de `Node`. Un SceneTree tiene una raíz única y todos los demás son descendientes. Un único parent por nodo, N children.
- **Scene-as-resource**: una escena guardada (`.tscn`) es un `PackedScene`. `instantiate()` produce un árbol clonado. La escena funciona simultáneamente como template, prefab, y unit de composición — una Player scene se *embebe* como nodo dentro de una Level scene. Composición > herencia para reuso.
- **Servers architecture** (clave del diseño de Linietsky, ver post blog oficial "Why does Godot use Servers and RIDs?"):
  - `RenderingServer`, `PhysicsServer2D/3D`, `AudioServer`, `NavigationServer` son APIs command-oriented que corren en threads dedicados.
  - Los objetos en el lado del server se referencian vía **RIDs** (Resource IDs opacos), no por puntero directo.
  - El main thread encola comandos; el server los drena en su propio thread. Flujo *unidireccional*: Logic → Physics → Rendering, sin feedback loops complejos. Paralelismo sin job scheduler explícito; sin race conditions porque nunca hay shared state accesible directo.
  - Linietsky: "Logic sets information into Physics, but it does not need to retrieve data from it" — las asimetrías de dependencias justifican la arquitectura.
  - Desde Godot 3.0, los RIDs cachean un puntero opaco para evitar hash-lookups, a costa de menos debug info en release.
- **Renderers** (todos sobre Vulkan/D3D12/Metal vía `RenderingDevice`, salvo Compatibility que es GLES3):
  - **Forward+**: clustered forward, desktop, miles de luces dinámicas, todas las features.
  - **Mobile**: single-pass forward, límites de luces por mesh, menos features pero faster on tiled GPUs.
  - **Compatibility**: GLES3, para web y hardware viejo.
- **GDScript**: lenguaje dinámico tipo Python diseñado *para* Godot — sintaxis corta, integración total con el editor (autocompletar escena, node paths, señales). Interpretado; 10× más lento que C# en hot loops.
- **C# support**: oficial vía Mono/.NET. Marshalling entre C# y engine-core tiene overhead; no hay export web para C#.
- **GDExtension**: sucesor de GDNative. Shared library cargada en runtime; registra clases C++ (vía `godot-cpp` bindings) como si fueran nativas del engine. Permite hot-reload y no requiere recompilar el engine.
- **Resource system**: `Resource` es la clase base para assets (`Texture2D`, `Mesh`, `Material`, `PackedScene`). Ref-counted, serializable como `.tres` (text) o `.res` (binary). Resources se comparten entre escenas por referencia.
- **Build system**: SCons. Módulos custom (directorio con `SCsub` + `config.py` + `register_types.{h,cpp}`) se compilan como static libs y se linkean en el binario final. Flag `custom_modules=path1,path2` para rutas externas.

## Rendering feature gap

| Feature | HDRP | URP | Godot Forward+ | Godot Mobile |
|---|---|---|---|---|
| Clustered lighting | yes (deferred) | yes (Forward+) | yes | no (single-pass) |
| Volumetric fog/lighting | yes | limited | yes | limited |
| Real-time GI | SSGI, RTGI | none oficial | SDFGI, VoxelGI | no |
| Ray tracing | DXR | no | no (limited compute) | no |
| SSR | yes (quality) | yes (basic) | yes | no |
| TAA | yes | yes | yes | limited |
| Decals | yes | yes | yes | no |
| Target | high-end desktop | mobile → console | desktop | mobile + weak desktop |

Gap vs HDRP: Godot 4 aún no tiene ray tracing hardware accelerated y su volumetric/GI queda por debajo en calidad y performance — pero SDFGI es *suficiente* para la mayoría de casos sin necesidad de bake. Gap vs URP: más o menos empatan en features básicos; URP tiene ecosistema de Shader Graph + Render Graph más maduro. Donde se cruzan: PBR estándar, shadow mapping CSM, SSAO, bloom, tonemapping — ambos lo cubren bien.

## Scripting + tooling

**Godot**: GDScript está íntimamente integrado con el editor — autocompletar conoce los nodos hijos, node paths son first-class (`$Player/Camera`, `get_node("Enemy/Health")`), señales (event system tipado) se conectan desde la UI con una línea. El tree editor *es* el workflow: arrastrar-soltar nodos, guardar sub-árbol como escena reusable, `Ctrl+D` para duplicar. Lightweight: editor ~80 MB, arranca en segundos, corre en Raspberry Pi. Debugger integrado con inspección del SceneTree en vivo. C# via .NET 6+ para performance crítico, pero el idioma de la casa es GDScript.

**Unity**: C# + Visual Studio / JetBrains Rider integration; Unity debugger + Roslyn analyzers maduros. Herramientas visuales:
- **Shader Graph**: shaders nodales, genera HLSL; funciona con URP y HDRP (no con BRP).
- **Timeline**: cinemática + gameplay sequencing con tracks de animación, audio, señales custom.
- **VFX Graph**: partículas GPU compute, millones de partículas.
- **Animator**: state machines con transiciones, blend trees, avatar retargeting.
- **ProBuilder / Polybrush**: grey-boxing y mesh editing en el editor.

**Asset Store** es probablemente el feature más difícil de replicar: decenas de miles de assets, shaders, tools, scripts. Visual Scripting (antes Bolt, ahora integrado) existe pero no es idiomático; la mayoría usa C#.

## Threading / performance

- **Unity DOTS**: paralelismo explícito vía `IJob`, `IJobParallelFor`, `IJobEntity`. Tú decides qué paraleliza, el *safety system* valida race conditions en editor (chequea `[ReadOnly]` vs writes, alias de NativeArrays), Burst vectoriza automáticamente si usas `Unity.Mathematics`. Dependencias entre jobs vía `JobHandle.CombineDependencies`. SIMD-by-default en loops tight. Curva de aprendizaje alta: hay que *pensar data-oriented*, portar un GameObject típico a Entities no es trivial, muchas APIs de Unity (animator, UI, physics clásica) no funcionan desde un Job.
- **Godot Servers**: paralelismo implícito a nivel de subsistema. Rendering y physics corren en su propio thread automáticamente vía command queue; el usuario rara vez escribe código multi-hilo. Trade-off: menos control fino para ocultar latencia CPU/GPU, pero dramáticamente más simple. Si tu gameplay está main-thread-bound (AI pesada, pathfinding N²), los servers no ayudan.
- **Godot main-thread default para scripts**: GDScript y C# corren en main thread. `WorkerThreadPool.add_task()` existe desde 4.0 pero es opt-in y rudimentario comparado con Jobs + Burst. No hay un safety system estático.
- **Burst vs nada**: Unity + Burst puede generar código vectorizado mejor que un humano C++ razonable escribiendo a mano en muchos casos. Godot 4 no tiene equivalente; C++ via GDExtension es tu mejor bet para compute hot paths.

## En qué es bueno

**Unity**: ecosistema inigualable (Asset Store, plugins, tutoriales, stackoverflow), deployment targets (todas las consolas, XR headsets, web via WebGL/WebGPU), DOTS cuando lo necesitas, herramientas visuales (Shader Graph/Timeline/VFX Graph), soporte enterprise.

**Godot**: licencia MIT sin strings, editor ligero, scene-composition workflow es *el* feature killer, hackabilidad (engine 100% C++ leíble), GDScript baja la barrera para no-programadores, sin vendor lock-in, sin runtime fee drama posible.

## En qué falla

**Unity**:
- GameObject/Component legacy arrastra overhead y mental model antiguo; DOTS coexiste pero no reemplaza — dos mundos paralelos, boilerplate para puentear (Hybrid Components, Baking, Companion GameObjects).
- 2023 install-fee fiasco dañó reputación y confianza empresarial; muchos estudios ya migraron o evalúan alternativas (Godot, Unreal, in-house). La memoria colectiva es larga.
- Tres pipelines paralelos (BRP/URP/HDRP) fragmentan tutoriales, assets del store, shaders escritos. Elegir pipeline al inicio de proyecto es prácticamente irreversible — migrar URP→HDRP rompe materiales y post-process.
- Builds IL2CPP lentos (minutos a decenas de minutos). Editor pesado (GB-scale instala), lento en proyectos grandes; domain reload entre play-stops rompe el flujo.
- Fragmentación de versiones LTS + no-LTS + preview; Unity 6 cambia APIs nuevamente.

**Godot**:
- Rendering parity todavía alcanzando a HDRP: no hay ray tracing HW accelerated, VoxelGI/SDFGI son alternativas pero más costosas y menos precisas.
- Asset ecosystem pequeño comparado con Unity; el Godot Asset Library es ~3 órdenes de magnitud menor.
- GDScript limita performance en hot loops — tienes que bajar a C# o GDExtension para algoritmos pesados (procedural generation, AI, sim física custom).
- C# tiene overhead de marshalling engine↔script; no hay web export para builds C# (GDScript sí exporta a web).
- Animation tooling (retargeting, IK, blend trees) y terrain system menos maduros que Unity/Unreal.
- No hay soporte oficial de consolas — hay que pasar por terceros (W4 Games, Pineapple Works) con licencias comerciales aparte.
- Tooling para multijugador / networking todavía básico comparado con Mirror/Fishnet en Unity.

## Qué podríamos copiar para ALZE Engine

1. **Servers pattern (Godot) para desacoplar subsistemas del main thread**. `RenderCommandQueue`, `PhysicsCommandQueue` con ring buffers lock-free. El gameplay encola comandos; el thread del server los drena. Unidireccional Logic → Physics → Render. Encaja perfecto con C++17 sin excepciones: errores por código de retorno, RIDs son `uint32_t` + generation counter.
2. **Scene como resource + PackedScene** (Godot). Una "scene" ALZE es un árbol de entidades serializado (JSON o binary custom). `scene.instantiate()` clona el árbol dentro del ECS actual. Permite prefabs, composición, y save/load unificados. Sin herencia de escenas al principio; solo embedding.
3. **`.meta` sidecar con GUID estable + import settings** (Unity). Cada asset (`.png`, `.gltf`, `.wav`) tiene `asset.png.meta` con `{guid, importer_version, settings:{...}}`. Todas las refs inter-asset usan GUID, no path. Rename-safe, VCS-friendly, permite cache de assets importados en `.cache/<guid>/`.
4. **SIMD-friendly job API sobre un thread pool raw** (Burst inspiration). No necesitamos LLVM custom — C++17 + `std::thread` + `alignas(32)` + intrinsics (`__m128`, `__m256`). API: `JobHandle submit(JobDesc)`, dependencias explícitas, colecciones con `[ReadOnly]`-equivalente vía `const T*`. Mathematics library interna con `Vec4f` mapeando a SSE, igual que `Unity.Mathematics`.
5. **Módulos SCons-lite** (Godot). ALZE ya usa CMake — replicar: `modules/<name>/module.cmake` + `register_types.cpp` auto-descubierto vía glob. Flag `ALZE_MODULES="audio;physics;net"` para seleccionar. Permite builds custom (minimal server, headless, full editor) sin ifdef-hell.
6. **Node tree + SceneTree frame phases** (Godot). Aunque somos ECS, pueden coexistir: tick phases ordenadas (`pre_update` → `update` → `physics` → `post_update` → `render`). Entidades opt-in a cada fase. Más explícito que el `Update/LateUpdate/FixedUpdate` opaco de Unity.
7. **`Resource` base class ref-counted + text format** (Godot `.tres`). Todos los assets (mesh, material, texture, shader) heredan de `Resource` con ref-count atómico, serializable como texto editable a mano. Diff-friendly.
8. **Assembly-definitions-style module boundaries** (Unity `.asmdef`). Cada módulo ALZE declara explícitamente de qué otros depende; CI enforza DAG acíclico. Evita spaghetti include. Implementación mínima: cada `modules/<name>/module.json` con `{"depends":["core","math"], "public_headers":["include/"]}`; script Python en pre-build valida.
9. **Addressables-style string-addressed asset loading**. No queremos paths crudos en código gameplay. API: `AssetRef<Texture> tex = load("environment/rocks/boulder_01")`. El mapping address→GUID→path vive en un manifest serializado. Permite swap (mod support, hot-reload, A/B testing) sin tocar código.
10. **Signals / typed events** (Godot). Node expone `signal health_changed(old:int, new:int)`, otros nodos conectan. En C++: `Signal<int, int> health_changed;` con `connect()` type-checked. Desacopla sin recurrir a strings mágicos tipo `SendMessage`.
11. **Editor introspection ligero** (Godot property system). Cada componente ECS expone un mini-schema (nombre, tipo, rango, default). Sirve para editor, serialización y scripting uniformes. En C++ sin RTTI: macros `ALZE_PROPERTY(name, type, flags)` generan un `static constexpr Property[]` por clase — zero runtime cost, usable por el editor.
12. **Frame graph ligero** (inspiración SRP custom). Un pass describe sus reads/writes de render targets; el engine resuelve barriers y reuso de memoria. Empezar con dos passes (gbuffer, lighting) y crecer. No implementar auto-aliasing — empezar manual, medir, optimizar.

## Qué NO copiar

- **`GameObject.Find` + reflection scatter** (Unity legacy). Nada de lookup por string en runtime ni message dispatch oculto. Sistema + consulta explícita ECS.
- **GDScript como first-class** (Godot). Somos C++; no escribimos un lenguaje de scripting. Si queremos scripting, embeber Lua o WASM directamente.
- **Tres render pipelines paralelos** (Unity). Elegir uno — clustered forward+ — y mantenerlo. Branching a nivel de shader para quality tiers, no a nivel de pipeline.
- **Asset Store walled garden** (Unity). No construir una economía de plugins pagos. Que los módulos externos sean git submodules o CMake FetchContent.
- **DOTS coexistiendo con GameObject** (Unity). Un solo modelo — ECS — desde el principio. No arrastrar legacy.
- **IL2CPP-style transpilation intermedia**. Somos nativos desde el source. Complejidad no justificada.
- **Godot Servers con RID hash-lookup inicial** — saltarse la versión naive, arrancar directo con `uint32_t index + uint32_t generation` (slot map).

## Fuentes consultadas

- [Unity DOTS — Unity](https://unity.com/dots)
- [ECS for Unity](https://unity.com/ecs)
- [Burst User Guide — Unity Docs](https://docs.unity3d.com/Packages/com.unity.burst@1.6/manual/docs/OptimizationGuidelines.html)
- [IL2CPP Introduction — Unity Manual](https://docs.unity3d.com/6000.3/Documentation/Manual/il2cpp-introduction.html)
- [Render pipeline feature comparison — Unity Manual](https://docs.unity3d.com/6000.3/Documentation/Manual/render-pipelines-feature-comparison.html)
- [Asset metadata — Unity Manual](https://docs.unity3d.com/Manual/AssetMetadata.html)
- [Simplify content management with Addressables — Unity](https://unity.com/how-to/simplify-your-content-management-addressables)
- [Unity is Canceling the Runtime Fee — Unity Blog](https://unity.com/blog/unity-is-canceling-the-runtime-fee)
- [Unity U-turns on runtime fee — TechCrunch](https://techcrunch.com/2023/09/22/unity-u-turns-on-controversial-runtime-fee-and-begs-forgiveness/)
- [Unity Technologies — Wikipedia](https://en.wikipedia.org/wiki/Unity_Technologies)
- [Why does Godot use Servers and RIDs? — Godot Blog (Linietsky)](https://godotengine.org/article/why-does-godot-use-servers-and-rids/)
- [Internal rendering architecture — Godot Docs](https://docs.godotengine.org/en/stable/engine_details/architecture/internal_rendering_architecture.html)
- [RenderingServer — Godot Docs](https://docs.godotengine.org/en/stable/classes/class_renderingserver.html)
- [Overview of renderers — Godot Docs](https://docs.godotengine.org/en/stable/tutorials/rendering/renderers.html)
- [Optimization using Servers — Godot Docs](https://docs.godotengine.org/en/stable/tutorials/performance/using_servers.html)
- [Using SceneTree — Godot Docs](https://docs.godotengine.org/en/stable/tutorials/scripting/scene_tree.html)
- [Introducing GDExtension — Godot Blog](https://godotengine.org/article/introducing-gd-extensions/)
- [godot-cpp — GitHub](https://github.com/godotengine/godot-cpp)
- [Custom modules in C++ — Godot Docs](https://docs.godotengine.org/en/3.1/development/cpp/custom_modules_in_cpp.html)
- [GDScript vs C# in Godot 4 — Chickensoft](https://chickensoft.games/blog/gdscript-vs-csharp)
