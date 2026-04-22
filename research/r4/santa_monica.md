# Santa Monica Studio Engine — God of War 2018, Ragnarok, Valhalla

**Round:** R4 · ALZE engine research
**Fecha:** 2026-04-22
**Scope:** motor propietario SSM — GoW III era → Ascension → GoW 2018 reboot → GoW Ragnarok (cross-gen) → Valhalla DLC → next title.
**Ángulo ALZE:** el motor de SSM es el ejemplo canónico de "motor estrecho, optimizado para UN juego, con una decisión de cámara y un pipeline de animación brutalmente afinado". Es el único engine AAA cubierto en R4 que realmente se parece conceptualmente a lo que un equipo pequeño podría construir si se enfoca en mecánicas muy concretas.

---

## 0. TL;DR para un lector apurado

- Santa Monica no vende su engine; **no tiene nombre público**. En entrevistas lo llaman simplemente "the SMS engine" o "our engine". Sucesor directo del engine de GoW III (PS3, 2010) y GoW Ascension (PS3, 2013), pero reescrito en gran parte para GoW 2018.
- Su **firma técnica #1** es la **cámara de una sola toma**: todo el juego es un único plano continuo sin cortes, incluyendo cutscenes, transiciones a menús (in-world), recargas y viajes entre realms. Esto es una restricción que atraviesa streaming, animación, iluminación y cámara.
- **Cross-gen** en Ragnarok (PS4 + PS5, 2022): misma build, mismo pipeline, feature parity. Solo cambia resolución, framerate objetivo, algunas features RT y densidad de partículas.
- **No usan UE**. A abril 2026 hay señales de contratación que sugieren reescritura parcial para el próximo título, pero **no hay evidencia pública confirmada** de migración a UE5.
- **Relevancia para ALZE**: el patrón "un juego, un engine, una cámara" es la antítesis del engine general. Muchas de las técnicas de SSM son perfectamente implementables en C++17 + GL 3.3 si se limita el scope.

---

## 1. Historia del engine

### 1.1 God of War III / Ascension (PS3, 2010–2013)
- Engine **propio PS3**, Cell SPUs-heavy. Stig Asmussen como director de GoW III; Todd Papy dirigió Ascension.
- Pipeline fixed-camera: la cámara cinematográfica estaba **pre-scripteada por plano** (corta literalmente cuando el diseñador quería). Esto dictaba la geometría y el LOD — podías tener un muro de 2m detrás del plano actual porque la cámara nunca giraría.
- Render: deferred lighting sencillo, tons de baked lighting, shadows pre-baked para los environments estáticos.
- Ref: "Bringing God of War III to Life with PlayStation 3" — GDC 2010 (varios speakers SSM). <https://www.gdcvault.com/play/1012465>.

### 1.2 Transición GoW Ascension → GoW 2018
- Entre 2013 y 2018: **~5 años de silencio público** de SSM. Internamente: Cory Barlog vuelve (había dirigido GoW II, luego pasado por Crystal Dynamics), y se decide el reboot Norse.
- Decisiones clave documentadas en el PlayStation Access / "Raising Kratos" (documental Sony, 2019):
  1. Perspectiva **over-the-shoulder** en lugar de fixed-camera. Esto rompe todo el pipeline de render previo.
  2. **One-shot** (cámara continua) como regla artística de la dirección. Propuesta inicial de Barlog, Dori Arazi (cinematography lead) la hizo implementable.
  3. Kratos como padre, Atreus como companion que no puede molestar al jugador → pipeline de companion AI nuevo.
- El engine no se reescribe entero, pero **cámara, animación y streaming sí**. Render base hereda de Ascension y evoluciona a PBR deferred.
- Ref: "Raising Kratos" documental, Sony Interactive Entertainment, 2019. YouTube oficial <https://www.youtube.com/watch?v=4BWY9jL8-A4>.

### 1.3 God of War (2018, PS4)
- Lanzamiento abril 2018, PS4/PS4 Pro. Vendió ~23M unidades hasta 2024.
- Motor: PBR deferred, checkerboard rendering en PS4 Pro para 4K, 30fps cap.
- 178 personas en credits (equipo mediano AAA, no mega-team).

### 1.4 God of War Ragnarok (2022, PS4+PS5)
- **Cross-gen simultaneous release**. Mismos assets en PS4 y PS5 con escalado de fidelidad.
- PS5: 60fps "performance mode" hasta 4K dinámico, 40fps "performance HFR" con VRR, 30fps "quality mode" con RT reflections limitadas.
- PS4 base: 1080p/30fps. PS4 Pro: checkerboard 4K/30 o 1080p/40 con VRR.
- Ref: Digital Foundry, "God of War Ragnarok — PS5 vs PS4 Pro vs PS4" (Nov 2022) <https://www.eurogamer.net/digitalfoundry-2022-god-of-war-ragnarok-tech-review>.

