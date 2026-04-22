# Four "Second-Tier" Proprietary Engines — Dossier

> Contexto ALZE: estos cuatro motores no dan para un dossier dedicado cada uno,
> pero cada uno ilustra una lección concreta (técnica u organizativa). No son
> Unreal/idTech/Frostbite: son el "segundo escalón", lo bastante ambiciosos
> para romperse y lo bastante específicos para enseñar algo.

---

## Glacier 2 (IO Interactive)

### Historia

Glacier 1 (IO Interactive, Copenhagen) debuta con *Hitman: Codename 47*
(2000); también *Freedom Fighters*, *Kane & Lynch 1/2*, *Mini Ninjas*. Glacier 2
es rewrite mayor para *Hitman: Absolution* (2012), presentado en GDC 2012
(Fauerby, lead programmer) con foco en el sistema "G2-Crowd". Iteraciones:
*Hitman* (2016 episódico), *Hitman 2* (2018), *Hitman 3* (2021) — renombrados
en 2023 como *HITMAN World of Assassination* unificando las tres campañas.
Proyecto actual: *007 First Light* (licencia MGM/Eon, ship 27 mayo 2026)
sobre Glacier, con nuevo volumetric smoke system "not done anywhere before".
Es el único motor que IO ha usado desde fundación: 25+ años iterando sobre el
mismo core.

### NPC crowd tech — el sello distintivo

Charla Fauerby GDC 2012: **1200 agentes por multitud**, **500 on-screen**, a
30 fps en Xbox 360/PS3. Filosofía explícita "quality over quantity" — cada
NPC con algo significativo. Crowd sim separada del resto del AI: LOD agresivo,
scheduling por prioridades, representación simplificada fuera de cámara.

En *Hitman* (2016+) la escala baja (mapas más verticales, más estado por NPC)
pero sube la **densidad de comportamiento**: rutinas diarias interrumpibles,
disguise system con "círculo de sospecha" por tipo de disfraz, estados de
alerta globales vs locales con decay temporal.

### Rutinas deterministas

Punto subestimado: las rutinas NPC son **deterministas reboot a reboot**
(misma semilla ⇒ mismo patrón). Es lo que permite el diseño: el jugador
aprende un mapa porque el NPC X siempre pasa por el punto Y en el segundo Z.
Sin determinismo el loop "plan → ejecución" no funciona. IO lo trata como
gameplay contract, no como feature.

### Hitman tech that matters

Dense scripted worlds con sim ligera (no hace falta sim profunda si el
authoring es denso); deterministic routines + interrupt priorities con
"abandon conditions" taggeadas (ve cadáver, oye disparo, ve disfraz
sospechoso); ragdoll/cloth estables desde Absolution (el jugador esconde
cuerpos, las físicas son requisito de gameplay); disguise/detection graph
como datos, no lógica hardcoded.

---

## Dunia (Ubisoft Montreal)

### Linaje — fork de CryEngine que se separó pronto

2008: Ubisoft Montreal licencia CryEngine 1 para *Far Cry 2*. Kirmaan
Aboobaker (ex-Crytek) encabeza el fork; tras el primer año sólo un **2-3%
del código CryEngine 1** sobrevive en Dunia. Línea: *FC2* (2008, dir.
Hocking) → *FC3* (2012, rewrite parcial) → *FC4* (2014) → *Primal* (2016) →
*FC5* (2018) → *New Dawn* (2019) → *FC6* (2021). Estado 2017 (Quenin,
architect): Dunia soporta "vegetation, fire simulation, destruction, vehicles,
systemic AI, wildlife, weather, day/night cycles, non-linear storytelling".
*Far Cry 7* ("Blackbird") ha sido reportado (Kotaku) migrando a **Snowdrop**
(motor interno Ubi Massive, *The Division* y *Star Wars*). Dunia se retira.

### Far Cry series evolution

FC2 ambicioso en sim ambiental, mapas grandes pero vacíos. FC3 hace "reset
comercial" — simplifica sim ambiental, sube densidad narrativa y outpost loop
(Vaas). FC4–FC6 son refinamiento iterativo: cada entrega añade un bioma
(Himalaya, Montana, isla pacífica, Yara) pero el motor no cambia
drásticamente. El "same game different hat" viene en parte de ahí — Dunia
impone la fórmula.

### Fire/weather propagation — feature firma

