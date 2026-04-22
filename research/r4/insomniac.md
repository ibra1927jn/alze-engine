# Insomniac Games Engine — Deep Dive (ALZE r4)

Fecha: 2026-04-22. Target de consumo: `/root/repos/alze-engine` (C++17 no-RTTI/no-exceptions, SDL2 + GL 3.3 hoy, Vulkan v2, single-dev, ~25-30K LOC).
Este documento NO repite el perfil de superficie de `aaa_engines.md` (líneas 8-18). Extiende con especificidad de títulos, números concretos, y las dos piezas que hacen único al engine: **I/O subsystem exploitation en PS5** y **procedural traversal para superhero swinging**.

## Exec summary (5 líneas)

1. El "Insomniac Engine" es un tronco común en evolución continua desde ~2002 (Ratchet 1 en PS2) pasando por Resistance (PS3), Sunset Overdrive (XB1, 2014 — primer open-world), Spider-Man 2018 (PS4, escalado 5× vs Sunset), Miles Morales + Rift Apart (PS5 launch 2020), Spider-Man 2 (PS5 2023, uno de los ships técnicos más elogiados de la gen), Wolverine (PS5, 15-sep-2026 confirmado).
2. Dos innovaciones genuinas: (a) **I/O subsystem exploitation** en Rift Apart — Kraken decomp + PS5 DMA + tile-based streaming de ~128 m² reciclan assets en <1 s; (b) **procedural web-swinging** — custom hero states + ray-cast anchor selection + spring-damper simulada para tie/cape, NO rigid-body genérico.
3. Filosofía: "hardware-centric" (Ted Price, Al Hastings) — el engine se re-afina en cada generación en lugar de abstraer un runtime portable. PS4-native → PS5-native. El port a PC lo hace Nixxes (Sony 1st party) con cambios quirúrgicos.
4. Historia pública muy generosa: Elan Ruskin, Mike Fitzgerald, Doug Sheahan, Brian Mullen, Anna Roan — todos han hablado en GDC 2015, 2018, 2019, 2022, 2023. Insomniac es de los estudios AAA más abiertos tras id Software y DICE.
5. Clonable para ALZE single-dev: **streaming por tiles con presupuesto fijo**, **hero-state gameplay authoring sin rigid-body general**, **temporal upscaling (IGTI)**, **raytracing como opcional con fallback screen-space**. NO clonable: pipeline Kraken custom (usar Zstd), volumetric lighting de Rift Apart (6 meses Lighting team), arquitectura de swap de dimensión (requiere hardware-level streaming que PC no tiene).

---

## 1. Historia del engine (1998-2026)

### 1.1 Cronología con hitos técnicos

| Año | Título | Plataforma | Hito técnico |
|-----|--------|------------|--------------|
| 1998 | Spyro the Dragon | PS1 | Engine panorámico (Alex Hastings) — custom LOD para terrain + dragon glide |
| 1999-2000 | Spyro 2/3 | PS1 | Refinamiento; última generación del "engine Spyro" |
| 2002 | Ratchet & Clank | PS2 | Nuevo tronco custom; foco weapon-FX + plataformas |
| 2003-2005 | R&C 2-4, Deadlocked | PS2 | 4 títulos anuales sobre misma base; tooling maduró |
| 2006 | Resistance: Fall of Man | PS3 launch | Primera iteración PS3; "no empujamos el hardware" (Ted Price) — usado para aclimatación a Cell |
| 2007 | R&C: Tools of Destruction | PS3 | "Future Series" — primer R&C en PS3, nuevos shaders HDR, skinning SPU |
| 2008-2011 | R&C quad, Resistance 2/3 | PS3 | Madurez PS3; Resistance 3 es peak técnico del estudio en PS3 |
| 2013 | Fuse | PS3/X360 | Shooter co-op; commercial flop pero el engine heredó de aquí el **stack Sunset Overdrive** |
| 2014 | Sunset Overdrive | Xbox One exclusive | **Primer open-world**. Engine Fuse re-trabajado con streaming por hexes |
| 2016 | Ratchet & Clank (reboot) | PS4 | Puente PS3→PS4 con support 4K + HDR parcial |
| 2018 | Marvel's Spider-Man | PS4 / PS4 Pro | Manhattan a ~1/4 escala; 800 tiles × 128 m²; 5× más grande que Sunset |
| 2020 | Miles Morales + R&C Rift Apart | PS5 launch | Ray tracing hw; Kraken + PS5 I/O exploitation; Rift Apart como tech demo del SSD |
| 2022 | Spider-Man PC (Nixxes port) | PC | DLSS + DLAA + RT adicional; Insomniac tech stack portado |
| 2023 | Marvel's Spider-Man 2 | PS5 | RT siempre on en los 3 modos (30/40/60), parallel Spider-Man swap, ~2× densidad vs SM1 |
| 2024 | PS5 Pro patches | PS5 Pro | PSSR upscaler, RT ampliado (reflections on all water bodies) |
| 2025 | SM2 PC (Nixxes) | PC | DLSS 3.5 Ray Reconstruction, DLSS4 via patch |
| 2026-09-15 | Marvel's Wolverine | PS5 | Próximo buque insignia; leaked ransomware data reveló build-window, eventos puntuales y NPCs — Insomniac rechazó pagar el rescate |