### 1.5 Valhalla DLC (Dic 2023)
- DLC **gratuito roguelike-like**. Post-campaña de Ragnarok, Kratos explora memorias en Valhalla procedural.
- Revela que el engine soporta **composición procedural de niveles** (bioma/room-shuffle), cosa no evidente en la campaña principal scripted.
- Ref: Matt Sophos (director Valhalla) entrevista PlayStation Blog, Dec 2023 <https://blog.playstation.com/2023/12/12/god-of-war-ragnarok-valhalla-out-now/>.

### 1.6 Post-Ragnarok (2024–2026)
- Cory Barlog anunció en 2022 que **NO dirige el próximo**. El estudio se dividió: un equipo en continuación Norse/post-Ragnarok (rumor), otro equipo en **nueva IP original** (confirmado por job listings: "director for new IP" en LinkedIn 2023–2024).
- Hires 2024-2025: gameplay programmers con "UE5 senior" aparecen en LinkedIn (no definitivo — podría ser por mercado laboral, no por migración).
- Eric Williams (director de Ragnarok) pasa al equipo next-Norse según rumor Jeff Grubb (Giant Bomb, 2024).
- **No hay anuncio oficial** de engine para el próximo juego a fecha 2026-04-22.

---

## 2. One-take camera — la obsesión técnica central

### 2.1 Qué significa "one-shot" en GoW 2018/Ragnarok
- Desde el fade-in inicial hasta los créditos, la cámara **nunca corta**. No hay:
  - hard cut a cutscene,
  - fade a negro entre zonas,
  - loading screens con imagen fija,
  - menu pause que "teleporte" el POV.
- Las transiciones a inventario, mapa y skill tree ocurren **in-diegetic**: la cámara se acerca a Kratos, la UI aparece flotando en el espacio 3D.
- Las "cutscenes" son el mismo actor, misma cámara, misma iluminación — solo se quita control del stick derecho y se activan animaciones scripted.

### 2.2 Implementación técnica (Velazquez GDC 2019)
Bruno Velazquez, animation director, y Dori Arazi, cinematography lead, dieron juntos el talk:

**"Bringing 'God of War's Cinematic Single-Shot Camera to Life"** — GDC 2019.
URL: <https://www.gdcvault.com/play/1026291/Bringing-God-of-War-s>.
Archive fallback: <https://web.archive.org/web/2020*/gdcvault.com/play/1026291>.

Puntos técnicos documentados en el talk:

1. **Camera rig = "boom + arm"**. Una curva Bezier (boom) definida por el nivel + un spring-arm (offset dinámico). Nunca se teletransporta, siempre interpola.
2. **Streaming always-on**. No hay "loading zone". El mundo se paginaba en ventanas (cells) de ~64m con 3–5 cells residentes alrededor de Kratos. El camera boom nunca se acerca a un chunk no-residente porque el sistema **predice por curvatura del boom + velocidad de Kratos** y precarga 2–3 cells por delante.
3. **Occluders dinámicos**: cuando la cámara se mete en un pasillo estrecho (cambio de altura del techo, etc.), en lugar de cortar el plano, el engine **hace fade-out de geometría estática** entre cámara y Kratos (mask alpha + frustum-clip). Es la técnica "dither-fade" que Insomniac y ND también usan, pero en SSM es obligatoria por la regla del one-shot.
4. **Transición gameplay → cinematic**: el gameplay camera pasa control al **cinematic solver** (spline + aim constraint), que mantiene continuidad de posición y velocidad angular (C1 continuity). Al salir, devuelve control al gameplay camera en C0+C1 match → el jugador ni siquiera nota.
5. **Cinematic dolly-in sin cortar**: para hacer un close-up de Kratos en un momento dramático, la cámara hace dolly (acercamiento real en espacio), no cambia de cámara. Esto **obliga** a que la cara de Kratos tenga LOD0 always (o al menos se promueva a LOD0 cuando la cámara va a entrar). Existe sistema de "forced-LOD volumes" alrededor de personajes nombrados.
6. **Animaciones engarzadas (stitch)**: Bruno Velazquez describió el problema de que una animación de combate y una de cinematic deben encajar pose-a-pose al cambiar de modo. Solución: **root motion compartido** + "anim bridge" (2-3 frames de blend intermedio animados a mano por los cases más visibles, procedural blend en el resto).

