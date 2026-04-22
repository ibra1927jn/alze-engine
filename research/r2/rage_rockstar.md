# RAGE (Rockstar Advanced Game Engine) — Investigación técnica (para ALZE Engine)

Fecha: 2026-04-21. Target: RAGE 1.0 → RAGE 9 (GTA VI, 2026).
Consumidor: ALZE Engine (C++17 no-RTTI/no-exceptions, SDL2 + OpenGL 3.3, equipo pequeño).
Convención por claim: (confirmed) = doc oficial o SIGGRAPH; (community-RE) = reverse-engineering de OpenIV/ScriptHook/GTAForums; (speculation) = inferencia.

## Overview

RAGE es el motor propietario de Rockstar Games, desarrollado por el **RAGE Technology Group** dentro de Rockstar San Diego (ex-Angel Studios, adquirido por Take-Two en 2002) (confirmed). El origen directo es el **Angel Game Engine (AGE)**, usado en Midtown Madness 2 (2000), rebautizado RAGE cuando Angel se convirtió en Rockstar San Diego (confirmed). El detonante fue la compra de Criterion por EA en 2004: RenderWare —el motor que Rockstar usaba en GTA III/VC/SA— quedó fuera de alcance, forzando a Rockstar a construir su propio stack (confirmed).

**Filosofía**: todo in-house, salvo dos middlewares clave: **Euphoria** (NaturalMotion, animación procedural) y **Bullet** (física rígida, en fork interno) (confirmed). Sin Unreal, sin Unity, sin CryEngine en ninguna iteración. No se licencia, no se distribuye, no existe "RAGE SDK" público. El motor está acoplado a los juegos: no hay separación Engine/Editor/Runtime como en UE; la codebase es un continuum.

**Juegos shippeados en RAGE** (confirmed, Wikipedia + Grokipedia):

| Año  | Juego | Estudio | Notas motor |
|------|-------|---------|-------------|
| 2006 | Rockstar Games Presents Table Tennis | R* San Diego | **Primer título RAGE** (Xbox 360). Nació como tech demo. |
| 2008 | GTA IV | R* North | Primera integración Euphoria; Bullet físicas. |
| 2008 | Midnight Club: Los Angeles | R* San Diego | RAGE para mundo urbano vehicular. |
| 2010 | Red Dead Redemption | R* San Diego | Mundo abierto continente-escala debut. |
| 2012 | Max Payne 3 | R* Studios | Primera build RAGE con **DirectX 11 + tessellation**; 720p paridad PS3/360. |
| 2013 | GTA V | R* North | Deferred + MRT, ~125 km² mapa + océano. |
| 2018 | Red Dead Redemption 2 | R* Games (all studios) | Volumetrics, PBR, Vulkan/DX12. |
| 2021 | GTA V Enhanced / GTA Online | R* North | DLSS (2021) + FSR (2022) añadidos. |
| 2026 | GTA VI | R* Games | RAGE 9, RTGI + strand hair + Kelvin waves (community-RE). |

**Por qué Rockstar domina el género open-world**: 20 años de iteración sobre la misma codebase, 3000+ devs, y control total sobre toolchain, streaming, animación, scripting, audio. No es un motor técnicamente superior a UE5/Decima/Frostbite — es un motor **co-diseñado con un solo caso de uso extremo** (ciudad/continente densa con sim persistente), y cada generación añade una vertical: GTA IV = Euphoria + física, RDR = continente, Max Payne 3 = DX11, GTA V = deferred/MRT/networking, RDR2 = volumétricos/PBR, GTA VI = RT + IA masiva.

## Architecture (what's known)

Debido a la ausencia de docs públicos (y la única fuente autorizada son ~5 talks SIGGRAPH + el leak de 2022 con ~3000 commits), la arquitectura se reconstruye de ScriptHook, OpenIV, RAGE Plugin Hook y el leak de Rockstar 2022 (community-RE):