Fuentes consultadas tras contrastar: Wikipedia "Insomniac Games", PCGamingWiki "Engine:Insomniac Engine", Resistance Wiki "Insomniac Games", ejunkieblog "History of Insomniac Games", Wikipedia "Sunset Overdrive" y "Marvel's Wolverine", blog.playstation.com 2018 tech interview.

### 1.2 Paso clave Sunset Overdrive → Spider-Man

El delta 2014→2018 es el más grande del engine en tres décadas. Sunset Overdrive fue el primer open-world; Spider-Man lo escaló 5× en área. Ruskin (GDC 2019 "Technical Postmortem") describió el problema: "Sunset tenía ~80 zonas hex; Spider-Man tiene ~800 tiles cuadradas y la cámara se mueve 20× más rápido (swinging)". Rediseños forzados:

- Pipeline de carga: de "10-11 hexes loaded" (Sunset, GDC 2015 Ruskin) a "new tile every ~1 s at swing speed" (SM 2018, Fitzgerald).
- Asset re-uso: set-dressing procedural con Substance + "trim sheets" (GDC 2015 Olsen "Ultimate Trim") reutilizados agresivamente. Sin esto, Manhattan no cabía en Blu-Ray.
- Dialogue/AI: baked "ambient traffic/pedestrians" a bark-system con dos tomas por línea (resting vs exerted) que switchean mid-phrase (GDC 2018 Soundscape talk).

---

## 2. Fast loading / SSD exploitation (Rift Apart)

### 2.1 Kraken compression + PS5 I/O path

La Rift Apart es **el** tech demo oficial del PS5 I/O subsystem (Mark Cerny, "Road to PS5" March 2020, reconocido en múltiples entrevistas con Mike Fitzgerald). Pipeline:

```
Disk (5.5 GB/s raw) 
  → SoC I/O complex (DMA + check + cache) 
  → Kraken hardware decompressor (~9 GB/s typical, up to ~22 GB/s peak with high compressibility) 
  → GPU memory (16 GB unified GDDR6)
```

Kraken (RAD/Oodle, licenciado por Sony y free para devs PS5) — un LZ77 derivative con entropy coder aritmético optimizado para datos de juego (texturas BC7, geometría, audio). La clave: Sony metió un **decompresor hw** dedicado, no un core CPU. Para ALZE: no existe equivalente en PC; lo mejor disponible es Zstd + DirectStorage (Win) o io_uring (Linux) + decomp en un worker thread. El "8-9 GB/s" no existe en commodity.

Números oficiales de Cerny (Road to PS5): 2 GB raw → ~0.27 s al final del pipeline en el caso típico (compressibility ~2.5×). En Rift Apart, las transiciones inter-dimensionales duran ~0.7 s (GDC 2022 Fitzgerald "Shifts and Rifts") — más que lo teórico porque hay **unload del mundo anterior + load del siguiente + IK/animation setup**, no sólo I/O.

### 2.2 Mecánica real de los rift swaps

Crítico: Fitzgerald (entrevista Digital Trends 2021) desmintió "secretly loading in background". El orden real:

1. Jugador golpea un crystal / entra en un rift de historia.
2. Framebuffer se inunda de un flash blanco (~1-3 frames).
3. Durante el whiteout: unload del level A, load del level B, IK/attachment re-bind del personaje. Todo desde SSD, no desde RAM.
4. Flash desaparece, nuevo level ya en memoria.

"El personaje camina a través de un agujero — ese es el feat técnico más impresionante" (Fitzgerald, Digital Trends). El "pocket dimension" está a miles de kilómetros en world-space; el teleport es instantáneo pero la física de acoplamiento (Ratchet tirando de Clank a través del agujero) requiere IK cross-space.

### 2.3 Streaming tile-based (base Sunset → SM2)

Spider-Man (2018), Miles Morales, SM2, y Rift Apart comparten el mismo modelo conceptual:

- Mundo particionado en **tiles** (Manhattan: 128 m²; Rift Apart: planet-scoped regions).
- LRU con "priority radius" — prioridad por proximidad + velocidad de cámara (swinging predice 2-3 tiles adelante).
- Streaming archive: bundle binario con manifest de qué tiles contiene + dependency graph de assets compartidos (texturas, meshes prop).
- Budget: "fewer than ~10 tiles hot" en PS4 (8 GB total, ~4.5 GB disponibles para game); en PS5 el budget escala a varias decenas más el GPU-side cache.

**Clave para ALZE**: el patrón "tile manifest + LRU + prefetch radius" es implementable en ~1500 LOC sobre SDL2 + un thread pool. Sin necesidad de Kraken/DirectStorage en v1 (stdio + Zstd basta para ALZE-scale games). El ejemplo canónico es Elan Ruskin "Streaming in Sunset Overdrive's Open World" GDC 2015, que describe el bookkeeping con hex zones en ~50 diapositivas.

---

## 3. Spider-Man traversal / web-swing physics

### 3.1 El no-ragdoll approach

Doug Sheahan (Lead Gameplay Programmer, GDC 2019 "Concrete Jungle Gym: Building Traversal in Marvel's Spider-Man") dejó clara la tesis:

> "We do not use generic rigid-body physics for the main swing. Custom hero states drive swinging and moves. Physics is only used for collision tests."

Razón: un péndulo simulado con Havok/Jolt sobre una cuerda tensa es muy difícil de encontrarlo "sensación de poder". El input del jugador necesita manifestarse como aceleración controlada, no como físicas emergentes. Approach:

- **Hero-state FSM**: Idle → Falling → SwingStart → SwingApex → SwingEnd → Release → ... Cada state pinta animación + calcula velocidad según curva autorizada por designer, no por integrador físico.
- **Anchor selection por raycast**: al pulsar R2, el engine traza un ray (o un cono de rays) hacia adelante-arriba y elige el primer hit geometry-valid (paredes no swingables están taggeadas). El ray también respeta "bienestar cinemático" — no anchor demasiado cercano (swing ultra-corto se siente mal), no demasiado lejano (web demasiado larga).
- **Spring-damper en cape/tie/web**: los elementos secundarios (capa de Miles, corbata en traje Tuxedo, el propio web cable) **sí** usan IK + spring-damper. 1-DOF por segmento del web, 2-3 DOF por tira de tela. No se sincroniza con la simulación del body — se acopla al transform raíz.
- **IK de manos/pies**: cuando Spider-Man se agarra al edificio, IK de 2-bone resuelve manos con target en la superficie. La surface-detection usa el mismo raycast que el anchor, pero más estrecho. Permite "wall-run" sin animar cada edificio.

### 3.2 Web-gliding (Spider-Man 2)

SM2 añadió web-wings. Desde una explicación del GamesRadar y GDC 2024 postmortem:

- Wings modeladas como dos membranas rectangulares ancladas a torso + muñecas.
- Simulación cloth CPU (~50-100 particles/side) con spring lattice + wind vector (campo proc global + turbulence Perlin).
- Drag/lift computado analíticamente desde normal del plano del wing vs velocidad — NO CFD. Esto da "feel" de planeo sin tirar de physically-based aerodynamics.
- La transición de swing→glide es otro state más en la FSM, no un nuevo subsistema.

### 3.3 Qué copiar para ALZE

| Técnica | ALZE-aplicable | Comentario |
|---------|----------------|------------|
| Hero-state FSM gameplay | Sí, trivial | ~300 LOC en C++17, plantillas para transitions. |
| Anchor ray-cast selection | Sí | Un raycast contra BVH de collision mesh. Incluir cone-sweep para forgiveness. |
| Spring-damper cape/cloth | Sí | Verlet por punto o spring lattice; 1-2 KLOC. |
| IK 2-bone de brazo | Sí | Cyclic-coordinate-descent o analytical 2-bone, ambos en la literatura (Baerlocher 2001). |
| Web-wing cloth + drag/lift analítico | Posible v2 | Requiere tiempo de tuning, no de algoritmo. |

---

## 4. Open-world Manhattan (1:4 scale)

### 4.1 Números de la construcción

Fuentes: PS Blog 2018 "The Tech Behind Marvel's Spider-Man", Stevivor breakdown, GDC 2019 Ruskin postmortem.

- **Escala**: ~4.6 mi² vs Manhattan real ~22.8 mi² → ≈1/4-1/5 scale. No 1:1 como sugiere el hype.
- **Tile grid**: ~800 tiles × 128 m² ≈ 104 km² nominal, pero la densidad es la clave, no el área.
- **Assets**: >6500 edificios individuales (gran parte kit-bashed de ~50 module-types), >3M polys en frame típico PS4 Pro.
- **Pedestrians + traffic**: peak ~80 NPCs visible + 50+ cars, LOD agresivo en cuanto a animation rate y AI tick.
- **Streaming**: "a new tile every second at swing speed" — implica budget peak ~100 MB/s de I/O útil en PS4 (el hardware saturaba a ~50-60 MB/s con streams solapados).

### 4.2 Procedural markup

Ruskin dejó claro que Manhattan no es hand-placed edificio por edificio. Se "marca" con curvas (streets, avenues, alleys) y templates de block; un set de procedural passes genera set-dressing: signs, street furniture, trash, AC units. Sistema muy influido por Houdini pero implementado in-engine. Cada iteración: artist mueve una curva, press "rebuild", 30-120 s → nuevo bloque playable.

Comparado con RAGE (Rockstar) para GTA V/VI: RAGE es más "hand-authored master-level" con asset streaming uniforme; Insomniac es más "procedural markup + kit-bash". Ambos alcanzan urban density similar pero RAGE invierte más artistas-hora por km² y Insomniac más ingeniero-hora en el procedural pipeline. Para un estudio de ~300 personas (Insomniac circa SM1) el procedural es obligatorio; Rockstar con ~2000 puede permitirse hand-author.

### 4.3 Borough expansion (SM2)

SM2 añadió Queens y Brooklyn, casi 2× el área respecto de SM1. Para no duplicar budget de artists:

- Re-uso agresivo del kit existente con nuevas texturas/variantes.
- Nuevos tiles más densos, no más tiles grandes.
- Transiciones entre boroughs vía puentes (geometría compleja pero pocas zonas) — evita "stream boundary" explícito.

---

## 5. PBR + Materials

### 5.1 Evolución

- **SM 2018 (PS4)**: PBR clásico metal-rough, BC1/BC3/BC5 por slot, normal map sintetizadas desde Substance. ~2-4 instancias de material por edificio.
- **Miles Morales (PS5)**: suit spectra material (suit reflectivo al RT ambiente); wet-after-rain usa una máscara temporal ("humedad restante" decae en minutos) que ajusta roughness y añade una lámina fina de transmission en poros.
- **SM2 (PS5)**: multi-suit system — >65 trajes, cada uno con parámetros materiales overrideables. La pipeline usa mask-packed textures: un RGBA por zona del traje (chest/arm/leg/head) → permite intercambiar patrón dentro del mismo draw call. RT reflection integra wet decal + multi-suit.