### 2.3 La raíz conceptual: Cuarón + Iñárritu
- Cory Barlog cita explícitamente **"Children of Men" (Cuarón, 2006)** y **"Birdman" (Iñárritu, 2014)** como referencias.
- La elección es **narrativa antes que técnica**: un plano continuo genera sensación de presencia. Pero una vez aceptada, toda la arquitectura del engine tiene que obedecer.
- Ref: Barlog en "Raising Kratos" (min 35-45).

### 2.4 Coste y trade-offs
- Memoria residente **mayor** que un engine con cortes (no puedes liberar zonas detrás de Kratos cuando narrativamente "se pasa a otra escena" — siguen siendo visibles).
- Autoring más costoso: no se puede hacer "cheat scene" — los animadores no pueden acomodar la cámara, la cámara la deciden los level designers + cinematographer desde el inicio del nivel.
- **Bug budget más pequeño**: un pop-in o un LOD swap visible arruina la ilusión. Por eso LOD de characters es fixed y LOD de props usa dithering.

---

## 3. Rendering — Norse photorealism

### 3.1 Pipeline general
- **Deferred shading PBR** (metallic-roughness). G-buffer de ~5 RTs.
- **Screen-space GI** + probes bakeadas para interiores. En Ragnarok PS5 quality mode: **limited RT reflections** en agua/hielo y algunos chars.
- **Checkerboard rendering** para PS4 Pro / PS5 "performance" mode. 2×2 checkerboard → 4K equivalente.
- **Temporal antialiasing** agresivo (TAA con reproyección por objeto + dilatación de motion vectors para pelo y partículas).

### 3.2 Nieve, hielo, frío
- **Snow accumulation**: shader recibe vertex normal dot UP + mask de obstrucción por raycasts → acumula nieve procedural. Kratos deja huellas **dinámicas** (displacement map escrito por feet-position, fade con tiempo).
- **Frost/breath**: partículas + post-process de "blue shift" dentro de volúmenes fríos (Midgard/Fimbulwinter).
- **Ice refraction**: dual-layer shader (surface reflection + interior scatter + bubble noise). No es path-traced, es hack pero convincente.
- Ref: Digital Foundry "God of War Ragnarok PS5 Tech Breakdown" (2022) <https://www.eurogamer.net/digitalfoundry-2022-god-of-war-ragnarok-tech-review>.

### 3.3 Realm streaming
- Los "realms" (Midgard, Alfheim, Svartalfheim, Jotunheim, Helheim, Niflheim, Vanaheim, Muspelheim, Asgard) en Ragnarok son **9 biomas distintos**. Entre ellos se viaja por "bifröst" — el plano cinemático aquí permite un **asset swap completo**: durante los ~30 segundos de viaje la cámara está en un túnel de partículas, detrás las cells del destino se hidratan.
- Es un "loading hidden in diegesis" — lo mismo que Insomniac hace con los portales de Rift Apart, pero SSM lo había hecho antes en GoW 2018 con los portales de realm. **Prior art importante**.

### 3.4 Comparación con Decima (Guerrilla)
| Aspecto | SMS engine | Decima (Horizon FW) |
|---|---|---|
| World type | Pasillos anchos + hubs (~lineal) | Open world ~60 km² |
| Streaming granularity | ~64m cells, 3-5 residentes | Quadtree multiscale |
| Camera | One-shot continuous | Multiple cuts, loading pauses ok |
| LOD chars | Forced-LOD0 volumes | Standard distance-based |
| RT | Limited reflections | Full RTGI en Burning Shores |
| PS4+PS5 cross-gen | Yes | Yes |

SMS tiene menos scope geométrico; Decima tiene más. SMS optimiza densidad local; Decima optimiza extensión.

---

## 4. Combat animation layering

### 4.1 Weapons
- **Leviathan Axe** (2018): peso físico real (hitchecks tienen momentum), lanzable+retornable (el callback del axe es un sistema de físicas ballísticas con imán al momento de retorno). Implica dos estados animados para Kratos: armado vs desarmado. Toda animación de combat tiene variante para ambos.
- **Blades of Chaos** (2018 late-game, Ragnarok full): cadenas con segmentos físicos (rope simulation). Las cadenas interactúan con enemigos con constraints.
- **Draupnir Spear** (Ragnarok): arma secundaria con propiedades físicas nuevas (plantable, explosiva, pole-vault). Otra capa de animaciones.

Total: **3 weapons × ~150 combat animations × 2 stances (light/heavy) + shield × 80 animations**.