**Capas compartidas entre juegos** (community-RE, desde leak + headers):
- `rage::` namespace raíz. Submódulos: `rage::math`, `rage::grcore` (render core), `rage::grmodel` (meshes), `rage::fwnet` (networking), `rage::atl` (allocators/templates).
- `fw*` prefix = framework genérico reutilizado entre juegos.
- `c*` prefix (ej. `CPed`, `CVehicle`) = código específico por juego (GTA-land vs RDR-land).

**Sistemas compartidos entre GTA V y RDR2** (community-RE): mission/dialogue manager, animation blending tree, scaleform UI, event-based ped AI. **Per-franchise**: rendering layer (RDR2 añade PBR + volumetrics que GTA V Legacy no tenía hasta 2025), traversal (horse vs car física distinta), economy/inventory.

**Streaming system** (community-RE + observación):
- **Grid de sectores** (tiles) con coordenadas mundo absolutas.
- **LOD tiers**: cada asset tiene 3–5 niveles (HD, LOD, SLOD1, SLOD2) swappeados por distancia (confirmed vía ymap/ytyp files).
- **Async asset load**: I/O thread dedicado lee de `.rpf` a memoria mapeada; main thread solo recibe handles.

**Mission scripting**: compilador propietario (community-RE). Formato `.ysc` (GTA V) / `.ysc` (RDR2 variante). Lenguaje interno conocido como **RAGE Script** o "RAGE Scripting Language" en la comunidad; sintaxis C-like reverseada (community-RE). **Natives** son funciones builtin exportadas al script (e.g. `PED::CREATE_PED`, `VEHICLE::SET_VEHICLE_ENGINE_ON`). La comunidad mantiene `native_db` con ~7000+ natives documentados por hash + firma (community-RE).

**Zone/sector world rep** (community-RE): el mapa se particiona en una grilla 2D (LODLight trees + IPL — Item PLacement files). Cada IPL referencia un `.ytyp` (type definition) que define qué props pueden spawnear. `.ymap` asocia coordenadas → assets. Streaming se dispara por cámara position + velocity cone.

## Rendering pipeline

**GTA V (2013) — deferred + MRT** (confirmed, Adrian Courrèges graphics study):
- Pipeline deferred clásico con **5 render targets MRT**: diffuse, normal, specular, irradiance, depth/stencil. HDR buffers en todo el pipeline.
- **Cascaded shadow maps (CSM)**: 4 cascadas en texturas 1024×4096 con resolución proporcional a cercanía. Dithering + depth-aware blur para suavizado.
- **SSAO**: half-res, pasada ruidosa + 2 pases depth-aware blur.
- **Reflections**: cubemaps reales 128×128 per-face convertidos a **dual-paraboloid maps** (2 texture lookups vs 6 del cubemap clásico, y sin seams).
- **Tone mapping**: filmic Uncharted-2 operator (no Reinhard).
- **Anti-aliasing**: FXAA + MSAA 2×/4× opcional; dithering es parte del pipeline (artefacto visible en LOD transitions alpha stippling).
- **LOD via alpha stippling**: transición dither pattern en vez de fade — barato en deferred porque evita el problema de transparencia.
- **Log-Z buffer** para precisión a distancia (ciudad de 125 km² exige range largo).