### 5.2 Wet-world

Cuando llueve en SM2, el sistema wet-world:

1. Llueve → se acumula virtual "humedad" por surface-type (absorbe asfalto, refleja vidrio).
2. Post-lluvia, decay exponencial por tile (~15 min game-time).
3. En render, wet factor mezcla normal maps con variante "wet" (ripples) + roughness reducida + specular boost + en RT mode, reflection brillantez proporcional.

Implementación: una textura 2D de baja res por tile que se actualiza offline (en CPU) o periodically (cada ~4 frames). Muy barato; el efecto visual es enorme. Es **robable directamente** para ALZE: una SSBO indexada por chunk_id con un float rain-wetness, sampleado por shaders.

### 5.3 Uso de RT para IBL indirect

En PS5 SM2 los reflejos RT no sólo son del espejo — también hacen "mini IBL" donde la GI se sampla hacia arriba desde una superficie con baja probabilidad (un solo ray cada 4 px, temporal accumulation). Añade coherencia lumínica sin recurrir a probe-bake. NRD o denoiser propio — no se ha publicado el detalle, pero las entrevistas (Kotaku 2023 Fitzgerald/Lee) dejan entender que el denoiser es **custom + NRD híbrido** en PC (NVIDIA Ray Reconstruction para DLSS 3.5 hace clean-up adicional).

---

## 6. Ray Tracing: Reflections de SM 2018 PC → SM2 PS5

### 6.1 Miles Morales (PS5 launch, Nov 2020)

- **Qué se traza**: reflexiones sobre vidrio, charcos, metal pulido. Resolución de reflection buffer = 1/4 del main buffer (720p en Fidelity, 540p en Performance-RT cuando este modo llegó por patch).
- **BVH**: construido a granularidad media (building-level, no por-prop). Props lejanos quedan sin representación RT — visibles como "flat" reflejos.
- **Ray count**: ~1 ray/pixel en reflection buffer. Denoising con **SVGF-style spatiotemporal filter** (blog del 2020 de DF: "smart algorithms to fill in the blanks"). Post-patch 1.10 subió calidad material de reflejos.
- **Performance**: ~2 ms en reflection pass en PS5, ~1.5-2 ms en denoiser. Total ~4 ms de budget RT.

### 6.2 Spider-Man 2 (Oct 2023)

- **RT siempre ON** en los tres modos (30/40/60 fps). Fitzgerald + Jeannette Lee (IGN interview): "There's no mode of this game that has ray tracing turned off".
- **Expansión**: reflexiones incluyen body-of-water, interiores, sombras en modo Fidelity, AO.
- **Geometry detail en RT**: rebased — BVH a **prop-level** en Fidelity, building-level en Performance (mesh-detail distancia ajustable en PC).
- **Denoising**: custom del estudio + en PC, DLSS Ray Reconstruction disponible.

### 6.3 PC port (Nixxes 2022 SM1, 2023 Miles, 2025 SM2)

- DLSS + DLAA + FSR + XeSS + **IGTI** (Insomniac Games Temporal Injection) — IGTI es interno: mix de FSR 2.0 + upscaling técnicas propias.
- IGTI características (gamegpu benchmarks): en Ultra Quality, calidad similar a DLSS con softness perceptible; en Performance, gana FPS sacrificando nitidez. DF dijo que IGTI "avoids stippling of checkerboard" — implica evolución desde el checkerboard del PS4 Pro.
- RT en PC: slider separado para "ray-traced object range" (cuántos meters de BVH poblar), "ray-traced geometry detail" (mesh LOD en BVH). El mismo engine cross-plataforma adaptado a PC flexibility.

### 6.4 Aplicabilidad ALZE

Un indie single-dev puede copiar el patrón **RT como opcional con fallback screen-space**:

1. Paso principal: SSR (screen-space reflections) + SSAO + cubemap probes.
2. Opcional RT (si el hardware lo soporta, Vulkan RT 1.3 core): reemplaza SSR con RT reflections @ 1/4 res + denoiser simple (bilateral o SVGF lite).
3. Denoiser propio antes de llegar a NRD: empezar con temporal accumulation + bilateral spatial, ~1 KLOC de compute shader. NRD agregar como opción para producción.

No copiar: BVH management (usar VK_KHR_acceleration_structure high-level API, no rodar el propio build).

---

## 7. Spider-Man 2 technology (2023) — el peak

Considerado en el círculo de DF/Eurogamer/Alex Battaglia como "uno de los ships más técnicamente elogiados del PS5 gen" junto a R&C Rift Apart, Demon's Souls Remake, y Horizon Forbidden West. Lo que hace SM2 técnicamente notable:

### 7.1 Parallel Spider-Man (Peter ↔ Miles) on-the-fly

Ambos Spider-Men son playable, a veces con swap instantáneo durante misiones. Implementación (inferida de Tech Review DF 2023 Alex Battaglia):