Fire como **sistema de grilla** (Levesque, postmortem): terreno
discretizado; cada celda tiene flammability, moisture, wind exposure; una
celda en llamas daña a vecinas sesgado por vector de viento → frente en
forma de campana que sopla en la dirección correcta. Variables ambientales:
wind speed/direction, rain, vegetation type; sabana seca prende, jungla
húmeda no, rain literalmente apaga fuego — emergente, no scripted.
Vegetación instanciada + wind procedural con "hair-transplant technique"
para reposicionar emitters; foliage deforma bajo viento, colisión, impactos;
ramas se queman y regeneran. Gameplay: FC2 usa fuego como arma; FC4+ usa
weather para enmascarar stealth.

---

## Creation Engine 2 (Bethesda)

### Linaje Skyrim → Starfield

Gamebryo era: *Morrowind* (2002), *Oblivion* (2006), *Fallout 3* (2008),
*New Vegas* (2010) — forks crecientes de Gamebryo/NetImmerse. Creation
Engine 1 es fork profundo de Gamebryo, debut con *Skyrim* (2011); luego
*Fallout 4* (2015), *Fallout 76* (2018). Creation Engine 2 se anuncia en
2021 y debuta con *Starfield* (2023); Todd Howard lo describe como "la mayor
revisión desde Oblivion": RT global illumination, volumetric lighting, Havok
para animación de personaje, procedural lip-syncing, mejoras en física y
post-process. TES VI previsto ~2028 sobre Creation Engine 2.

### Mod ecosystem — el verdadero moat

Formato **.ESM / .ESP** idénticos salvo un byte en header (master vs plugin);
ESL en Skyrim SE+ añade light plugins (4096 cargados en un slot). El mundo
se describe como **records** (NPC, item, celda, diálogo) agrupados en GRUPs;
cada record tiene **FormID estable**. El **load order** resuelve conflictos:
cualquier plugin redefine un record sin recompilar nada. Creation Kit público
= mismo editor que el equipo interno. **Papyrus** es scripting stackable
(mods extienden scripts base). ABI estable por décadas: mods de Morrowind se
pueden migrar parcialmente a Skyrim. Resultado: Skyrim (2011) sigue vivo
comercialmente en 2026 con 100k+ mods comunitarios. Ningún otro motor AAA
consigue ese ciclo de vida.

### Limitaciones conocidas

Starfield expuso los límites: loading screens en todas partes (no aprovecha
SSD next-gen, Digital Foundry lo marcó como "missed opportunity"); planetas
son conjuntos de patches generados con loading screen entre ellos; animación
de personaje sigue por detrás de CDPR/Naughty Dog; el modelo "game object per
item" acumula persistent references en saves → inestabilidad tras 100+ horas
(desde Oblivion hasta Starfield); 1080p DRS + FSR2 → 4K a 30 fps en Xbox
Series X con settings bajos, no escala con hardware moderno.

---

## Luminous (Square Enix)

### FFXV troubles — el engine empezó torcido

2012: Square Enix anuncia "Luminous Studio" como tech showcase (demo *Agni's
Philosophy*) — motor next-gen interno que reemplace Crystal Tools (murió en
el parto de FFXIV 1.0). *FFXV* (2016) tuvo desarrollo desastroso: empezó como
*FF Versus XIII* en PS3 con Crystal Tools, pivotó a PS4 con Luminous, cambió
de director (Nomura → Tabata), mutó de scope; shipped incompleto, patches
por años, arco de DLC cancelado. Tech flex: 5M polígonos/frame, 150M polys/
sec; pelo de Noctis con ~20k polígonos (5× generación anterior), >50 shaders,
referenciado en estilistas reales sobre mannequins; volumetric lighting,
hair physics dedicados. Pero costoso de autorear, rendimiento inestable,
pipeline difícil; vía entrevistas Luminous "no fue diseñado para NVMe ni
streaming de texturas moderno".

### Luminous Productions como subsidiaria

En 2018 Square Enix forma **Luminous Productions** como subsidiaria, en parte
para sacar al equipo técnico de la fricción interna de FFXV y darles proyecto
nuevo. Project Athia → **Forspoken (2023)**: IP nueva (Frey), open world +
parkour mágico + Luminous lucido. Recepción mixta a mala: crítica tibia,
meme-ificación del guion, ventas bajas (Forspoken en Steam lanzó peor que
*Hi-Fi Rush*, sorpresa de $30 del mismo mes). El juego "real" se hizo en ~18
meses; todo anterior fue conceptual. 150 devs — 2-3× menos que FFXV.