**RDR2 (2018) — deferred + forward híbrido** (confirmed, Imgeself graphics study + Fabian Bauer SIGGRAPH 2019):
- **6 G-Buffers**: albedo, normales, material props (metallic/roughness PBR), cavity, motion vectors, depth.
- **Tile-based deferred** path para environment maps lighting (similar a Forward+, pero ejecuta lights en tiles 16×16 o 32×32).
- **Atmosphere unified solution** (SIGGRAPH 2019, Fabian Bauer, Rockstar North): voxelization + raymarching con scattering/transmittance para viewport principal + reflection maps + sky irradiance probe grid. **Clouds vol umétricas** proyectan sombras reales sobre el terreno y colisionan con montañas distantes (percepción de escala). **Directabilidad artística**: física-real + palettes curvas por hora/clima.
- **Shadows**: 4 cascadas direccionales + texture arrays para spots/points. Truco: copiar shadow data de spot → face de point cubemap array (ahorra memoria).
- **SSR** (screen-space reflections) combinado con environment cubemaps para agua.
- **Planar reflections** solo para espejos explícitos.
- **TAA** (temporal AA) como AA primario; bloom en 7-mip target + multi-tap.
- **Baked interior cubemaps** streaming desde disco (millones de probes).
- **Vulkan + DX12** (confirmed, PC version nov 2019).

**GTA VI (2026) — RAGE 9** (community-RE, Digital Foundry Trailer 1+2 analysis):
- **RTGI (ray-traced global illumination)** fully, sin light sources rasterizados — DF observó que toda iluminación responde consistentemente a RT.
- **Ray-traced reflections** en superficies glossy (pintura de coches, charcos, sudor).
- **Strand-based hair** aplicado a todos los personajes, incluidos NPCs (impresionante — normalmente solo protagonistas).
- **FSR1 upscaling** de 1152p → 4K en trailer 2 (DF measurement).
- Target **30 FPS en PS5/XSX**, posible 60 FPS en PS5 Pro vía Full Path Tracing (community-RE, tekingame speculation).
- **AI hybrid CPU+GPU**: hasta ~8000 agentes concurrentes en escenas abiertas (community-RE, referencia a GDC talk R* feb 2026).

## Streaming + world representation

Mapas: **GTA V ~125 km²**, **RDR2 ~75 km² de terreno explorable** (confirmed, aunque estimaciones varían 50–75 km²). Ambos cargan **sin loading screens** una vez dentro del mundo — solo boot y fast travel.

**Mecanismos** (community-RE):
- **`.rpf` (RAGE Package File)** = archivo contenedor. Versiones 0→8 (Table Tennis → RDR2). Header 4 bytes versión + 4 bytes TOC size + 4 bytes entry count. TOC a 2048 bytes offset. Compresión **zlib sin header deflate**. V2+ soporta encryption (confirmed, gtamods.com/wiki/RPF_archive).
- **Entry types**: directory entries + file entries, binario alineado (16 bytes por entry en v0).
- **Streaming folder convention**: `x64/levels/gta5/_citye/...` organiza por región. `streaming/` en RDR2 para assets LOD-swappables.
- **Predictive loading**: el engine samplea la **cámara velocity + direction cone** y precarga tiles en un arco frontal más grande que el trasero (community-RE, observado en los patterns de I/O logs de mods como RPH).
- **Vehículo a alta velocidad + avión**: la altura y velocidad modifican el radio de stream. Volar en jet sobre Los Santos engancha el engine a LODs muy bajos para cobertura horizontal grande. Tren en RDR2 y caballo a galope han sido optimizados específicamente (community-RE).
- **Indoor↔outdoor seamless**: interior shells pre-baked (portales + probes). GTA V entra a tiendas sin transición; RDR2 saloons igual. Los interiores grandes (casino, Heist vaults) son "apartment portals": mini-mundos con coords offseteadas.
- **IPL/IMAP hierarchy**: "occupation zones" declaran qué assets son legítimos en qué chunk. LOD tree es un R-tree 2D o grid hash (community-RE).

## Euphoria + physics