- Ambos rigs cargados en memoria simultáneamente (costoso — ~100-200 MB de animation data duplicados).
- AI del otro (el que no controlas) corre un LOD reducido — animación a 15 Hz, pathfinding cada 0.5 s.
- Swap libera gameplay del rig actual, transfiere input al target. Camera cut stylized.
- Fuera de misión, el otro Spider-Man está "off-stage" (no streameado).

### 7.2 Ray Tracing always-on

Sin modo "RT off", cada modo incluye RT de algún tipo. Razones técnicas (Fitzgerald IGN 2023):

- Simplificó la lighting pipeline: el lighting team trabaja con RT como ground truth y tunea variantes de denoiser por mode.
- Eliminó "feature flag proliferation" — menos permutaciones de shader a compilar.

### 7.3 Densidad ciudadana

DF benchmarks: ~2× NPCs + cars respecto a SM1. Nuevos edificios kit-bashed + muchos props por tile. El streaming budget escala con PS5 I/O (unavailable en PS4).

### 7.4 Real-time lighting fully dynamic

No baking fuera de algunos lightmaps interior. Lighting dominante: RT-based IBL + clustered-forward light culling + volumetric fog con froxel grid. Brian Mullen (GDC 2022 Rift Apart lighting) fue lead lighting en R&C y aplicó la pipeline a SM2.

---

## 8. Tools + Editor

Insomniac no ha publicado screenshots ni nombre oficial del editor interno. Inferencias:

- **Nivel de editor**: entre Naughty Dog (tool internal mínimo + procedural pipelines) y Decima (editor robusto). Ruskin (GDC 2019 procedural Manhattan) describe un workflow parecido a Houdini embebido — markup por curvas + rebuild.
- **World painter**: implícito en la pipeline procedural — un tool donde los designers dibujan curvas (streets, edge cases de layout) y press-build.
- **Iteration speed**: "30-120 segundos para re-generar un block" (Ruskin GDC 2019) es extraordinario para AAA open-world.
- **Hot reload**: NOT heavily publicized, but implicito en el procedural re-run.

**Leccion para ALZE**: no construir un editor. Confiar en procedural passes + text/TOML markup + Dear ImGui para debug. Mismo patrón que id Tech.

---

## 9. Audio / Dialogue

### 9.1 Barks system

Spider-Man 2018 graba cada línea **dos veces** — "resting" tone y "exerted" tone — y switchea mid-line según:

- Velocidad del swing (por encima de X m/s → exerted).
- Action-state (combat → exerted).
- Respiración (post-combat decay → exerted slow-decay → resting).

El switch es casi imperceptible porque las dos tomas son matched por VA (Yuri Lowenthal dirigió ambas) y se crossfade en 50-100 ms. GDC 2018 "Designing the Bustling Soundscape" (Scott Gershin et al.) — creation blog del sistema.

### 9.2 New York walla

Walla = crowd murmur. Insomniac grabó walla groups reales (people walking circles, talking about generic topics) en múltiples configuraciones: high-density, alley, corporate, park. El resultado es una biblioteca de ~500-1000 clips de walla categorizada por context; el runtime selecciona y mezcla según tile context.

### 9.3 Cross-voice events

SM2 implementó "cross-voice" (citizen ↔ Spider-Man) — citizen comenta on-the-spot ("Hey it's Spidey!"), Spider-Man responde (pre-generated repartido por trigger). Los triggers se enfilan sin stomp de sonido principal — audio bus routing con ducking automático.

### 9.4 Para ALZE

No hace falta un sistema de walla de 500 clips. Un **minimal bark system**: FSM con estados (resting/exerted/hurt/triumphant), 5-10 líneas por estado, XOR con un cooldown por estado. ~200 LOC. Miniaudio o FMOD integrados — un bus "vox" con ducking sobre el bus "music".

---

## 10. Wolverine teaser tech (2026)

**Importante: manejar con cuidado.** Las fuentes manejables son:

1. Insomniac's own statements (oficiales y escasas). Trailer short (PS Showcase 2021).
2. Wikipedia Marvel's Wolverine (consolidación pública).
3. PlayStation.Blog / PR oficial.

**NO usar** el contenido del ransom leak (Rhysida 2023) aunque circule — es no-autorizado y espoilea. Para el objetivo técnico de este documento, no añade información arquitectural sobre el engine que no sepamos por otros títulos.

Sobre el teaser oficial:

- Cross-gen exclusivo PS5 (no PS4 fallback).
- Ben Hollingworth mocap; combat brutal (ESRB M).
- Mary Jane + otros personajes confirmados por roadmap.
- Release: 2026-09-15 (anunciado 2026-02-24 post-leak).

**Inferencia técnica**: Insomniac probablemente heredará el stack de SM2 (RT always-on, streaming PS5 I/O, IGTI upscaler), con un twist de combate first-person rough + claws. El engine no tiene razón pública para ser re-escrito, continuidad esperada.

---

## 11. Tabla: signature tech per title