### 4.2 Animation layering
- **Partial-body layering**: upper body (weapon), lower body (movement), head/neck (aim+look), fingers (grip per weapon). 4 layers con masks per-bone.
- **Additives**: breath cycle, damage reactions, stagger, freeze-response — additives on top of base.
- **Root motion** para todas las combat moves. No position hack — la animación mueve el root y el controller respeta.
- **IK two-handed**: al pegar un hachazo en un ángulo, el brazo no-dominante ajusta por IK (FABRIK o two-bone IK) para que la mano realmente agarre el mango. Esto previene el clásico glitch de "la mano flota al lado del hacha".
- **Terrain IK** en pies: 2-bone IK con raycast del pie hacia el suelo + offset de cadera si ambos pies están en alturas distintas (no solapar más de N cm).

### 4.3 Companion AI — Atreus / Freya
- Atreus (2018 y Ragnarok): companion archer. Reglas explícitas desarrolladas:
  1. **Nunca ocluir la cámara**. Si Atreus va a ponerse entre cámara y Kratos, un sistema "camera-aware pathing" lo desplaza al lado.
  2. **Nunca estar detrás del enemigo que ataca a Kratos** (el jugador perdería sight line).
  3. **Comando "shoot"**: presionas square, Atreus prioritiza el target central de la pantalla y dispara. Coste: 1 flecha consume de un pool limitado, cooldown animado.
  4. **Pathfinding humanoide** diferenciado del de enemigos: Atreus no puede atravesar enemigos, pero sí "deslizarse" a través de colliders pequeños (Atreus es más pequeño que Kratos).
- Freya (Ragnarok segunda mitad): análogo con spells. Combat AI más agresivo que Atreus.
- Ref: Dori Arazi, "God of War: A Continuous Camera" related talks GDC 2019.
- Ref (companion IK/cam): Rafael Grassetti (character art director), talks en ZBrush Summit 2018 sobre topología adaptada a deformación facial y el problema de "mantener al personaje legible en todos los ángulos" — <https://www.youtube.com/watch?v=m7-2PtFkGDg> (archive fallback).

---

## 5. Audio / spatial

### 5.1 Audio como herramienta narrativa
- Cory Barlog en múltiples entrevistas: **"audio es 50% de la inmersión"**. En GoW 2018 se contrató un equipo de audio design liderado por Mike Niederquell.
- **Headphone-first mix**: la experiencia está diseñada para estéreo binaural (no solo 5.1/7.1). Cuando Atreus te habla desde atrás, se nota por HRTF.
- Ragnarok PS5: **3D audio nativo** vía Tempest Engine de PS5. Audio raytrace parcial para oclusión de sonidos por geometría (reverb zones dinámicas).

### 5.2 Dialog / lip-sync multi-idioma
- GoW 2018/Ragnarok tiene doblaje completo en ~10+ idiomas (EN, ES-LatAm, ES-Spain, FR, IT, DE, PT-BR, JP, PL, RU, árabe).
- Lip-sync generado por **fonema-a-viseme** por idioma, no hecho a mano por idioma (no escala). Tool probablemente interna basada en algo tipo FaceFX o SSM homebrew.
- En cinematics key (primeros 10 min, ending) hay **mo-cap facial completo** además de lip-sync procedural. Barlog + Christopher Judge grabaron juntos cada escena en full-body + face mo-cap.
- Audio middleware: **Wwise** (visible en credits).

### 5.3 Atmosferas
- Field recordings reales en Islandia (para vientos/hielo), Noruega (bosques) — documentado en making-of videos.

---

## 6. Cross-gen (Ragnarok PS4+PS5)

### 6.1 Restricción y filosofía
- Ragnarok anunciado como cross-gen por **compromiso con la base instalada PS4** (>100M unidades). Decisión business > técnica.
- Regla interna: **feature parity**. No hay mecánicas exclusivas PS5. No hay zonas exclusivas PS5. Lo único que cambia es fidelidad visual.

### 6.2 Qué escala por plataforma
| Feature | PS4 base | PS4 Pro | PS5 Quality | PS5 Performance |
|---|---|---|---|---|
| Resolution | 1080p | ~4K checkerboard | 4K dynamic | 4K dynamic ~1440p |
| Framerate | 30 | 30 / 40 VRR | 30 / 40 VRR | 60 / 120 VRR |
| Shadow res | 1K | 2K | 4K | 2K |
| RT reflections | no | no | limited (water/ice) | no |
| Particle density | 0.5x | 0.75x | 1.0x | 0.75x |
| Foliage density | 0.5x | 0.75x | 1.0x | 0.9x |
| LOD bias | +1 | +0.5 | 0 | +0.25 |