**Euphoria** (NaturalMotion, confirmed):
- Tecnología: **Dynamic Motion Synthesis (DMS)**. No es animación pregrabada — es un **solver biomecánico en tiempo real**. Cada personaje modela: esqueleto + músculos + "motor nervous system" (PID controllers que intentan mantener pose/balance).
- Debuts en GTA IV (2008) — Rockstar fue **licensee #1** (confirmed, GameDeveloper article). Usado para: caídas, tumbadas, empujones, agarres a heridas, reacciones a disparos con protección de cabeza, recuperación de tropezones.
- **Intelligent ragdoll**: el personaje intenta proteger la cabeza, extender manos para amortiguar caída, agarrarse a asideros. No es ragdoll limp.
- Evolución por juego: **máximo drama en GTA IV** (todos se caen dramáticamente), **tonificado en GTA V** (más "gameplay-friendly", menos slapstick), **renacido en RDR2** (más matices, interacción con caballo/obstáculos naturales).
- **Problema crítico**: NaturalMotion fue comprada por Zynga en enero 2014 (USD 527M). En 2017 Zynga **cerró el licensing commercial** de Euphoria y Endorphin. Desde entonces **no se puede licenciar** para estudios nuevos (confirmed, PocketGamer.biz).

**Bullet fork** (confirmed, grandtheftwiki + GTA wiki):
- Rigid body dynamics: collisions, constraints, joints.
- Integrado en RAGE desde GTA IV. Fork interno — Rockstar no contribuye cambios upstream.
- **Vehicle physics**: GTA IV tenía deformación body panels más realista (bumper cae, capó se abolla); GTA V endureció chassis para gameplay. RDR2 volvió a deformación realista en carromatos y trenes.
- **Ragdoll blending**: transición animación → ragdoll vía IK blend. Cuando Euphoria termina (recovery exitoso o muerte), control vuelve a animaciones.

## AI + ambient life

**GTA V pedestrian system** (community-RE, gamedeveloper.com):
- NPCs agrupados por **archetypes** (forest ranger, musclehead, businessman, country bumpkin, wannabe celebrity, etc.) — no individuos con personalidad única.
- Cada archetype tiene un **pool de voice lines** por contexto (player cerca, player con arma, player chocando coche, smalltalk ambient). Cientos de líneas por archetype.
- **Spawning por zona**: businessmen en Rockford Hills, country bumpkins en Grapeseed/Blaine County. Ilusión de coherencia geográfica.
- **No hay líneas location-specific** — la misma voice line se reutiliza en toda la ciudad para abaratar producción.

**RDR2 ecology** (confirmed, Rockstar Newswire + British Ecological Society):
- **~200 especies animales** (mamíferos, aves, peces, reptiles, insectos).
- **Food chain simulada**: coyotes cazan presas pequeñas; buffalos los asustan; lobos huyen de grizzlies; vultures bajan a carroña. Carcasses decay y son scavenged.
- **Efecto jugador**: hunting excesivo reduce poblaciones locales; desbalance se propaga.
- **Schedules/routines**: NPCs en Saint Denis van a trabajo, bares, iglesia por hora del día. Dormitan en camas por la noche. Se guarnecen bajo techo si llueve.
- **Dialogue barks** con context filter: greeting, insult, response-to-greeting, post-encounter memory ("we met at the saloon yesterday"). **Cooldown** entre líneas para evitar spam.

## Mission scripting + tools

