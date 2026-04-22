# DCC + Asset Pipeline + Exchange Formats — research notes for ALZE Engine

Fecha: 2026-04-22. Round 5 / agente 7.
Target: ALZE Engine (`/root/repos/alze-engine`, C++17, SDL2 + OpenGL 3.3, sin RTTI/exceptions, equipo pequeño). Baseline actual: cgltf + stb_image + GLAD; el modelo de entrada hoy es **glTF 2.0**.
Alcance: cómo se mueve un asset desde el DCC del artista hasta la VRAM en runtime. Formatos de intercambio (FBX / glTF / USD / Alembic / OBJ / Collada / VRM), herramientas DCC (Blender / Maya / 3ds Max / Houdini / Substance / Megascans), lenguajes de material (MaterialX) y la cocina del asset (validator → converter → DDC → runtime).

No overlap: R1 `rendering_libs.md` menciona KTX2 + cgltf como piezas; R3 `virtual_textures_streaming.md` cubre Basis/KTX2 como contenedor de textura. Este documento empalma con ambos: de dónde salen los meshes/materiales que acaban en esos pipelines.

## 1. File format landscape — el panorama crudo

Un engine típicamente lee **uno o dos formatos de intercambio** y **muchos formatos nativos derivados**. La elección del formato de intercambio decide qué DCCs puedes soportar, cuánto código de import hay que escribir, y cuánto arte tiene que rehacerse si cambias.

### FBX (Autodesk Filmbox, 1996 → binario 2009+)

- Origen: Kaydara's Filmbox (1996), adquirido por Autodesk en 2006.
- Formato binario (y ASCII) propietario, SDK cerrado en C++. Licencia gratuita pero restrictiva (no-reverse-engineer).
- **El lingua franca AAA histórico 2005-2020.** Maya/Max/MotionBuilder lo exportan nativamente; Blender/Houdini lo soportan con importers de terceros (Blender usa `io_scene_fbx` en Python, emulando el SDK).
- Soporta: meshes, skinning + blend shapes, animación esqueletal, cámaras, luces, materiales (limitado — Phong/Lambert, no PBR estándar), nodos de transformación, metadata user properties.
- **Problemas crónicos:**
  - Binario no documentado públicamente (spec reverse-engineered por Blender Foundation). Cada versión de SDK rompe archivos viejos.
  - "Unit scale" hell: Maya-cm vs Max-inches vs Blender-m, rotaciones Euler/Quaternion mismatched, axis up (+Y vs +Z) incompatible según exporter.
  - Materiales: no hay estándar PBR. Cada engine inventa convenciones "Autodesk Standard Surface" o mapea a su shader.
- Todavía dominante en AAA porque Maya/Max siguen siendo el estándar. **En ALZE: no hacer nada con FBX hasta que alguien pida importarlo.**

### glTF 2.0 (Khronos, 2017)

- **El "JPEG de los 3D"** — Khronos positioning desde 2015 (glTF 1.0) y definitivamente desde 2.0 (junio 2017).
- Formato abierto, royalty-free, spec ~70 páginas en GitHub (`github.com/KhronosGroup/glTF`).
- **Dos sabores**:
  - `.gltf` (JSON texto + `.bin` buffer externo + texturas separadas). Debuggeable a mano.
  - `.glb` (un solo archivo binario con JSON embebido + bin chunk). Más compacto, menos archivos.
- **Lo que especifica canónicamente:**
  - Meshes (indexed triangles, points, lines) con POSITION, NORMAL, TANGENT, TEXCOORD_0..n, COLOR_0, JOINTS_0, WEIGHTS_0.
  - **PBR metallic-roughness baseline** (baseColor + metallic + roughness + normal + occlusion + emissive) — la primera spec masiva que hace PBR explícito.
  - Skeletal + morph target animation con `KHR_animation_pointer`.
  - Scene graph (nodes, transforms, cameras, lights via extension).
  - **Extensions ecosystem** (`KHR_*` ratificadas, `EXT_*` vendor): `KHR_draco_mesh_compression`, `KHR_texture_basisu` (KTX2!), `KHR_materials_clearcoat`, `KHR_materials_transmission`, `KHR_lights_punctual`.
- **Soportado por:**
  - Exportación nativa: Blender (desde 2.80, oficial), Godot 4 (primary format), Three.js, Babylon.js, UE5 (via plugin), Unity (via UPM package), Substance Painter/Designer.
  - Maya y 3ds Max: vía "Autodesk glTF Exporter" o Khronos plugins.
  - Viewers: glTF Sample Viewer (Khronos), gltf-viewer (Donmccurdy), Windows 3D Viewer, macOS Preview.
  - Parsers C/C++: **cgltf** (single-header, BSD-3, lo que ALZE ya usa), tinygltf (header-only C++), Microsoft glTF-SDK, yocto-gl.
- **Por qué ganó como formato de intercambio para motores medianos:**
  1. PBR standard resuelve el dolor material. Los DCC saben exportar metallic-roughness.
  2. JSON es introspectable y diffable en git.
  3. cgltf carga en ~500 LOC.
  4. KTX2 texture support vía `KHR_texture_basisu`.
- **Lo que NO soporta bien:**
  - Materiales complejos tipo shader graph (no hay MaterialX en glTF core).
  - Escenas gigantes con overrides tipo USD layers.
  - Físicas / colisiones (extension `KHR_physics_rigid_bodies` está en draft, no ratificada).
  - LODs con streaming — hay `MSFT_lod` como extension pero no es first-class.

### USD (Pixar Universal Scene Description, 2012 interno → 2016 open source)

- **Origen**: Pixar Animation Studios, diseñado por Guido Quaroni's team como sucesor de TidScene/Presto. Usado en pipeline Pixar desde Finding Dory (2016). Open source Apache 2.0 en GitHub (julio 2016).
- **Qué es realmente**: un *sistema de composición de escenas*, no solo un formato de archivo.
  - **Stage**: la escena "resuelta" visible al usuario.
  - **Layer**: un archivo `.usda` / `.usd` / `.usdc` (texto / crate binary / crate compressed) con prims (primitives).
  - **Prim**: un nodo con path jerárquico (`/World/Set/Chair_01`), con type (Mesh, Xform, Material, Scope, etc.) y properties (attributes + relationships).
  - **Reference / payload**: un prim puede *referenciar* otro layer, incluyendo composición LIVRPS (Local-Inherits-VariantSets-References-Payloads-Specializes) — el orden canónico de resolución Pixar.
  - **VariantSet**: un prim puede tener múltiples variantes (ej. "lowPoly" / "highPoly"), seleccionables per-shot.