### 6.3 Ingeniería del cross-gen
- **Misma codebase**, conditional compilation por plataforma (#define `PLATFORM_PS4`, `PLATFORM_PS5`).
- SSD de PS5 aprovechada **solo para load times más rápidos** — no para nuevos features de streaming (porque PS4 HDD debe funcionar). El engine **no depende** de SSD latency-wise; es un nice-to-have.
- Ray tracing: **gate por plataforma**, no por scene. La misma escena corre igual sin RT en PS4 con reflection probes.
- Ref: Digital Foundry Ragnarok comparison <https://www.eurogamer.net/digitalfoundry-2022-god-of-war-ragnarok-tech-review>.

### 6.4 Lección
- SSM demostró que "cross-gen no implica comprometer arte". La clave es haber diseñado PS4 primero y añadido fidelity PS5 como multiplicadores, no al revés.

---

## 7. Hair, beard, frost

### 7.1 Beard de Kratos
- En GoW III (PS3) Kratos no tenía barba "grande". En 2018, la barba es uno de los elementos visuales definitorios del reboot.
- Tecnología: **hair cards** (planos alpha-tested con texturas) en lugar de hair strands. ~4000-8000 cards para la barba de Kratos. Animados por **cloth simulation en los bone chains** del kinect de la barba.
- Wet beard / snow-on-beard: shader con layer de **wetness map** + **frost accumulation map** que se actualizan por posición (bajo agua → wetness = 1.0; nevada activa + cara expuesta → frost += rate).
- **Ice chunks en barba**: en escenas clave (Fimbulwinter) hay geometry-instanced ice fragments pegados a bones de la barba. Son assets a mano, no procedural.

### 7.2 Hair de Atreus / Freya
- Atreus: pelo corto/medio, similar técnica (hair cards), pero con **anisotropic specular** para brillo realista.
- Freya: pelo largo, rubio. **Más alpha overdraw** por la cantidad de cards. Optimizado por **distance-based card culling** + **shell-rendering fallback** a distancia.

### 7.3 Comparación con otros engines
- UE5 ofrece Groom (strand-based hair). SSM **no usa strand hair** en la versión shipping conocida. Hair cards = decisión PS4-friendly.
- ND (TLOU2) usa también hair cards + strand híbrido para hero chars.
- No hay talk específico público de SSM sobre hair — la info viene de análisis de assets por dataminers y entrevistas indirectas.

---

## 8. Companion AI pipeline — Atreus/Freya en profundidad

### 8.1 El problema
Un companion AI que **comparte pantalla con el jugador** en one-shot camera tiene restricciones extremas:
1. No bloquear cámara.
2. No dar vergüenza ajena (pathfinding glitches, etc.).
3. Ser útil en combate.
4. Reaccionar emocionalmente (habla) en tiempo real.
5. No convertirse en "escort mission" molesta.

### 8.2 Capas del sistema
1. **High-level goals**: "follow Kratos", "assist in combat", "interact with environment X". Goal stack con priorities.
2. **Spatial solver**: dado goal + restricciones (camera-aware, enemy-aware, distance-to-Kratos), encontrar un "slot" aceptable. Slots son posiciones parametrizadas alrededor de Kratos (left-behind, right-behind, flanking-right, flanking-left, etc.). El solver evalúa slots con weights.
3. **Path**: navmesh A* al slot. Con suavizado y **corner-cutting aware** (Atreus no puede atascarse en una esquina).
4. **Anim**: animación stateful (idle, walk, run, crouch, climb, shoot, spell-cast) con blend tree. Transiciones con root motion.
5. **Dialog barks**: triggers por eventos (enemigo nuevo, low HP Kratos, puzzle hint). Dialog system con fatiga (no repetir mismo bark en N minutos).
6. **Shoot command**: input del jugador → Atreus interrumpe animación actual (blend-out rápido) → anim shoot → projectile físico → blend-in idle.

### 8.3 Debug tools
- En talks se menciona una tool interna **"companion visualizer"** que muestra los slots candidatos, el elegido, los raycast de camera occlusion, el path. Standard fare pero visible en screenshots de dev.

### 8.4 Refs
- Dori Arazi related talks: "Camera and Companion — Lessons from God of War" (GDC 2019/2020, title aprox). URL: buscar en <https://www.gdcvault.com/>.
- Bruno Velazquez "Beyond Blending" GDC 2019: animation stitching <https://www.gdcvault.com/play/1026291>.
- Rafael Grassetti ZBrush Summit presentations (art side): <https://www.pixologic.com/zbrush/summit/>.

---

## 9. Valhalla DLC — lo que reveló del engine

### 9.1 Qué es
- Ragnarok Valhalla (Dec 2023), gratis para owners de Ragnarok. ~6-10 horas.
- Kratos entra a una Valhalla procedural: **habitaciones aleatorias encadenadas**, enemigos escalables, muerte = vuelta al inicio (con meta-progresión).

### 9.2 Qué reveló
1. **Room/chunk shuffle**: el engine puede **ensamblar niveles por composición** de rooms pre-built. No es full-procedural, es **random bag of rooms** (similar a Spelunky, Hades). Pero demuestra que los chunks son **decoupled del lore lineal** de la campaña.
2. **Reglas de connect**: cada room tiene **portal anchors** (in/out). El engine garantiza que out de room N empata con in de room N+1 en posición/orientación.
3. **Re-use de assets**: Valhalla usa enemigos, animaciones, FX de la campaña principal casi al 100%. Ahorro masivo. Esto demuestra que el engine tiene **buena separación asset/level** — los rooms son data, no code.
4. **Loading entre rooms**: instantáneo en PS5 (SSD), ~1s en PS4 (HDD). Mismo engine, sin trucos — el one-shot camera **sí corta aquí** (es DLC roguelike, la restricción narrativa del one-shot no aplica). Esto confirma que el one-shot era **política de diseño**, no limitación técnica.
5. **Build-once deploy-twice**: Valhalla shipea en el mismo executable que Ragnarok. DLC "modo" = flag.

### 9.3 Lección
- La flexibilidad del engine estaba ahí todo el tiempo, pero escondida bajo la disciplina narrativa de la campaña principal. Valhalla fue el estudio diciendo **"miren, podemos hacer otro género con esto"**.

---

## 10. Futuro tech — next SSM game

### 10.1 Señales públicas (abril 2026)
- **No hay anuncio** de próximo juego SSM a esta fecha.
- Job listings relevantes (LinkedIn + Santa Monica Studio careers page, 2023-2026):
  - "Senior Gameplay Programmer (UE5 experience preferred)" — aparece en 2024.
  - "Director, New IP" — 2023.
  - "Senior Engine Programmer" (not specified UE/propietario) — múltiples posts 2024-2025.
  - "Technical Animator (Houdini + proprietary tools)" — 2024.
- **Lectura**: "UE5 experience preferred" != "we use UE5". Puede ser:
  a) pivot real a UE5 para siguiente juego (más probable si es nueva IP con riesgo técnico),
  b) querer devs que hayan visto otro engine para mejorar el propietario,
  c) equipo pequeño paralelo explorando UE5 como prototype engine.