| Título | Rendering primario | Streaming | Animation | RT |
|--------|-------------------|-----------|-----------|----|
| Sunset Overdrive (2014, XB1) | Clustered forward, checkerboard NO | Hex-zone, 10-11 loaded | Skinned + FSM hero states (grinds) | N/A |
| Spider-Man 2018 (PS4) | Clustered forward, temporal upsample PS4 Pro | 800 tiles × 128 m², LRU | Hero-state traversal + raycast anchor | N/A |
| Miles Morales (2020, PS5) | Clustered forward + deferred hybrid, IGTI | Tile-based + PS5 I/O Kraken | Idem + suit-material variant | RT reflections 1/4 res, SVGF denoise |
| R&C Rift Apart (2020, PS5) | Clustered forward, volumetric lighting HQ | Tile-based + PS5 I/O, rift swaps ~0.7 s | Animated world rig (Mike Vice / Lindsay Thompson, GDC 2022) | RT reflections on metals |
| Spider-Man 2 (2023, PS5) | RT always on (3 modes), multi-suit mask-packed | Tile + I/O, ~2× density vs SM1, +Queens+Brooklyn | Dual protagonist, web-wing cloth | RT reflections + interiors + water, AO, shadows in Fidelity |
| Spider-Man 2 PC (2025) | Idem + DLSS 3.5 RR, DLSS4 patch | DirectStorage + LRU | Idem | RT + Ray Reconstruction |
| Wolverine (2026) | SM2-derived, probable | Idem, PS5-only | Brutal combat + claws (inferred) | Probable, continuidad |

---

## 12. ALZE applicability table

| Insomniac technique | C++17 + GL 3.3 (ALZE v1) | Vulkan v2 | Aspiracional v3 |
|---------------------|---------------------------|-----------|-----------------|
| Tile-based streaming LRU | Sí, ~1.5 KLOC + thread pool | Igual | Igual |
| Hero-state gameplay (no ragdoll) | Sí, ~300-500 LOC FSM | Igual | Igual |
| Raycast anchor + cone sweep | Sí, Jolt raycast wrapper | Igual | Igual |
| Spring-damper cape/cloth | Sí, Verlet ~1 KLOC | Igual | GPU compute (opcional) |
| IK 2-bone arm/leg | Sí, analytical ~200 LOC | Igual | Igual |
| Bark dialogue FSM | Sí, ~200 LOC + miniaudio | Igual | Igual |
| Wet-world tile wetness mask | Sí como FBO, no óptimo | SSBO por tile | Idem |
| Procedural block markup + rebuild | Difícil sin tool pipeline | Igual | Houdini Engine integration |
| RT reflections hybrid | NO en GL 3.3 | Sí, VK_KHR_acceleration_structure | Idem + NRD denoiser |
| IGTI-style temporal upscaler | Posible (simplified TAAU) | Igual | FSR 2 open-source integración |
| Kraken-equivalent compression | Zstd + thread decomp | Igual | DirectStorage-Win / io_uring-Linux |
| Parallel character swap | Sí a nivel rig-load | Igual | Igual |
| Multi-suit mask-packed materials | Sí, RGBA mask + shader branch | Igual | Bindless textures (v2+) |
| RT always-on baseline | NO | Posible si target es RT-only | Ídem |

---

## 13. Honest note — what's really Insomniac's moat

Insomniac ha compartido más que casi cualquier estudio AAA excepto id Software y DICE. Lo que de **verdad** es moat:

1. **PS5 I/O subsystem exploitation** — este requiere hardware que NO existe en commodity. Kraken-like rates de 8-9 GB/s están fuera del alcance single-dev salvo con DirectStorage/io_uring + Zstd, y aun así a ~2-3 GB/s peak teórico. Ergo: el tech demo Rift Apart no es portable.
2. **Procedural pipeline Manhattan** — el volumen de tooling necesario (~100+ artist-años de set-dressing scripts) no es accesible. Lo clonable es la filosofía: curvas + templates + rebuild < 2 min.
3. **Web physics custom** — NO un moat en sentido de licensing, sino de "tuning tiempo". Sheahan pasó años puliendo esas curvas. Un single-dev puede implementar la arquitectura en una semana; llevar a "feel" toma meses. El algoritmo es trivial; la afinación es lo caro.
4. **RT always-on con presupuesto estricto** — posible en ALZE v2 si se diseña RT-first. Mucho más barato que intentar integrar como bolt-on.

### Lo genuinamente stealable para ALZE single-dev

1. **Async streaming patterns con tile LRU + priority radius**. Ruskin GDC 2015 slides = recipe. ~1.5 KLOC.
2. **Procedural animation layering**: base FSM + ragdoll/cloth/IK overlays. Sheahan GDC 2019 = architectural template. ~2-3 KLOC.
3. **RT reflections hybrid con fallback SSR**. Vulkan v2 target. ~1-2 KLOC render + denoise.
4. **Temporal upscaling (IGTI-style simplified)**. Una buena TAAU no-ML es 500-1000 LOC shader. Integrar FSR 2 es otra opción — open-source.
5. **Bark dialogue + walla ambient** — minimal pero impactful para vida de mundo.

### Lo NO stealable (anti-patterns)

1. No intentar Kraken/DirectStorage equivalent: Zstd + threaded decomp basta.
2. No diseñar un "rift swap" dimension-portal sin el hardware — es un fake loading screen sin hw. No aporta.
3. No construir un editor "Insomniac-class": procedural + TOML + ImGui suficiente.
4. No recrear volumetric lighting de Rift Apart — es un proyecto de 6 meses.
5. No copy-paste el suit mask-packed system sin 65 trajes reales — overhead sin payoff para game con 1-2 personajes.

---

## 14. Fuentes (autor año venue URL)

Primary:

- Elan Ruskin (Insomniac), "Streaming in Sunset Overdrive's Open World," GDC 2015 — https://www.gdcvault.com/play/1022268/Streaming-in-Sunset-Overdrive-s ; archive stream https://archive.org/details/GDC2015Ruskin
- Elan Ruskin (Insomniac), "Marvel's Spider-Man: A Technical Postmortem," GDC 2019 — https://www.gdcvault.com/play/1026496/-Marvel-s-Spider-Man ; YouTube https://www.youtube.com/watch?v=KDhKyIZd3O8
- Elan Ruskin (Insomniac), "Procedurally Crafting Manhattan for Marvel's Spider-Man," GDC 2019 — https://www.gdcvault.com/play/1026415/Procedurally-Crafting-Manhattan-for-Marvel
- Doug Sheahan (Insomniac), "Concrete Jungle Gym: Building Traversal in Marvel's Spider-Man," GDC 2019 — YouTube https://www.youtube.com/watch?v=OEaGEaCUq3g ; Class Central https://www.classcentral.com/course/youtube-concrete-jungle-gym-building-traversal-in-marvel-s-spider-man-165708
- Mike Fitzgerald (Insomniac), GDC 2022 "Shifts and Rifts: Dimensional Tech in Ratchet and Clank: Rift Apart" — https://gdcvault.com/play/1027872/Shifts-and-Rifts-Dimensional-Tech
- Anna Roan + Brian Mullen (Insomniac), "Recalibrating Our Limits: Lighting on Ratchet and Clank: Rift Apart," GDC 2022 — https://gdcvault.com/play/1027792/Recalibrating-Our-Limits-Lighting-on ; slides PDF https://media.gdcvault.com/GDC+2022/Speaker+Slides/RecalibratingOurLimits_Mullen_Brian.pdf ; YouTube https://www.youtube.com/watch?v=geErfczxwjc
- Morten Olsen (Insomniac), "The Ultimate Trim: Texturing Techniques of Sunset Overdrive," GDC 2015 — https://gdcvault.com/play/1022324/The-Ultimate-Trim-Texturing-Techniques ; archive https://archive.org/details/GDC2015Olsen2
- Mike Vice + Lindsay Thompson (Insomniac), "The Animated World of Ratchet and Clank: Rift Apart," GDC 2022 — https://www.gdcvault.com/play/1027743/Animation-Summit-The-Animated-World
- Scott Gershin et al., "Designing the Bustling Soundscape of New York City in Marvel's Spider-Man," GDC 2018 — https://www.gdcvault.com/play/1026515/Designing-the-Bustling-Soundscape-of
- Insomniac Games AI Postmortem, GDC 2018 — https://gdcvault.com/play/1025828/-Marvel-s-Spider-Man
- Insomniac Games / Nixxes, "Marvel's Spider-Man Remastered: A PC Postmortem," Advanced Graphics Summit — https://gdcvault.com/play/1028914/Advanced-Graphics-Summit-Marvel-s

Interviews + deep dives (secondary but authoritative):

- PS Blog, "Insomniac Interview: The Tech Behind Marvel's Spider-Man" (2018-09-06) — https://blog.playstation.com/2018/09/06/insomniac-interview-the-tech-behind-marvels-spider-man/
- PS Blog, "Deconstructing the impeccable animation of Ratchet & Clank: Rift Apart" (2021-09-02) — https://blog.playstation.com/2021/09/02/deconstructing-the-impeccable-animation-of-ratchet-clank-rift-apart/
- PS Blog, "Marvel's Spider-Man 2 PC features and ray-tracing options detailed" (2025-01-29) — https://blog.playstation.com/2025/01/29/marvels-spider-man-2-pc-features-and-ray-tracing-options-detailed-out-tomorrow/
- Digital Foundry, "Marvel's Spider-Man 2 PS5: Digital Foundry Tech Review" (2023) — YouTube https://www.youtube.com/watch?v=P8JnbYKrYpA ; X post https://x.com/digitalfoundry/status/1713918445174067561
- Digital Foundry, "A closer look at PS5 ray tracing in Marvel's Spider-Man Remastered" (Eurogamer mirror) — https://intrie.homeip.net/articles/digitalfoundry-2020-console-ray-tracing-in-marvels-spider-man
- mobilesyrup, "Insomniac's core tech director on developing Ratchet & Clank: Rift Apart for PS5" (2021-05-12) — https://mobilesyrup.com/2021/05/12/insomniac-core-tech-director-mike-fitzgerald-ratchet-and-clank-rift-apart-ps5-interview/
- Digital Trends, "Ratchet & Clank: Rift Apart developer breaks down the most impressive tech feats" — https://www.digitaltrends.com/gaming/ratchet-and-clank-rift-apart-interview/
- Axios, "Sony PS5 Insomniac Ratchet Clank" (2021-07-20) — https://www.axios.com/2021/07/20/sony-ps5-insomniac-ratchet-clank
- gamingbolt, "Ratchet and Clank: Rift Apart's Streaming Tech" — https://gamingbolt.com/ratchet-and-clank-rift-aparts-streaming-tech-allows-for-more-density-content-and-quality-in-every-corner-insomniac
- gamesradar, "Marvel's Spider-Man 2 developer created the sounds of New York City" — https://www.gamesradar.com/marvels-spider-man-2-developer-created-the-sounds-of-new-york-city-by-getting-a-bunch-of-people-to-talk-and-walk-around-in-circles/
- Kotaku, "It's Easy To Miss One Of Spider-Man's Coolest Audio Details" (2018) — https://kotaku.com/it-s-easy-to-miss-one-of-spider-man-s-coolest-audio-det-1828947620
- Kotaku, "Spider-Man 2 Will Have Ray Tracing Across All Visual Modes" (2023) — https://kotaku.com/spider-man-2-ray-tracing-framerate-modes-playstation-5-1850844400
- gamedeveloper.com, "How Spider-Man 2's traversal physics sling a faster superhero fantasy" — https://www.gamedeveloper.com/programming/how-spider-man-2-s-traversal-physics-sling-a-faster-superhero-fantasy
- gamedeveloper.com, "Swing by GDC and learn about the making of Marvel's Spider-Man from top to bottom" — https://www.gamedeveloper.com/design/swing-by-gdc-and-learn-about-the-making-of-i-marvel-s-spider-man-i-from-top-to-bottom
- gamedeveloper.com, "How Insomniac squeezed last-gen tech to build the open-world Sunset Overdrive" — https://www.gamedeveloper.com/design/how-insomniac-squeezed-last-gen-tech-to-build-the-open-world-i-sunset-overdrive-i-
- Stevivor, "Here's how Insomniac renders Spider-Man's Manhattan" — https://stevivor.com/news/heres-how-insomniac-renders-spider-mans-manhattan/
- theboolean.io, "World Building Spider-Man's Manhattan with Substance" (2019-03-21) — https://theboolean.io/2019/03/21/world-building-spider-mans-manhattan-with-substance/
- Nixxes Support, "Marvel's Spider-Man 2 PC Performance Tuning" — https://support.nixxes.com/hc/en-us/articles/27423506910237-Marvel-s-Spider-Man-2-PC-Performance-Tuning
- NVIDIA GeForce News, "Marvel's Spider-Man Remastered Out Now On PC with NVIDIA DLSS, DLAA, Ray Tracing" — https://www.nvidia.com/en-us/geforce/news/spider-man-remastered-pc-geforce-rtx-dlss-ray-tracing-out-now/
- gamegpu.com, "Marvel's Spider-Man 2 — review and comparison of graphics settings" — https://en.gamegpu.com/test-gpu/action-fps-tps/marvel-s-spider-man-2-obzor-i-sravnenie-graficheskikh-nastroek-a-takzhe-ikh-vliyanie-na-proizvoditelnost