**RAGE Script** (community-RE):
- Lenguaje interno C-like. Compilado a bytecode `.ysc` (GTA V) / `.ysc` RDR2-variant.
- **VM stack-based** que ejecuta el bytecode en runtime. El main thread del game tick también driven por scripts (misiones, minigames, UI flows).
- **Natives** = funciones builtin en C++ del engine expuestas al script. Identificadas por **hash 64-bit** (ej. `0x8D9FD9F1`). La comunidad ha reverseado ~7000 natives (native_db maintained en GitHub).
- **ScriptHookV** (Alexander Blade): inyecta DLL en proceso GTA V, expone natives a C++/`ScriptHookVDotNet` (C#/VB). Permite mods client-side. Actualizado con cada patch. **Script Hook cierra GTA V al entrar Online** (mitigación anti-cheat client-side).

**Rockstar Editor** (confirmed, Rockstar support + GTA Wiki):
- Sistema de replay para GTA V. Modos: **Manual recording** (start/stop con tecla) y **Action Replay** (F1 salva últimos 30 segundos, ring buffer en memoria).
- Editor timeline clip-based: cortes, cámaras virtuales libres, edits de clip.
- **Determinismo**: Rockstar no ha documentado oficialmente si es snapshot-replay (grabar posiciones/estados cada frame) o re-simulation (semillas + inputs). Lo más probable por el tamaño de los replay files y la capacidad de Action Replay de mantener 30s en RAM es **snapshot-based** (positions, animations, states per-frame comprimido) más que re-simulation (speculation). RDR2 **no incluyó** Rockstar Editor al release — la sincronización determinista para un mundo tan grande debió ser demasiado cara.
- **Director Mode**: el mismo sistema permite spawnear personajes, escenarios, clima custom — herramienta de content creation de facto.

**Level authoring** (community-RE, GTA V 2022 leak):
- Pipeline basado en **Maya** para modeling/rigging/anim + export a formato Rockstar (`.ydr` drawable, `.ydd` drawable dictionary, `.ycd` clip dict, `.ybn` bounds).
- Mission scripts autorizados en un **IDE interno** con autocompletado de natives.
- **ymap/ytyp/ybn triangulation** es trabajo artista/diseñador sobre un world editor propietario (visto brevemente en leaked screenshots).

## Known performance characteristics

**RDR2 en PS4 base (2013 hardware, 2018 game)** = milagro de ingeniería (confirmed, múltiples DF analyses). Comparables generacionales:
- **Fox Engine** (Kojima/Konami, MGSV 2015): deferred + SSAO + global illumination bakeada, 1080p60 en PS4 base — **más liviano** que RDR2.
- **CryEngine** (Crysis 3 PS3): forward+ con volumétricos, 720p30.
- **Decima** (Horizon Zero Dawn 2017): checkerboard 4K en PS4 Pro, clouds volumétricos muy similares al approach de RDR2 (ambos influenced by Schneider/Vos GPU Pro 7 chapter) — Decima y RAGE son **tech peers** en atmospheric rendering.

**RDR2 en Xbox One X**: native 4K con todo activado (confirmed, DF). PS4 Pro: checkerboard 4K (DF notó aliasing edge). PS4 base: 1080p. Xbox One base: 864p upscaled, blurriest por TAA sobre resolución baja.

**GTA VI target 2026**: 30 FPS locked en PS5/XSX con RTGI permanente. PC 2027 + PS5 Pro tendrán modo Full Path Tracing (community-RE, techtimes).

## En qué es bueno

- **Escala open-world**: nadie iguala la densidad + tamaño + persistencia simultánea. RDR2 hace 75 km² con 200 especies + ecología funcional; GTA V hace 125 km² con 1000+ NPCs y tráfico dinámico.
- **Character fidelity**: Euphoria + PBR + strand hair (GTA VI) ponen a Rockstar por encima de casi cualquier otro open-world en animación corporal.
- **Seamless cutscene↔gameplay**: Rockstar es máster en no cortar. RDR2 transiciona de cinemática a control del jugador sin cargas perceptibles.
- **World density**: tiendas entrables, propiedades comprables, minijuegos (poker, five-finger filet, fishing) todos funcionales en el mismo world executable.
- **Replay/Editor**: Rockstar Editor como herramienta de community content que generó la machinima modern scene.

## En qué falla (conocido)

- **Onboarding engineer brutal**: codebase de 20 años, todo custom (memory allocators, template library `rage::atl`, string handling), sin docs públicas, sin comunidad fuera de Rockstar. Nuevos ingenieros tardan meses en ser productivos.
- **Ciclos de release largos**: GTA IV → V → VI = 5 + 13 años. Engine + game coupling significa que cada título es "nuevo motor" en práctica.
- **GTA Online netcode**: **P2P** (peer-to-peer) 32 jugadores (confirmed). Lobby-based matchmaking, sin servidor autoritativo dedicado para world state (cloud saves sí vive en Rockstar Social Club). Consecuencias: IPs expuestas, susceptibility a session hijack, lag cuando host cambia, cheat prevalence. Rockstar llevó años mitigando (BattleEye en GTA V Enhanced 2025).
- **Modding oficial ausente**: no hay SDK, no hay workshop. Mods toleran unofficially en single-player (OpenIV, ScriptHook); aggressive en Online (banned).
- **Física vehicular inconsistente generacionalmente**: GTA IV realistic → GTA V arcade → RDR2 back to sim. No hay continuidad — cada juego redecide.

## Qué podríamos copiar para ALZE Engine

Mecanismos concretos, feasible para equipo pequeño:

**1. Predictive async streaming basado en camera velocity cone**.
- Mantener un **radius base** para stream (ej. 200m) y un **forward cone** proporcional a velocidad cámara (`radius + k * |velocity|` en la dirección de avance).
- Thread I/O dedicado con queue prioritario: chunks dentro del cone prioridad alta, fuera del cone baja.
- Eviction LRU cuando memoria excede budget. Fácil de implementar en C++17 con `std::thread` + `std::condition_variable`.

**2. Grid de chunks jerárquico con LOD tiers**.
- Grid 2D fijo (ej. 128m × 128m per chunk). Cada chunk tiene 3 LODs: HD (<50m), MID (50–200m), LOD (200m+).
- Asset variants pre-autorizados por tier (decimation offline en asset pipeline).
- **Frustum + distance culling** por chunk (bounding boxes). Hierarchical: si un macro-chunk 512m está fuera, skip sus 16 sub-chunks enteros.

**3. Deterministic replay via snapshot ring buffer (Rockstar Editor style)**.
- Cada N frames (ej. cada 10) snapshot completo de: transform + velocity de entidades jugador-relevantes + inputs del frame.
- Ring buffer en memoria para últimos 30s (Action Replay equivalent). Dump a disco con zstd cuando el jugador guarda.
- Playback = restore snapshots + interpolación entre keys. No requiere re-simulation determinista (más barato, aceptable para visual replay).

**4. StumbleController (Euphoria-inspired, sin licenciar Euphoria)**.
- Al detectar impacto (fuerza > threshold): blend animación actual → **physics-driven ragdoll partial** solo en torso/brazos, mantener IK en piernas para recuperación.
- Target pose (balance reference) vía PID simple por joint: error = `target_angle - current_angle`, torque = `Kp*err + Kd*err_dot`.
- Transición back a animación cuando velocity angular de joints < threshold durante 0.3s.
- No es DMS real — es **animation + physics blend con balance heurístico**. 90% del efecto visual, 10% del costo de ingeniería.

**5. Dialogue bark system (tipo RDR2)**.
- Tabla de líneas: `{id, archetype, context_tags[], cooldown_sec, audio_clip}`.
- Context filters: player_distance, player_weapon, last_interaction, zone, time_of_day.
- NPC query: "dame una línea válida para mi archetype+contexto actual, excluyendo las que dije en últimos N sec".
- **Cooldown global por NPC** (no reutilizar línea en 60s) + **cooldown por bark type** (no spam del mismo insulto).
- Data-driven: las líneas viven en CSV/JSON, no en código.

**6. Content-addressable asset archive (tipo .rpf)**.
- Formato: header (magic + version + entry count + TOC offset) + TOC (name hash → {offset, compressed_size, uncompressed_size, flags}) + data blobs.
- Compresión **LZ4** (más rápido decompress que zlib, importante para streaming) o **zstd** level 3 para build final (mejor ratio, decompress aceptable).
- **Memory-mapped reads** (`mmap` POSIX, `MapViewOfFile` Win32) — el OS page-cachea automáticamente, reduces copies.
- Hash names (FNV-1a 64-bit) para lookup O(1); opcional string table al final para debug.
- Integridad: CRC32 per-entry. Encryption optional (XOR stream para builds release, sin pretender ser seguro).

## Qué NO copiar

- **No construir todo in-house**. Rockstar invirtió 20 años + 3000 devs + 2 franquicias #1 mundial para perfeccionar RAGE. ALZE no puede. Usa GLM, stb_image, OpenAL, ImGui, miniaudio, ENet — middlewares estables open-source.
- **No licenciar Euphoria**. No está disponible desde 2017 (Zynga cerró licensing tras comprar NaturalMotion). Implementar StumbleController simplificado como se describió arriba.
- **No intentar continent-scale sim** sin un equipo de 100+. RDR2's ecology de 200 especies requirió diseñadores dedicados por bioma, años de playtesting. Apuntar a ~10 especies con rule-based behaviors y un solo bioma primero.
- **No replicar P2P netcode como GTA Online**. El P2P ha sido un dolor continuo para Rockstar (IP leaks, cheats, desyncs). Si ALZE necesita multiplayer, usar **authoritative server** (gameservers dedicados, incluso minimal con ENet/geckoSv o LiteNetLib equivalente C++).
- **No coupling engine↔game**. RAGE se permite el lujo porque Rockstar solo shipea sus propios juegos. ALZE debe mantener separación Engine (código reutilizable) ↔ Game (código específico) desde día 1.
- **No scripting language propio**. RAGE Script es proprietary con coste de mantenimiento enorme. Usar **Lua** (~13K LOC, battle-tested) o **AngelScript** (C++-like syntax). Wasm si hot-reload y sandboxing estricto.

## Fuentes consultadas

- https://en.wikipedia.org/wiki/Rockstar_Advanced_Game_Engine
- https://rockstargames.fandom.com/wiki/Rockstar_Advanced_Game_Engine
- https://gta.fandom.com/wiki/Rockstar_Advanced_Game_Engine
- https://grokipedia.com/page/Rockstar_Advanced_Game_Engine
- https://www.adriancourreges.com/blog/2015/11/02/gta-v-graphics-study/
- https://www.adriancourreges.com/blog/2015/11/02/gta-v-graphics-study-part-2/
- https://imgeself.github.io/posts/2020-06-19-graphics-study-rdr2/
- https://advances.realtimerendering.com/s2019/ (Fabian Bauer, "Creating the Atmospheric World of Red Dead Redemption 2", SIGGRAPH 2019)
- https://gtamods.com/wiki/RPF_archive
- https://openiv.com/
- https://en.wikipedia.org/wiki/Euphoria_(software)
- https://en.wikipedia.org/wiki/NaturalMotion
- https://www.gamedeveloper.com/pc/product-i-grand-theft-auto-iv-i-using-naturalmotion-s-euphoria
- https://www.gamedeveloper.com/design/breaking-down-gta-v-s-pedestrian-dialogue-system-an-analysis-with-speculative-examples
- https://www.pocketgamer.biz/naturalmotion-ends-commercial-tech-licensing-business/
- https://www.rockstargames.com/reddeadredemption2/features/wildlife
- https://theconversation.com/red-dead-redemption-2-virtual-ecology-is-making-game-worlds-eerily-like-our-own-107068
- https://gtaforums.com/topic/994320-digitalfoundry-gta-vi-trailer-analysis/
- https://www.youtube.com/watch?v=9zVIy_La7nk (DF Trailer 2 Tech Breakdown)
- https://gtaforums.com/topic/972501-r-tech-presentations-gdc-siggraph-etc/
- https://en.wikipedia.org/wiki/Rockstar_Games_Presents_Table_Tennis
- https://www.techspot.com/review/537-max-payne-3-performance/
- https://cybernews.com/news/rockstar-games-gta-source-code-leaked/ (2022 leak context)
- http://www.dev-c.com/gtav/scripthookv/
- https://ragepluginhook.net/