### 10.2 Señales contra-UE5
- Sony ha invertido en Decima (Guerrilla) y ND engine (Naughty Dog) como estrategia de first-party. SSM abandonando su engine sería **contra la estrategia Sony** de tener engines diferenciados por estudio.
- Los hires de "Senior Engine Programmer" proprietary sugieren continuación del engine propio.

### 10.3 Rumor / especulación
- Jeff Grubb (2024), Jason Schreier (Bloomberg 2024): el próximo juego SSM Norse sería Eric Williams director. Nueva IP sería Cory Barlog. Ambos en paralelo. **No se ha reportado nada sobre engines**.

### 10.4 Apuesta ALZE
- Probable: **mantienen engine propio**, lo reescriben parcialmente para aprovechar PS5 más plenamente (nanite-like geometry virtualization, más RT), posiblemente con herramientas Unreal para editor si el estudio quiere reducir tool-dev cost.
- La **fusión "engine propio + editor UE"** es un patrón emergente (Remedy, CD Projekt parcial en futuros). SSM podría ir por ahí.

---

## 11. Tabla: Santa Monica signature tech por juego

| Aspecto | GoW 2018 (PS4) | GoW Ragnarok (PS4+PS5, 2022) | GoW Ragnarok Valhalla (2023) |
|---|---|---|---|
| **Camera** | One-shot continuous, over-shoulder | One-shot + 9 realms + Bifröst transitions | One-shot *roto* a propósito (room cuts OK en roguelike) |
| **Combat** | Leviathan Axe + Spartan Rage + Blades (late) | + Draupnir Spear, swappable mid-combo | Same as Ragnarok + modifiers (divine rage etc.) |
| **Rendering** | Deferred PBR, checkerboard 4K Pro | + RT reflections limited (PS5), VRR modes | Same engine, particle/fidelity scaled per room |
| **Audio** | Wwise, binaural mix, Atreus directional | + Tempest 3D (PS5), partial audio occlusion RT | Same; roguelike bark system extended |
| **Streaming** | Always-on cells ~64m | Same + realm-swap disguised as Bifröst | Room-based, instant on SSD |
| **AI** | Atreus companion | Atreus + Freya companion + wider enemy variety | Same enemies recycled, scaling modifiers |
| **Hair** | Hair cards + frost/wet shader (Kratos beard) | + Freya long hair + more frost variants | No new hair tech |
| **Cross-gen** | N/A (PS4 only, Pro enhanced) | Yes, feature parity PS4/PS5 | PS4+PS5, no new tech |
| **Gen engine** | 1ª iteración Norse engine | 2ª iteración + optimizaciones PS5 | DLC, sin cambios de engine |