### Dissolución — el engine que mató al estudio

**28 febrero 2023** — Square Enix Holdings anuncia que Luminous Productions
se fusionará con Square Enix Co. Ltd. el 1 mayo 2023. Menos de dos meses tras
el lanzamiento de Forspoken. Efectivamente: cierre. Luminous de facto muerto.
**FFXVI** (2023) y **FFVII Rebirth** (2024) usan UE/in-house de CBU III.
Square Enix después anunció commitment con UE5 para mainline.

El arco completo — tech demo brillante → juego troubled → estudio dedicado →
juego fallido → disuelto — es una de las narrativas de engine más claras de
la década. "La tecnología era bonita pero no producía juegos a tiempo" es el
resumen interno según entrevistas.

---

## En qué son buenos cada uno

- **Glacier 2** — multitudes densas con comportamiento individual (1200
  agentes con rutina significativa), rutinas deterministas, disguise/detection
  como grafo de datos, ragdoll/cloth estables, iteración continua 25+ años.
- **Dunia** — sim ambiental emergente (fire como grilla con viento/humedad),
  vegetación masiva con wind procedural, day/night y weather como señales de
  gameplay, streaming de terrenos open-world.
- **Creation Engine 2** — ecosistema de mods que mantiene juegos vivos décadas,
  formato plugin/record estable, Creation Kit público, radiant AI, Papyrus.
- **Luminous** — character rendering (pelo, piel), volumetric lighting, path
  tracing temprano, polígonos/frame altos, presentaciones técnicas bonitas.

---

## En qué fallan cada uno

- **Glacier 2** — opacidad total (casi nada público fuera de la charla 2012);
  cinemáticas detrás de Unreal en fidelidad facial; herramientas no
  reutilizables; un único estudio lo usa.
- **Dunia** — fork drift caro (mantener rama divergente de CryEngine 15+ años
  es sangría); la fórmula Far Cry se estancó porque el motor impone cómo son
  los juegos; aging, por eso FC7 migra a Snowdrop.
- **Creation Engine 2** — loading screens ubicuos, no aprovecha SSD moderno,
  animación facial tras la competencia, bugs crónicos en persistent refs,
  modelo "game object per item" no escala.
- **Luminous** — costoso de autorear, pipeline frágil, no diseñado para
  NVMe/streaming moderno, tech-demo-driven en lugar de production-driven, y
  mató a su propio estudio.

---

## Qué podríamos copiar (mecanismo concreto) de cada uno para ALZE Engine

### De Glacier — rutinas NPC taggeadas con interrupt priorities

Cada NPC con un **schedule** de 24h en bloques de 15 min, cada bloque con
**tarea** (waypoint + acción) y **abandon-vector** (eventos que rompen la
rutina, cada uno con prioridad):

```
- t: 09:00  duration: 15m  task: patrol_route_A
  abandon_on:
    - event: witnessed_violence     priority: 100
    - event: witnessed_disguise     priority:  70
    - event: heard_gunshot          priority:  60
```

Determinismo: mismo seed + mismo input player = misma ejecución. Escalable
porque los bloques son pequeños y la mayor parte del tiempo el NPC está en
LOD barato.

### De Dunia — propagación en grilla con tags de flamabilidad/humedad

Discretizar terreno en cells (Voronoi o quadtree), cada cell con
**flammability**, **moisture**, **fuel_load**, **wind_exposure**. Fire (o
water, infection, plaga) se propaga con regla simple:

```
damage_to_neighbor = base_damage
                   × flammability(n) × (1 - moisture(n))
                   × wind_alignment(n, wind_vector)
```

Generalizable: mismo código sirve para fire, flooding, disease, rumor, sound.
Clave: **tag system** — taggear assets con propiedades de propagación en vez
de lógica ad-hoc por asset.

### De Creation — mod support como feature first-class

Diseñar el engine desde día uno para que **todo asset y entidad esté en un
record store con ABI estable**: FormID único 64-bit, formato binario tipo ESM
con records + groups + fields, load order resuelve conflictos (último plugin
gana), content hash por plugin para integrity, scripting dedicado stackable,
editor público idéntico al interno. Coste upfront alto; beneficio: mod
ecosystem que extiende la vida del juego 10× más allá del soporte oficial.