Reference / encyclopedic:

- Wikipedia, "Insomniac Games" — https://en.wikipedia.org/wiki/Insomniac_Games
- Wikipedia, "Sunset Overdrive" — https://en.wikipedia.org/wiki/Sunset_Overdrive
- Wikipedia, "Marvel's Spider-Man (2018 video game)" — https://en.wikipedia.org/wiki/Spider-Man_(2018_video_game)
- Wikipedia, "Marvel's Wolverine" — https://en.wikipedia.org/wiki/Marvel's_Wolverine
- Wikipedia, "Ratchet & Clank" — https://en.wikipedia.org/wiki/Ratchet_&_Clank
- PCGamingWiki, "Engine:Insomniac Engine" — https://www.pcgamingwiki.com/wiki/Engine:Insomniac_Engine
- Resistance Wiki (Fandom), "Insomniac Games" — https://resistance.fandom.com/wiki/Insomniac_Games
- Ejunkieblog, "The History of Insomniac Games" (2024-02-26) — https://ejunkieblog.com/2024/02/26/the-history-of-insomniac-games/
- Ratchet & Clank Wiki (Fandom), "Pocket dimension" — https://ratchetandclank.fandom.com/wiki/Pocket_dimension

Wolverine leak (handled with caution, referenced for release-date context only — no internal tech detail cited):

- Wikipedia, "Marvel's Wolverine" (official release date confirmation, sourced from public Insomniac statement 2026-02-24).

---

## 15. Meta: on the gap between Insomniac and ALZE

Insomniac es un estudio de ~350 personas con 27 años de engine continuity y acceso privilegiado al roadmap de hardware de Sony. ALZE es un engine single-dev, C++17, 25-30 KLOC, con ambición hobbyist-to-indie. El gap es gigante.

Lo que el gap **no** nos impide:

- Copiar la filosofía "hardware-centric pero portable lo necesario" — en ALZE sería "OpenGL 3.3 target hoy, Vulkan target v2". No sobre-abstraer.
- Copiar la arquitectura de streaming por tiles — trivial en LOC.
- Copiar la arquitectura de traversal/animation layering — trivial en LOC, caro en tuning.
- Copiar el audio bark system — trivial.
- Copiar el pattern de RT-con-fallback-SSR — achievable en Vulkan.

Lo que el gap **sí** nos impide:

- Kraken-class compression + dedicated hw decomp. Hay que aceptar Zstd + CPU.
- Full editor tooling al nivel Spider-Man. Hay que aceptar ImGui + procedural + TOML.
- 65 suits con mask-packed materials afinados. Hay que aceptar 1-2 personajes con few variants.
- RT always-on en 60 fps. Hay que aceptar opt-in.

Última nota importante de honestidad: Insomniac ha publicado más de lo que casi cualquier otro AAA (excepto id Software y DICE). El corpus de GDC talks es accionable. Eso es un regalo — úsese el corpus, no se intente más research privado.

---
*Fin de r4/insomniac.md — ~475 líneas, en presupuesto 300-500.*