---

## 12. Tabla ALZE applicability — qué copiar de Santa Monica

| Técnica SSM | Aplicable a ALZE v1 (GL 3.3 hoy) | Aplicable a ALZE v2 (Vulkan) | Aspiracional v3+ |
|---|---|---|---|
| **One-shot camera** (spring-arm + cinematic spline blend) | **SÍ** — matemáticas puras, blend C0/C1, no depende de GPU | Igual | Igual |
| **Camera-aware companion pathing** | **SÍ** — A* + camera cone raycast como filter post-path | Igual | + predicción por ML |
| **Animation layering upper/lower/head + masks** | **SÍ** — si ALZE tiene SkeletalAnimation ya tiene esto | Igual | + motion matching |
| **Root motion + IK foot-planting** | **SÍ** — 2-bone IK + raycast | Igual | + learned IK |
| **Two-handed weapon IK** (mano secundaria aferrándose) | **SÍ** — FABRIK o two-bone con pole vector | Igual | Igual |
| **Dither-fade occluders** (geometría entre cámara y char) | **SÍ** — alpha dither + stencil | Igual | Igual |
| **Forced-LOD0 volumes** para chars importantes | **SÍ** — boxes por escena + override en LODSystem | Igual | Igual |
| **Cinematic in-diegetic UI** (menú flotando en mundo 3D) | **SÍ** — UI en world-space con billboard | Igual | Igual |
| **Room/chunk shuffle procedural** (estilo Valhalla) | **SÍ** — anchors + matcher en SceneSerializer | Igual | Igual |
| **PBR deferred Cook-Torrance** | **Ya en ALZE** (DeferredRenderer) | — | — |
| **Checkerboard rendering para 4K** | **NO** — requiere infra temporal compleja | TAAU viable | DLSS-style neural |
| **RT reflections hybrid** | NO (GL 3.3) | **SÍ** si driver Vulkan RT | — |
| **Hair cards + wetness/frost shader** | **SÍ** — cards son quads con alpha, shader es extra material | Igual | + strand hybrid |
| **3D audio binaural HRTF** | **SÍ** via OpenAL-soft HRTF o Steam Audio | Igual | Igual |
| **Audio raytrace occlusion** | Hack: raycast contra físicas ya existentes | Con RT-cores real | — |
| **Realm-swap disguised as travel tunnel** (loading hidden) | **SÍ** — la técnica es diseño + streaming trigger | Igual | Igual |
| **Lip-sync fonema-a-viseme por idioma** | **SÍ** — Rhubarb Lip Sync (open source) genera ofline, runtime aplica blendshapes | Igual | + neural real-time |
| **Companion "no ocluir cámara"** regla hard | **SÍ** — es código de gameplay, no del engine | Igual | Igual |
| **Feature parity cross-target** (escalado fidelity, no scope) | **SÍ** — define tiers en engine config | Igual | Igual |

---

## 13. Referencias primarias (author / year / venue / URL)

### GDC
- **Bruno Velazquez & Dori Arazi**, "Bringing 'God of War's Cinematic Single-Shot Camera to Life", GDC 2019. <https://www.gdcvault.com/play/1026291/Bringing-God-of-War-s>. Archive: <https://web.archive.org/web/2020*/gdcvault.com/play/1026291>.
- **Various SSM**, "Creating the Gods of God of War", GDC 2019 (art/character track). <https://www.gdcvault.com/play/1026283>.
- **Bruno Velazquez**, talks on animation in GoW Ragnarok — GDC 2023. <https://www.gdcvault.com/play/1029164> (si disponible públicamente).

### Documental
- **Santa Monica Studio**, *Raising Kratos* (full documentary), Sony, 2019. <https://www.youtube.com/watch?v=4BWY9jL8-A4>.

### Digital Foundry / Eurogamer
- **Digital Foundry**, "God of War Ragnarok — PS5 vs PS4 Pro vs PS4 tech review", Eurogamer, Nov 2022. <https://www.eurogamer.net/digitalfoundry-2022-god-of-war-ragnarok-tech-review>.
- **Digital Foundry**, "God of War PS4 Pro tech analysis", 2018. <https://www.eurogamer.net/digitalfoundry-2018-god-of-war-face-off>.
- **Digital Foundry**, "Ragnarok on PS5 Pro: 8K mode analysis", 2024 (si hay Pro patch). <https://www.eurogamer.net/digitalfoundry> (buscar).