- **Hydra**: el pipeline de rendering abstracto dentro de USD. `HdStream` (OpenGL/Metal basic), `HdStorm` (Pixar's hydra reference renderer), `HdPrman` (RenderMan bridge), `HdEmbree` (CPU path tracer). Ship-a-viewer-for-free cuando integras Hydra.
- **UsdPhysics**: extension oficial (2021+) para rigid body spec. No es runtime; describe qué debería simularse.
- **Tamaño de la dependencia**: OpenUSD es ~700k LOC de C++, compila en ~15-30 min, requiere TBB, Boost (hasta recientemente), Python, OpenSubdiv. **Integración seria = 20-50k LOC propios + la dependencia masiva.** NVIDIA Omniverse, Apple ARKit/RealityKit, Autodesk Maya (USD plugin), Houdini Solaris lo integran; cada uno invirtió año(s)-persona.
- **Apoyos industriales:**
  - **Apple** adopta USD como formato preferido para AR/VR (Reality Composer, visionOS, USDZ).
  - **NVIDIA** bases Omniverse enteramente en USD.
  - **Autodesk** Maya/3ds Max con USD plugins oficiales.
  - **Blender** añadió USD import/export desde 3.x (aún incompleto para material graphs).
  - **Khronos** en 2023 firmó un MoU con Pixar para armonizar glTF ↔ USD.
- **Para ALZE: overkill total en v1.** USD es correcto cuando tienes decenas de artists, shots múltiples, overrides, colaboración entre estudios. Para un equipo de 1-3 devs, el costo de integración come el proyecto.

### Alembic (Sony Pictures Imageworks + ILM, 2011)

- Formato open-source (New BSD) para "baked" scene caches: meshes deformados por frame, point clouds, curves, cameras. NO es authoring format — es cache format.
- Backends: **HDF5** (legacy) u **Ogawa** (lightweight binary, default desde 2013). Ogawa tiene mejor perf I/O.
- **Caso de uso**: un character animado en Maya/Houdini se "cachea" a un `.abc` por shot; el renderer ofline (RenderMan, Arnold, V-Ray) lee el cache y renderiza. Los games engines lo usan para VFX cinemática (cutscenes pre-renderizadas de simulación física/hair/cloth).
- **En games**: UE5 y Unity soportan import de Alembic para cutscenes o props animados costosos. Blender exporta/importa nativo.
- **Para ALZE: irrelevante.** Alembic sirve a VFX offline; no tiene lugar en un engine pequeño en runtime.

### OBJ (Wavefront, 1989)

- Texto ASCII de Wavefront Technologies, legacy absoluto.
- Solo meshes triangulares + UVs + normals + referencia a `.mtl` (material sidecar).
- **Sin animación, sin skinning, sin scene graph.** Un archivo = un mesh.
- Todavía útil para: prototipos, intercambio entre sculptors (ZBrush → Substance), testing. Cada DCC lo soporta.
- Para ALZE: útil como formato de test / debug. `tinyobjloader` es single-header y ~1k LOC.

### Collada (`.dae`, Khronos, 2004-2012)

- XML open standard originalmente de Sony/Khronos. Pensado como sucesor FBX pero nunca cuajó.
- Soportado por prácticamente todos los DCC pero mal (exporters inconsistentes, archivos enormes).
- **Deprecado de facto.** Khronos literalmente lo reemplaza con glTF 2.0. No usar.

### VRM (VRM Consortium, 2018)

- Formato japonés para **avatares humanoides** (VTubers, social VR) basado en glTF 2.0 + extension.
- Añade: bone naming estándar ("Hips", "Spine", "Head", "LeftEye"), blend shape conventions (A/I/U/E/O/Blink), "first person" camera setup, "spring bones" (secondary motion para pelo/ropa).
- **Nicho pero importante si ALZE toca metaverso/avatar tools.** VRChat, Resonite, Cluster, Neos lo soportan. Standard en la escena VRM-heavy de Unity (UniVRM).
- Para ALZE: relevante solo si apuntas a VR social. Fácil de añadir como extension sobre cgltf porque es glTF + fields extra.

### Tabla: asset format comparison

| Formato     | PBR | Animación      | Scene hierarchy | Licencia          | Tooling |
|-------------|-----|----------------|-----------------|-------------------|---------|
| **FBX**     | Parcial (no estándar) | Skeletal + blend shapes completo | Sí | Autodesk EULA (gratis pero restrictivo) | Autodesk DCCs nativo; resto via import quirks |
| **glTF 2.0**| Sí (metallic-roughness + extensions) | Skeletal + morph + 1-curve | Sí | Royalty-free (Khronos) | Soporte universal; exporters maduros |
| **USD**     | Sí (MaterialX, UsdPreviewSurface) | Completo + variants + overrides | Sí + composición | Apache 2.0 | Pixar, Apple, NVIDIA, Autodesk; huge dep |
| **Alembic** | No (cache-only) | Baked deformed | Sí (scope cache) | New BSD | DCCs offline + UE5/Unity para VFX |
| **OBJ**     | No | No | No (un mesh) | De facto público | Universal, pero solo mesh static |

## 2. USD deep dive

USD merece sección propia porque su diseño es radicalmente diferente al resto — y porque entender qué problema resuelve aclara cuándo vale adoptarlo.

### El problema que resuelve

Pixar en 2012 tenía un pipeline donde:
- El modelado entregaba un `.obj` / Maya file al layout dept.
- Layout construía un set, lo entregaba a lighting.
- Lighting añadía luces + materials, lo entregaba a animation.
- Si modeling cambiaba un prop al final, los downstream tenían que re-aplicar cambios manualmente.

USD resuelve esto con **layers + overrides**: cada departamento trabaja en su propio layer, refiriendo al layer upstream. Cambios propagan automáticamente vía composition.

### Modelo de datos

```
/World            (type Scope)
  /Set            (type Xform)
    /Chair_01     (type Mesh, references chair_model.usd)
      .points     (attribute)
      .faceVertexIndices
      .primvars:st  (UV)
    /Chair_02     (variant "red" selected from VariantSet)
  /Lights
    /KeyLight     (type SphereLight, radius 1.2, color (1,0.9,0.8))
```

El path jerárquico es la identidad canónica. `GetPrimAtPath("/World/Set/Chair_01")` devuelve el prim resolviendo todas las layers.

### Composition: LIVRPS

El orden de resolución de overrides (strongest → weakest):
1. **Local** — opiniones directas en el layer actual.
2. **Inherits** — propiedades heredadas de "class" prims.
3. **VariantSets** — variantes seleccionadas.
4. **References** — contenido de otros layers embebidos.
5. **Payloads** — referencias lazy (no cargadas hasta demand).
6. **Specializes** — herencia débil por default.

Entender LIVRPS es prerequisito para hacer cualquier cosa seria con USD — y es exactamente lo que convierte USD en "complejo".

### Hydra rendering

Hydra es el "render abstraction layer" dentro de USD. Un `HdRenderDelegate` implementa los hooks necesarios (draw meshes, apply materials, etc.); USD le entrega la escena resuelta.

- `HdStorm` — el hydra OpenGL reference renderer de Pixar. Runs de usdview.
- `HdPrman` — bridge a RenderMan. Production lighting.
- `HdEmbree` — CPU path tracer reference (útil para validación).
- `HdCycles`, `HdArnold`, etc. — community.

Si un engine integra Hydra, obtiene "ábreme este usd y renderízalo" gratis.

### UsdPhysics

Schema ratificado en USD para describir rigid bodies:
- `UsdPhysicsRigidBodyAPI` (masa, frozen, velocity).
- `UsdPhysicsCollisionAPI` (shape: box/sphere/mesh).
- `UsdPhysicsJoint` (fixed/revolute/prismatic/spherical).
- `UsdPhysicsScene` (gravity, stepper).

Describir una sim; NO la corre. Un engine que adopte UsdPhysics conecta sus datos a PhysX/Jolt/Bullet.

### Costo de integración

- OpenUSD source: ~700k LOC C++ + Python bindings.
- Build time: 15-30 min con TBB + (opcional Boost) + OpenSubdiv + Python + Alembic optional.
- Lib size: ~200 MB build debug; runtime libs suman ~80 MB.
- Integración mínima "puedo abrir un .usd y parsear mesh": ~2k LOC + la dep masiva.
- Integración real "cargar, composición correcta, Hydra delegate para mi renderer": 20-50k LOC propios + entender LIVRPS y Hydra.
- **Nvidia Omniverse** invirtió años-persona en su integración. No es "add dependency y ya".

Refs USD:
- OpenUSD docs — https://openusd.org/release/
- GitHub — https://github.com/PixarAnimationStudios/OpenUSD
- Guido Quaroni, "Introduction to USD," SIGGRAPH 2019 course — https://graphics.pixar.com/usd/files/Siggraph2019_USD.pdf
- "USD Composition Arcs" — https://openusd.org/release/glossary.html#livrps-strength-ordering
- NVIDIA Omniverse USD — https://docs.omniverse.nvidia.com/usd/latest/

### Para ALZE

USD no tiene lugar en v1 o v2. **Aspiracional en v3** si ALZE crece a equipo de 5+ con colaboración entre artistas y devs múltiples. El costo de integración y la curva de aprendizaje son prohibitivos para <3 devs.

## 3. glTF 2.0 deep dive

Ya resumido arriba; detallar qué contiene técnicamente y cómo ALZE lo explota.

### Estructura de un `.glb`

```
[12-byte header: magic "glTF" + version 2 + length]
[4 + 4 + JSON chunk bytes]
[4 + 4 + BIN chunk bytes]
```

El JSON describe: `scenes`, `nodes`, `meshes`, `materials`, `textures`, `images`, `samplers`, `accessors`, `bufferViews`, `buffers`, `animations`, `skins`. Los `accessors` apuntan a ranges del bin chunk con typed views (POSITION → vec3 floats starting at byte offset X).

### Materials canónicos

El PBR metallic-roughness base:

```json
"pbrMetallicRoughness": {
  "baseColorTexture": { "index": 0 },
  "metallicRoughnessTexture": { "index": 1 },
  "baseColorFactor": [1,1,1,1],
  "metallicFactor": 1.0,
  "roughnessFactor": 1.0
}
```

Extensiones útiles:
- `KHR_materials_clearcoat` — coche pintura coat encima del base.
- `KHR_materials_transmission` — vidrio / cristales (no es alpha blending, es refracción-aware).
- `KHR_materials_volume` — densidad, attenuation distance. Para vidrio con thickness.
- `KHR_materials_ior` — índice de refracción explícito.
- `KHR_materials_sheen` — telas (terciopelo).
- `KHR_materials_specular` — Dielectric specular override.
- `KHR_materials_anisotropy` — metal cepillado.
- `KHR_materials_iridescence` — nácar, jabón.
- `KHR_lights_punctual` — luces point/spot/directional con color + intensity + range.
- `KHR_texture_basisu` — textura referenciada es KTX2 supercompressed (BC/ASTC/ETC transcoding).
- `KHR_draco_mesh_compression` — mesh position/normal/UV comprimidos con Draco (geometric compression).

### cgltf — el parser ALZE usa

- Single-header en C89, ~8k LOC.
- BSD-3.
- Carga un `.gltf` o `.glb` en una estructura POD (`cgltf_data` con punteros crudos a `cgltf_mesh*`, `cgltf_accessor*`, etc.).
- **No decodifica imágenes** — delega a stb_image u otro cargador.
- **No carga a GPU** — produce CPU-side data; el engine hace los uploads.
- Soporta extensions (pasa los JSON raw; engine interpreta).

ALZE ya usa cgltf y stb_image. **Suficiente para v1.**

### Herramientas del ecosistema glTF

- **`gltf-validator`** (Khronos, Dart): valida spec compliance. CI-integrable.
- **`gltfpack`** (Arseny Kapoulkine, Meshoptimizer): optimiza glTF (quantiza accesos a menor precisión, vertex cache, Meshopt compression). Reduce tamaño 3-10x con calidad aceptable.
- **`glTF Sample Viewer`** (Khronos): viewer web reference con todas las extensions.
- **`gltf.report`** (Don McCurdy): web tool para inspeccionar + visualizar.
- **`meshoptimizer`** (Kapoulkine): lib C para reoptimizar mallas — index buffer remap, vertex cache, LOD simplification. glTF-friendly.

### Limitaciones honestas de glTF

- **Escenas gigantes** (ciudades enteras con 100k props): JSON se vuelve pesado. No hay composition tipo USD.
- **Author-time vs runtime**: glTF es a la vez formato intermedio y formato final. No hay un "cook" claro — el runtime consume lo mismo que el DCC exporta.
- **Custom gameplay data**: no hay lugar estándar para pondér "este mesh es un spawner / un trigger / un NPC con clase X". Cada engine añade extensions propietarias vía `extras` JSON object.
- **Mesh LODs**: `MSFT_lod` existe pero solo lo ALZE engine-side code tiene que implementar; no es first-class.

### Para ALZE

**Stay on glTF 2.0 como formato de intercambio.** Es la decisión correcta y ya está implementada. Añadir progresivamente support para:
1. `KHR_texture_basisu` (KTX2) para texture streaming (cruza con R3 virtual textures).
2. `KHR_materials_clearcoat` / `KHR_materials_transmission` si el juego tiene vidrio/coches realistas.
3. `KHR_lights_punctual` para exportar luces desde Blender (evita hacer scripting ad hoc).
4. `EXT_meshopt_compression` para bajar disk footprint.

Refs glTF:
- Spec — https://github.com/KhronosGroup/glTF/tree/main/specification/2.0
- Khronos glTF landing — https://www.khronos.org/gltf/
- cgltf — https://github.com/jkuhlmann/cgltf
- gltf-validator — https://github.khronos.org/glTF-Validator/
- gltfpack — https://github.com/zeux/meshoptimizer/blob/master/gltf/README.md
- meshoptimizer — https://github.com/zeux/meshoptimizer
- Don McCurdy gltf-transform — https://gltf-transform.dev/

## 4. Blender como DCC principal

Blender Foundation, Ton Roosendaal, GPLv2+. Open source, gratis, multi-plataforma.

### Por qué Blender ganó la conversación para games indie/mid

- **Gratis** — sin licencia AAA por seat anual.
- **Todo-en-uno** — modeling + sculpting + UVs + painting + rigging + animation + VFX + compositing + video edit + game tooling.
- **Python API completa** (`bpy` module) — casi cualquier cosa del UI es scriptable. Exporters custom se escriben en ~200 LOC.
- **Adopción industry:** Khronos **3D Commerce** (estandar para ecommerce 3D) usa Blender como referencia. Ubisoft donó 2019-2022. Epic, NVIDIA, AMD tienen development fund contributions. Intel, Google, Adobe sponsorizan releases.
- **Pipeline de games-specific features en 4.x (2023-2026):**
  - glTF import/export nativo oficial (no plugin) con soporte extensions.
  - Auto-smooth + custom normals confiables (antes era bug source).
  - **Geometry Nodes** — procedural mesh generation node graph. Herramienta para crear LODs, scatter, procedural meshes.
  - **Grease Pencil** 2D-in-3D si el arte estilizado.
  - **Texture baking** integrado — bake normal/AO/cavity desde highpoly a lowpoly sin pasar por Substance.
  - **Cycles-X** path tracer integrado para bake de lightmaps / AO / light probes.
  - **Asset Browser** (3.0+) — librería compartida de meshes/materials/actions reutilizables cross-file.
  - **Viewport PBR** EEVEE que approxima glTF render (importante para QA de export).

### Flujo típico Blender → glTF → ALZE

1. Artista modela en Blender, UV unwrap con Smart UV / manual.
2. Material creado con Principled BSDF (Blender's BRDF que mapea a glTF metallic-roughness 1:1 en export).
3. Texturas pintadas en Substance o bakeadas desde highpoly.
4. Rig + weights si es character; action strips.
5. File → Export → glTF 2.0 (.glb). Opciones: embed/separate textures, Draco compression, tangents.
6. glb consume por cgltf en ALZE.

Este flujo *funciona hoy* y es lo que ALZE debería documentar como path canónico.

### Blender 4.x highlights para ALZE

- **glTF exporter 4.0+** soporta KHR extensions ratificadas + custom extensions via plugin hook.
- **Attributes system** extensible — se pueden exportar custom per-vertex data (motion vectors, painted masks).
- **Real-time compositor** en viewport para pipeline tests.
- **Vulkan render backend** experimental en 4.3+ (para Blender mismo, no exports).
- **EEVEE Next** (4.2+) — rewrite del motor realtime, mejor approximation de Cycles. Pre-visualiza PBR en editor antes de exportar.

### Python scripting para pipelining

```python
import bpy
# Export all selected to glTF with ALZE convention
bpy.ops.export_scene.gltf(
    filepath="/tmp/assets/character.glb",
    export_format='GLB',
    export_draco_mesh_compression_enable=True,
    export_image_format='AUTO',   # WEBP where possible
    export_apply=True,             # apply modifiers
    export_yup=False,              # keep Z-up if ALZE uses Z-up
    export_selected=True,
)
```

ALZE puede distribuir un add-on de Python que:
- Valida convenciones (materiales llamados `mat_foo` no `Material.001`).
- Exporta con defaults ALZE-correct.
- Añade extras JSON con metadata custom (collision shape, gameplay tags).

### Limitaciones honestas de Blender

- **Animation polish**: Maya tiene mejor workflow para character animation AAA (Graph Editor, constraint system más maduro). Blender 4.x cerró gap pero Maya sigue marginalmente superior en rigging profundo.
- **USD** import/export en Blender aún incompleto (material graph overrides missing).
- **FBX** bidireccional pero algunos quirks (custom properties a veces pierden).
- **Memoria**: sculpting de multi-M polys puede saturar. ZBrush sigue siendo el rey sculpt.

Refs Blender:
- blender.org — https://www.blender.org/
- Python API — https://docs.blender.org/api/current/
- glTF addon — https://docs.blender.org/manual/en/latest/addons/import_export/scene_gltf2.html
- Khronos 3D Commerce (usa Blender) — https://www.khronos.org/3dcommerce/

## 5. Maya + 3ds Max — el duopolio Autodesk

### Maya (Alias/Wavefront 1998 → Autodesk 2005+)

- Standard AAA animation + modeling desde 2005. Pixar, ILM, Weta históricamente Maya-centric.
- MEL (Maya Embedded Language, 1998) + Python (2.x 2008+, 3.x 2022+). PyMEL wrapper popular.
- Precio: ~$1,875/año (indie ~$295/año bajo threshold revenue).
- **Strengths**: rigging profundo, animation layers, dynamics Nucleus, integraciones USD/Arnold/Bifrost, pipelines industriales.
- **Para games**: FBX export maduro; glTF via Khronos/Autodesk plugin. USD plugin oficial.

### 3ds Max (Autodesk 1996+)

- Standard para **arquitectura**, env modeling, y históricamente games Western (Epic UT/Gears of War era, muchos env artists siguen en Max).
- MAXScript (legacy) + Python desde 2017+.
- Precio: idéntico a Maya (~$1,875/año).
- **Strengths**: modifier stack intuitivo, hard-surface modeling, V-Ray ecosystem, arquitectura.
- **Para games**: FBX nativo; glTF via Babylon.js exporter comunidad.

### Juntos — por qué aún dominan AAA

- Décadas de investment en training (muchos artists conocen solo Maya/Max).
- Estudios AAA tienen licensing enterprise negociado.
- Pipelines custom construidos encima en ~20 años (rigging rigs, VFX tools).
- Autodesk SDK (devkit) para FBX/Maya API C++ maduro para tools internos.

### Para ALZE

No soportar Maya/Max directamente. **Soportar glTF 2.0 y los DCCs exportan a él.** Ahorrás FBX SDK (cerrado, licencia) y el dolor del import parsing.

Si un colaborador insiste en Maya, puede instalar el plugin "Khronos glTF Exporter for Maya" (oficial) y seguir el flujo glTF.

Refs:
- Maya product — https://www.autodesk.com/products/maya/
- 3ds Max product — https://www.autodesk.com/products/3ds-max/
- Khronos Maya glTF exporter — https://github.com/KhronosGroup/glTF-Maya-Exporter

## 6. Houdini + Houdini Engine — procedural

SideFX Houdini. El estándar de la industria para VFX procedural + increasingly game content authoring.

### Houdini como DCC

- Nodal/procedural desde el día 1. Todo es un node graph evaluable.
- SOP (Surface Operators) para geometry, VOP (VEX Operators) para shaders/compute, DOP (Dynamics) para simulation, COP (Composition), CHOP (motion channels).
- VEX language (C-like) compilable a VEX bytecode o JIT OpenCL para GPU execution.
- Precio: Houdini Indie $269/año (render limits), Houdini Core $1,995/año, Houdini FX $4,495/año.
- **Apprentice** gratuita con watermark — útil para aprendizaje.

### Houdini Engine

SDK **C++ (HAPI)** que permite ejecutar asset Houdini desde **dentro de otro DCC o game engine**. Es el secreto de por qué Houdini ganó tracción AAA.

- **Houdini Digital Asset (HDA)**: un "nodo comprimido" que encapsula un procedural tool. Inputs (meshes, parameters), outputs (meshes, textures, spawn points).
- **Houdini Engine plugins oficiales**:
  - **Unreal Engine 5**: plugin first-party desde UE4. Editor in-UE permite drag HDA → ejecuta SideFX → regenera asset procedural.
  - **Unity**: plugin similar.
  - **Maya / 3ds Max / Cinema 4D**: integraciones.
- **Casos de uso en games shipped:**
  - **Far Cry 5 / 6** (Ubisoft): terrain procedural generation via Houdini HDAs.
  - **Horizon Zero Dawn / Forbidden West** (Guerrilla/Decima): vegetation, procedural placement.
  - **Jedi Fallen Order / Survivor** (Respawn): environment building blocks.
  - **Spider-Man 2** (Insomniac): city block generation.
  - **CoD Warzone** (IW): POI procedural variations.
- **Heightfield terrain**: Houdini SOP network para erosion, masks, biome distribution. UE5 imports como Landscape + RVT material.
- **Procedural vegetation**: scatter rules, age/health variants, biome falloff. Exporta a Megascans/UE5 Foliage Tool.

### Para ALZE

Houdini Engine SDK es C++ con headers pulidos. **Aspiracional v2**: ALZE podría añadir un panel "Houdini Asset" que ejecuta HDAs para generar terrain / props. Requiere:
- Licencia Houdini Engine (incluida en Indie $269/año).
- Integración HAPI (~3-5k LOC, bien documentado).
- UI para exponer parameters de HDA en editor ALZE.

**No en v1.** Primero tener un editor funcional; Houdini Engine llega cuando procedural content authoring se vuelve cuello de botella.

Refs:
- SideFX Houdini — https://www.sidefx.com/products/houdini/
- Houdini Engine docs — https://www.sidefx.com/docs/houdini/hengine/
- HAPI C API — https://www.sidefx.com/docs/hapi/
- Unreal plugin — https://www.sidefx.com/products/houdini-engine/plug-ins/unreal-plug-in/

## 7. Substance 3D — la suite de PBR texture authoring

Historia: Allegorithmic (Francia, 2003). Adquirida por **Adobe en enero 2019** por ~$3B. Renombrado "Substance 3D" en 2021.

### Substance Painter

- 3D texture painting WYSIWYG sobre meshes.
- **Smart Materials** — materials con parámetros + dynamic layering (edge wear, dirt, grunge). Lo que hace Painter único: los materiales "se pintan solos" en bordes / cavidades usando curvature + AO como mascaras auto.
- **Export presets**: metallic-roughness (glTF), UE4/5, Unity Standard/HDRP, Arnold, V-Ray, custom.
- Precio: $19.99/mes o ~$240/año con suite (Painter + Designer + Sampler + assets).

### Substance Designer

- **Node graph-based material authoring** — procedural textures generadas por node graph sin pixel paint.
- Cada `.sbs` es un grafo; se "cocina" a un `.sbsar` (archivo compilado) que cualquier software Substance-compatible puede consumir con parameter exposed en sliders.
- **Caso de uso clásico**: crear un material "brick wall" paramétrico (height, bricks count, mortar color, weathering) → reusable en 100 escenas.

### Substance Sampler

- Imagen real → material PBR (fotogrametría casual + AI). Útil para capturar texturas de fotos.

### Substance Automation Toolkit (SAT)

- Command-line tools + Python API para batch processing `.sbs`/`.sbsar` → cocinar a texturas PNG/EXR/TGA.
- **Usado en AAA pipelines** para renderizar materials por lote en CI.
- Incluye `sbsbaker`, `sbsrender`, `sbscooker`, `sbsmutator`.

### Para ALZE

**v1 recomendado**: Substance Painter para "hero assets" (character textures, weapon details). ~$240/año es razonable. El asset master es el `.spp` file (Painter native); el export es glTF-compatible PBR metallic-roughness.

**v2 optional**: Substance Designer para procedural material library reutilizable. Curva de aprendizaje más alta — nodal mindset.

**Alternativas free**:
- **Armor Paint** (open source, ~$19 one-time): similar Painter concept.
- **Material Maker** (GPLv3): similar Designer concept.
- **Blender texture painting + baking**: menos capable pero free y dentro del DCC.

Refs Substance:
- Adobe Substance 3D — https://www.adobe.com/products/substance3d.html
- Painter docs — https://substance3d.adobe.com/documentation/spdoc/substance-3d-painter-20316164.html
- SAT — https://helpx.adobe.com/substance-3d-sat/home.html
- Armor Paint — https://armorpaint.org/
- Material Maker — https://www.materialmaker.org/

## 8. Megascans + Quixel Bridge

Quixel Megascans: biblioteca de assets 3D scanneados (photogrammetry) + texturas + vegetación.

- **Quixel**: start-up sueca, librería crecía desde 2014.
- **Adquirida por Epic Games en 2019**. **Gratis para usuarios de UE5** (license para UE vehicles).
- **2025 update**: Epic transitioned Megascans → Fab (Epic's unified marketplace). Parte gratis, parte paga.
- **Contenido**: ~15k assets (rocks, walls, vegetation, surfaces, decals) en resolución 8k-16k, con topología LOD ready.
- Formatos: FBX + OBJ + texturas PNG/EXR (albedo, normal, roughness, displacement, AO, cavity, opacity).
- **Export a otros engines**: Quixel Bridge tool exporta a FBX + PNG con presets específicos (Unity, Blender, custom).

### Usabilidad en ALZE

- Un asset Megascans → Blender (import FBX) → set material Principled BSDF con texturas PNG → export glTF → cgltf.
- Funciona. El cuello de botella es el conversión FBX → glTF (Blender la hace).
- Licencia permite uso en cualquier engine si descargaste con cuenta de UE5. Check current Fab license para non-UE usage.

**Para ALZE v1**: usar Megascans para environment art rápido. Free content de alta calidad. Ahorra semanas de modelado.

Refs:
- Quixel Megascans (legacy) — https://quixel.com/megascans
- Fab marketplace — https://www.fab.com/
- Quixel Bridge app — https://quixel.com/bridge

## 9. MaterialX — lenguaje de shader graph interchange

### Origen y adopción

- **Industrial Light & Magic** (ILM, Lucasfilm) propone en 2012. Open source (Apache 2.0) desde 2017.
- Now Academy Software Foundation project (alongside USD, OpenEXR, OpenColorIO, OpenVDB).
- **Adoptado por**: Pixar (USD native), Autodesk Maya, SideFX Houdini, Epic UE5, NVIDIA Omniverse, Adobe Substance 3D, Foundry Modo, Chaos V-Ray, AMD Radeon Pro Renderer.

### Qué es exactamente

**Spec + file format (`.mtlx`, XML) para describir shader graphs portables entre DCCs y renderers.** Dos piezas:

1. **Shader graph language**: nodos con inputs/outputs tipados, conexiones, metadata. Similar a UE Material Editor pero formato portable.
2. **Standard Surface + Lama + UsdPreviewSurface**: shading models estándar sobre los que los nodes se ensamblan.

### Qué resuelve

Pre-MaterialX: "hice este material en Substance Designer, ahora cómo lo uso en UE, Unity, V-Ray, y Arnold?" Cada uno tenía su propio shader graph. MaterialX da un formato común.

### Flujo típico

1. Artista autor en Substance Designer / Houdini / Maya hypershade.
2. DCC exporta MaterialX `.mtlx`.
3. Engine/Renderer target importa `.mtlx` y traduce a su internal shader (UE → Material Expression graph; Arnold → OSL; Hydra → HdMaterial).
4. Runtime compila a shader target (HLSL / GLSL / SPIR-V / MSL).

### Para ALZE

**Aspiracional v2+.** Útil cuando:
- ALZE tiene material graph editor propio.
- Hay 10+ materials complejos y querés portabilidad entre DCCs.

**v1 recomendado: NO MaterialX.** Un shader HLSL escrito a mano por desarrollador + parametrizado via glTF PBR factors cubre el 95% de necesidades.

Refs MaterialX:
- materialx.org — https://materialx.org/
- GitHub — https://github.com/AcademySoftwareFoundation/MaterialX
- MaterialX spec — https://materialx.org/assets/MaterialX.v1.39.Spec.pdf
- Doug Smythe, "MaterialX: A Multi-vendor, Multi-package Shader Interchange Format," SIGGRAPH 2018

## 10. Asset baking pipeline — de source a runtime

El flujo canonical AAA:

```
[source assets] → [validator] → [converter] → [DDC / cook cache] → [runtime pack] → [runtime load]
   .blend          gltf-valid    glb -> .alze    per-platform       .pak / archive   memcpy to GPU
   .fbx            custom rules  basisu          cooked blob        Zstd/Oodle
   .sbsar          asset scheme  meshoptimizer
```

### Source assets

- Los "source" (`.blend`, `.fbx`, `.spp`) viven en un repo separado del engine (Git LFS, Perforce, o DVC). Nunca se cargan en runtime.
- Un exporter script produce el formato de intercambio (`.glb`, `.mtlx`) checked-in en el repo del juego.

### Validator

- Reglas específicas del proyecto. Ejemplo:
  - Materiales deben tener nombres `mat_*`.
  - Meshes no pueden exceder 100k triangles.
  - Texturas deben ser potencia de 2 (o tener excepciones marcadas).
  - Skin bones ≤ 128.
  - glTF spec compliance (gltf-validator output limpio).
- Ejecutado en CI. Un fallo de validación = artist PR se bloquea.

### Converter / Cook

- Transforma el intermedio a formato runtime-optimized.
- Operaciones:
  - Reoptimize vertex cache (meshoptimizer).
  - Quantize attributes (pos → int16, UV → uint16, normal → SNORM8).
  - Generate LODs (meshopt simplification).
  - Transcodificar texturas KTX2 → BC7/ASTC/ETC por plataforma.
  - Generate tangents si el DCC no los exportó.
  - Pack meshes en archives (ver "runtime pack" below).

### DDC (Derived Data Cache)

- Concepto de Epic: el output del cook se cachea **keyed by hash of inputs + cook version**. Si cambias un triangle en un mesh, solo ese mesh se recooked; todo lo demás se hit del cache.
- **Local DDC**: `~/UnrealEngine/DerivedDataCache/` del dev. Instant iteration.
- **Shared DDC**: network filesystem compartido entre todo el equipo. Un dev cooking = todos se benefician.
- **Cloud DDC**: Epic ofrece "Horde Storage" para CI + remoto.
- **Invalidation**: hash = content hash(inputs) + cook version + platform. Cualquier cambio → miss → re-cook.
- Docs: https://dev.epicgames.com/documentation/en-us/unreal-engine/derived-data-cache-in-unreal-engine

### Runtime pack (archive format)

- Un único blob (`.pak` UE, `.bundle` Unity, `.rpf` Rockstar, `.pck` Godot, `.bsa` Bethesda).
- Index header + stream de datos cookeados comprimidos (Zstd, Oodle Kraken, LZ4).
- Runtime: mmap + decompress on-demand. Cross-ref R4 `fromsoftware.md` para el `.bdt/.bhd` de FromSoft, y R1 `rage_rockstar.md` para el `.rpf`.

### Incremental builds + cache invalidation

- **Content hash determinístico**: el converter debe ser determinístico byte-por-byte con los mismos inputs, o el DDC no hittea. Gotchas:
  - Timestamps en output → NO poner.
  - Hash maps con iteration order no determinístico → evitar.
  - Floating point non-determinism (denormals, SIMD compile flags).
- **Granularity**: cachear por asset (mesh + su textura bakeadas separadas). Un mesh cambia → solo su blob re-cook.
- **Build server CI**: correr cook completo en CI a nightly. DDC-cached produce build rápido.

### Frostbite asset cooker (DICE, EA)

- Cross-cutting con R1 `rendering_libs.md` Frostbite mentions. Cooker in-process con engine para iterar rápido ("bake on save"). Fuente: Sean Murray GDC talks + DICE tech blog.

### Para ALZE

**v1 minimo viable:**

1. Source repo separado: artists committean `.blend` + `.glb` generado con Blender addon.
2. **Validator** sencillo en Python: lee `.glb` con pygltflib, checks reglas básicas (naming, bone count, material PBR). CI fail on rule break.
3. **Converter**: tool Python que llama `basisu` para transcodificar texturas y `meshoptimizer`'s `gltfpack` para optimizar meshes. Output: un `.alze.glb` por asset.
4. **DDC local**: un directorio `~/.alze/ddc/` con hash-keyed blobs. Hash = SHA-256(input bytes + cook version string).
5. **Runtime pack**: v1 no lo necesitamos — cargar archivos individuales del file system. Pack cuando haya 1000+ assets o para ship.

**v2 upgrades:**
- Shared DDC via S3 / filesystem compartido.
- Incremental cook parallel (multiprocess).
- Per-platform variant (ALZE_PC vs ALZE_ANDROID con diferentes tile/quality).

**v3 aspiracional:**
- UE5-style DDC con content addressable store (CAS).
- Archive format propietario (`.alze` pack).

## 11. Tabla — DCC tool comparison

| Herramienta     | Precio                 | Strengths                                            | API / Scripting   | Calidad exporter glTF |
|-----------------|------------------------|------------------------------------------------------|-------------------|------------------------|
| **Blender**     | Gratis (GPLv2+)        | All-in-one, Python API, glTF nativo                 | Python 3 (`bpy`)  | Excelente (oficial Khronos contrib) |
| **Maya**        | ~$1,875/año (indie ~$295) | Animation, rigging, industrial pipelines          | MEL + Python 3    | Bueno (plugin Khronos oficial) |
| **3ds Max**     | ~$1,875/año            | Modifier stack, arch-viz, hard-surface              | MAXScript + Python | Medio (plugin comunidad Babylon) |
| **Houdini**     | Indie $269/año → FX $4,495 | Procedural everything, VFX, terrain               | VEX + Python + HAPI C++ | Bueno para mesh; materiales limitados |
| **Substance Painter** | ~$240/año (suite)  | PBR texture painting, smart materials               | Python + JS UI    | N/A (es texture tool, no mesh) |
| **Substance Designer** | ~$240/año (suite) | Procedural material node graph                      | Python + SBS nodes | N/A (material output) |
| **ZBrush**      | ~$400/año o $895 one-time | Sculpting millions of polys                       | ZScript (MAX-style) | Vía GoZ bridge o export OBJ |
| **Armor Paint** | ~$19 one-time OSS      | Substance Painter alternative                       | Haxe + UI         | Export PBR glTF-compatible textures |

## 12. ALZE applicability — tres versiones

### v1 (HOY, GL 3.3 + cgltf)

**Stack:**
- **DCC principal**: Blender 4.x (gratis, exporter glTF nativo).
- **Sculpting**: Blender + (optional ZBrush si team puede afford) o free alt (Nomad Sculpt trial, Dust3D).
- **Texture authoring**: Substance Painter ~$240/año para hero assets; Blender built-in para utility textures.
- **Assets scanned**: Megascans (gratis en Fab con UE account) como env content rapido.
- **Formato intercambio**: glTF 2.0 (`.glb`) con `KHR_texture_basisu` + `EXT_meshopt_compression`.
- **Textures**: KTX2 + Basis Universal transcoded a BC7 (PC) en load time.
- **Runtime parser**: cgltf + stb_image (status quo).
- **Cook**: Python script que llama gltfpack + basisu. Hash-keyed DDC local en `~/.alze/ddc/`.
- **Validation**: gltf-validator en pre-commit hook o CI.

**Qué NO hacer:**
- NO FBX import. Artists usan Blender (o Maya con plugin Khronos) y exportan glTF.
- NO USD. Overkill.
- NO MaterialX. Usar PBR metallic-roughness baseline + un par de extensions.
- NO custom archive format. File-per-asset con hash-keyed dir es suficiente hasta 5k+ assets.
- NO Houdini Engine. Procedural content se hace en Blender Geometry Nodes por ahora.

**Costo monetario estimado equipo 1-3 devs:**
- Substance 3D Collection: ~$240/año × 1-2 seats = $240-480/año.
- Blender: gratis.
- Megascans via Fab/UE: gratis.
- Total: ~$240-480/año. Muy razonable.

### v2 (6-12 meses, con Vulkan u GL 4.4)

**Adiciones:**
- **Houdini Engine** integration (Indie $269/año + HAPI C++): HDAs para terrain procedural, scatter, props variations. ~3-5k LOC integration.
- **MaterialX** experimental support para material interchange entre Substance Designer / Blender. ~2-3k LOC en ALZE tools + runtime translate a shader HLSL.
- **DDC compartido** via S3 o Nginx filesystem para equipos distribuidos.
- **Multi-platform cook** (Android ASTC, PC BC7, ... mismo KTX2 source).
- **Advanced glTF extensions**: `KHR_materials_transmission` (vidrio), `KHR_lights_punctual` (luces desde DCC).

### v3 (12-24 meses, open-world grande)

**Adiciones aspiracionales:**
- **USD** integration con Hydra delegate custom para ALZE renderer. 20-50k LOC. Solo si ALZE tiene 5+ devs + colaboración artistas/programadores + Apple AR / NVIDIA Omniverse target.
- **Content addressable store** (CAS) estilo UE5 Zen / Perforce Helix. DDC de próximo nivel.
- **Custom archive format** `.alze` con streaming I/O async + Oodle Kraken compression.
- **RVT/SVT** integration con texture cooker que bakea virtual texture tiles desde Megascans/Substance (cross-ref R3 `virtual_textures_streaming.md` v3).

## 13. Veredicto honesto v1

**Recomendación concreta para ALZE hoy:**

- **DCC**: **Blender 4.x** como primary (gratis, Python exhaustivo, glTF oficial).
- **Formato intercambio**: **glTF 2.0** (ya implementado vía cgltf — no cambiar).
- **Textures**: **KTX2 + Basis Universal** con transcoding runtime a BC7 (cross-ref R1/R3 notas). Añadir `KHR_texture_basisu` al cgltf parsing.
- **Texture authoring**: **Substance Painter** ~$240/año para hero assets. Blender internal para utility. Megascans para env art.
- **Cook**: script Python que ejecuta `gltfpack` (meshopt) + `basisu` (KTX2). DDC con SHA-256 hash de input + cook version en `~/.alze/ddc/`.
- **Validation**: `gltf-validator` + reglas custom Python en pre-commit / CI.
- **Runtime parser**: **cgltf** (status quo) + stb_image + libktx (nueva dep ~50k LOC, muy focused).

### Honesto: qué NO hacer

- **NO pelear FBX.** El FBX SDK es cerrado, licenciado, inestable entre versiones. Si un colaborador entrega FBX, que lo convierta a glTF en Blender antes de committear. No escribir FBX import custom.
- **NO inventar un formato custom.** "Nuestro propio `.alze3d` binario" es la tentación de principiante. Todos los engines AAA que lo hicieron (Source VMF/VTF, id Tech PAK, Bethesda BSA) lo hicieron antes de glTF existir. Hoy tenés glTF. Úsalo como *intermediate* y, si hace falta, introducí un cook blob binario (`.alze.glb` con custom extras) pero NO inventes desde cero.
- **NO USD hasta que scope justifique.** Es una decisión que define un engine AAA con varias manos. Para 1-3 devs, USD es una dependencia que come tu productividad.
- **NO MaterialX hasta que haya 20+ materials distinct y colaboración cross-DCC.**
- **NO Houdini Engine hasta que procedural content sea el dolor #1** — generalmente es terrain o props variation. Pre-ese dolor, Blender Geometry Nodes cubre.

### El punto clave

**glTF 2.0 cubre el 95% de las necesidades de un engine pequeño.** El 5% restante (escenas gigantes, shader portability, procedural AAA, AR/VR platform) son problemas de equipos de 10-100 personas, no de 1-3. Gastar ingeniería anticipadamente en USD / FBX / MaterialX / Houdini Engine es gastar el único recurso finito (tiempo-dev) en cosas que aún no duelen.

**La disciplina es**: empezar con glTF + Blender + KTX2 + cgltf + Substance. Medir qué duele. Responder al dolor real. Esa es la diferencia entre un engine que shipea y uno que permanece en `src/todo/future_usd_integration/`.

---

## Fuentes consultadas

### glTF
- Khronos glTF 2.0 spec — https://github.com/KhronosGroup/glTF/tree/main/specification/2.0
- Khronos glTF landing — https://www.khronos.org/gltf/
- cgltf (Johannes Kuhlmann) — https://github.com/jkuhlmann/cgltf
- tinygltf (Syoyo Fujita) — https://github.com/syoyo/tinygltf
- meshoptimizer + gltfpack (Arseny Kapoulkine) — https://github.com/zeux/meshoptimizer
- gltf-validator (Khronos) — https://github.khronos.org/glTF-Validator/
- Don McCurdy gltf-transform — https://gltf-transform.dev/
- Khronos, "Why glTF?" — https://www.khronos.org/news/press/khronos-releases-gltf-2.0-specification

### USD (OpenUSD)
- openusd.org — https://openusd.org/release/
- GitHub Pixar OpenUSD — https://github.com/PixarAnimationStudios/OpenUSD
- Guido Quaroni, "USD at Pixar," SIGGRAPH 2019 — https://graphics.pixar.com/usd/files/Siggraph2019_USD.pdf
- USD Composition — https://openusd.org/release/glossary.html#livrps-strength-ordering
- NVIDIA Omniverse USD docs — https://docs.omniverse.nvidia.com/usd/latest/
- Apple USDZ — https://developer.apple.com/documentation/realitykit/usdz_file_format_specification
- Autodesk USD for Maya — https://help.autodesk.com/view/MAYAUL/2024/ENU/?guid=GUID-7B19A81E-CC13-4937-9D59-72F6E7C23FB9

### FBX / Autodesk
- Autodesk FBX SDK — https://www.autodesk.com/developer-network/platform-technologies/fbx-sdk-2020-3
- Blender FBX reverse-engineering notes (`io_scene_fbx`) — https://github.com/blender/blender/tree/main/scripts/addons_core/io_scene_fbx
- Maya product — https://www.autodesk.com/products/maya/
- 3ds Max product — https://www.autodesk.com/products/3ds-max/

### Blender
- blender.org — https://www.blender.org/
- Python API — https://docs.blender.org/api/current/
- glTF addon docs — https://docs.blender.org/manual/en/latest/addons/import_export/scene_gltf2.html
- Khronos 3D Commerce (Blender como reference DCC) — https://www.khronos.org/3dcommerce/
- Blender 4.3 release notes — https://www.blender.org/download/releases/4-3/

### Houdini
- SideFX Houdini — https://www.sidefx.com/products/houdini/
- Houdini Engine — https://www.sidefx.com/products/houdini-engine/
- Houdini Engine docs — https://www.sidefx.com/docs/houdini/hengine/
- HAPI C API — https://www.sidefx.com/docs/hapi/
- UE5 plugin — https://www.sidefx.com/products/houdini-engine/plug-ins/unreal-plug-in/
- GDC "Procedural Tools with Houdini" talks — https://www.sidefx.com/tutorials/gdc/

### Substance / Adobe
- Adobe Substance 3D — https://www.adobe.com/products/substance3d.html
- Painter docs — https://helpx.adobe.com/substance-3d-painter/home.html
- Designer docs — https://helpx.adobe.com/substance-3d-designer/home.html
- SAT (Substance Automation Toolkit) — https://helpx.adobe.com/substance-3d-sat/home.html
- Adobe Substance acquisition press — https://news.adobe.com/news/news-details/2019/Adobe-To-Acquire-Allegorithmic-Leader-In-3D-Editing-And-Authoring-Software-For-Games-Entertainment-And-Design/default.aspx

### Megascans / Quixel
- Quixel Megascans (legacy) — https://quixel.com/megascans
- Fab marketplace — https://www.fab.com/
- Quixel Bridge — https://quixel.com/bridge
- Epic acquisition press 2019 — https://www.unrealengine.com/en-US/blog/quixel-joins-epic

### MaterialX
- materialx.org — https://materialx.org/
- GitHub AcademySoftwareFoundation — https://github.com/AcademySoftwareFoundation/MaterialX
- Spec PDF — https://materialx.org/assets/MaterialX.v1.39.Spec.pdf
- Smythe & Stone, "MaterialX: An Open Standard for Network-Based CG Object Looks," SIGGRAPH 2018 — https://dl.acm.org/doi/10.1145/3214745.3214781

### Alembic / Collada / VRM
- Alembic — http://www.alembic.io/
- Alembic GitHub — https://github.com/alembic/alembic
- Collada legacy spec — https://www.khronos.org/collada/
- VRM Consortium — https://vrm.dev/en/
- UniVRM (Unity plugin) — https://github.com/vrm-c/UniVRM

### Asset baking / DDC
- Epic, "Derived Data Cache in Unreal Engine" — https://dev.epicgames.com/documentation/en-us/unreal-engine/derived-data-cache-in-unreal-engine
- Epic, "UnrealBuildTool / UAT overview" — https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-build-tool-in-unreal-engine
- Frostbite tech blog — https://www.ea.com/frostbite/news
- Basis Universal (Rich Geldreich) — https://github.com/BinomialLLC/basis_universal
- KTX-Software (libktx) — https://github.com/KhronosGroup/KTX-Software
- Epic Horde (cloud DDC) — https://dev.epicgames.com/documentation/en-us/unreal-engine/horde-for-unreal-engine

### ALZE relevant
- ALZE repo local — `/root/repos/alze-engine` (cgltf + stb_image + GL 3.3 baseline)
- R1 rendering_libs.md (KTX2 + Basis mention) — `/root/lab_journal/research/alze_engine/rendering_libs.md`
- R3 virtual_textures_streaming.md (KTX2 detail) — `/root/lab_journal/research/alze_engine/r3/virtual_textures_streaming.md`
- R4 fromsoftware.md (archive format precedent) — `/root/lab_journal/research/alze_engine/r4/fromsoftware.md`