### De Luminous — lección organizativa (no técnica)

No hay tech que copiar. Lección: **no envíes tech demos como juegos**. No
separar "engine team" del "game team" hasta que el engine esté estable en
producción. No announcement-driven tech. Si tu tech demo es más impresionante
que tu juego, el juego está en problemas. Medir semanas por features
shippeables, no por benchmarks.

---

## Qué NO copiar

- **De Glacier** — su opacidad. Un small-team necesita lo opuesto: docs
  abundantes, dogfooding abierto.
- **De Dunia** — "fork profundo y divergir para siempre". Forkear CryEngine
  y mantener rama 15 años drena recursos; fork ligero + upstream periódico,
  o build from scratch.
- **De Creation** — "game object per item en mundo persistente". Cada
  botella/tenedor/flecha es entidad con estado guardable. No escala; es la
  razón de los saves corruptos en Skyrim/Fallout/Starfield. Para ALZE: estado
  agregado por región, entidades individuales sólo cuando gameplay lo requiera.
- **De Luminous** — priorizar tecnología sobre producción. Si el pipeline no
  deja al designer iterar en minutos, el engine está mal hecho independientemente
  de cuántos polígonos renderiza.

---

## Lección cross-engine

Cada uno ilustra un modo distinto:

- **Glacier** — éxito por **foco continuo**: un motor, un género, 25 años.
  No cambies de core sin razón brutal.
- **Dunia** — éxito por **feature firma** (fire/weather) que definió una
  saga. Identifica 1-2 sistemas que sean *tu* cosa. Fallo: no evolucionar el
  core arrastra fork drift.
- **Creation** — éxito **no** por la tecnología sino por el **ecosistema**.
  El moat real suele estar fuera del engine (community, tools, mod ABI).
- **Luminous** — fallo por **tech-over-production**. Un motor bonito que no
  produce juegos a tiempo cierra estudios.

Un equipo pequeño como ALZE no puede aplicar las cuatro a la vez. **Elige
una.** Apuesta: la de Creation — mod/extension ecosystem first-class —
porque es la única que transforma la vida útil del producto en órdenes de
magnitud; el resto son higiene.

---

## Fuentes consultadas

- Fauerby, K. "Crowds in Hitman: Absolution", GDC 2012 slides (media.gdcvault.com).
- GDC Vault — "Creating the AI for the Living, Breathing World of Hitman: Absolution"
  (play/1019353); "Creating a Scalable and Destructible World in Hitman 2" (gdc-19/1026198).
- Gamasutra / Game Developer — "Interview: How Far Cry 2's Fire Fuels, Spreads";
  "The making of Far Cry 2".
- Levesque, J-F. — "Far Cry: How the Fire Burns and Spreads" (jflevesque.com, 2012-12-06).
- Wikipedia — Dunia Engine, CryEngine, Creation Engine, Luminous Engine, Luminous
  Productions, 007 First Light.
- StarfieldDB — "All the new Starfield Creation Engine 2 features"; GamingBolt —
  "Starfield Technical Analysis"; GameRant — "Starfield's Creation Engine Will Have
  to Face the Reaper"; NeoGAF — Digital Foundry Starfield tech + PC reviews.
- UESP Wiki — "Skyrim Mod:Mod File Format"; CK Wiki — "Persistence (Creation Kit)".
- Game World Observer — "Luminous Productions merge with Square Enix" (2023-02-28);
  PC Gamer — "Square Enix snuffs out Luminous"; Push Square — "Forspoken Dev Luminous
  Productions Will Soon Be No More"; Dualshockers — "Former Luminous Engine Lead
  Explains...". MegaVisions — "From Final Fantasy to Forspoken".
- Kotaku / GamesRadar / Insider Gaming — Far Cry 7 Snowdrop migration; Idle Sloth X.
- IOI — ioi.dk/glacier; ioi.dk/about. The Boar (2025) — "From collapse to classic:
  The making of the Hitman trilogy".

> Nota: no se localizó pieza Schreier/Bloomberg específica sobre disolución de
> Luminous (el anuncio fue nota oficial Square Enix 2023-02-28). Si aparece, incorporar.