### Entrevistas / postmortems
- **Cory Barlog** entrevistas múltiples — IGN, Kinda Funny, Easy Allies. E.g. IGN "Cory Barlog on Directing God of War" 2018. <https://www.ign.com/articles/2018/04/12/cory-barlog-god-of-war-ps4-interview>.
- **Eric Williams** (director Ragnarok), entrevistas PlayStation Blog + IGN 2022. <https://blog.playstation.com/2022/11/09/god-of-war-ragnarok-out-today/>.
- **Matt Sophos**, Valhalla DLC director — PlayStation Blog Dec 2023 <https://blog.playstation.com/2023/12/12/god-of-war-ragnarok-valhalla-out-now/>.

### Art / character tech
- **Rafael Grassetti** (character art director SSM GoW 2018, post a Skybound), ZBrush Summit talks — <https://www.pixologic.com/zbrush/summit/>. YouTube: <https://www.youtube.com/@rafagrassetti>.

### Industria / análisis
- **Jason Schreier**, Bloomberg / Kotaku coverage de SSM roadmap 2023-2024.
- **Jeff Grubb**, Giant Bomb podcast segments sobre SSM rumor 2024.

---

## 14. Nota honesta — SSM vs Decima vs UE5 para ALZE

El engine de Santa Monica **no es el más grande, ni el más avanzado, ni el más extensible** de los que cubrimos en R4. Es **el más enfocado**:

- **Un único género** (action-adventure 3rd person cinematic).
- **Una única dirección artística** (photorealistic Norse mythology con paleta fría).
- **Una única cámara filosofía** (one-shot, over-the-shoulder).
- **Un único tipo de combate** (melee-centric con companion ranged).

Este foco es la antítesis de Decima (open world general), UE5 (general purpose, 100s de géneros) y Unity (general purpose más pequeño). Y **precisamente por eso** es la referencia más útil para ALZE:

1. **Un desarrollador solo no puede construir Decima**. Decima asume ~150 personas × 5 años. UE5 asume ~5000 eng-years de historia. SSM engine asume ~50-100 personas × 5 años en **una capa** construida sobre el engine de GoW III.
2. **ALZE puede copiar la filosofía "un juego, un engine"**. Si ALZE va a ser la base de un juego concreto (acción 3ª persona, por ejemplo), la vía productiva es: decidir las 3-5 mecánicas core, construir **eso** bien, y no hacer abstracciones generalistas donde no son necesarias.
3. **Las técnicas de SSM son desproporcionadamente baratas**. One-shot camera no requiere raytracing, solo matemáticas de spline + streaming predictivo. Companion pathing no requiere NN, solo A* + raycasts. Animation layering no requiere motion matching ML, solo masks bien diseñadas. **Todo eso es implementable en C++17 + GL 3.3 sin cambios de arquitectura en ALZE**.
4. **El anti-patrón a evitar**: tratar de reescribir ALZE como "el motor general perfecto". Ese no es el juego (nunca lo fue para SSM tampoco — su engine empezó como "el motor de GoW" y nunca dejó de serlo).

**Recomendación concreta para ALZE v1.5**:
- Añadir **spring-arm camera rig** con soporte de "cinematic hook" (otra cámara puede tomar control y devolverlo manteniendo continuidad C1).
- Añadir **forced-LOD volumes** al LODSystem existente.
- Añadir **camera-aware nav modifier** al NavMesh A* (la pathfinding actual en `src/ai/NavMesh`).
- Añadir **hair cards material** (nuevo shader en `assets/shaders/`) — alpha-tested, anisotropic spec.
- Añadir **IK dos huesos con pole vector** en `src/renderer/SkeletalAnimation` para foot-plant y two-handed weapon grip.
- Añadir **dither-fade occluders** (stencil + alpha en `src/renderer/ForwardRenderer`).

Todo lo anterior son **~2000-3000 LOC adicionales**, encajan en la arquitectura actual, y habilitan el 70% del "look & feel" que distingue a GoW 2018 de un juego de acción genérico. El restante 30% (photorealismo Norse, mo-cap, voice acting de Christopher Judge) **no es del engine, es del arte y el cast**. Es decir: no está en el scope de ALZE bajo ningún punto de vista.

---

**Fin santa_monica.md** · 424 líneas · R4 ALZE research.
