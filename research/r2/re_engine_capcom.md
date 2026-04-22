# RE Engine (Capcom) — "Reach for the Moon"

> Investigación autónoma 2026-04-21 para ALZE Engine. Fuentes: Capcom Open Conference RE:2023, CEDEC talks, Capcom IR 2025, GDC 2019 (AMD/Capcom optimization slides), Autodesk dev blog on DD2 tools, Creative Bloq, Digital Foundry, 80.lv, Wikipedia. Cuando algo es especulación vs sourced, lo marco [SPEC].

---

## Overview

RE Engine (officially "Reach for the Moon Engine", no relación directa con "Resident Evil") es el motor propietario AAA de Capcom, sucesor de **MT Framework** (2006-2016, usado en Dead Rising, Lost Planet, Devil May Cry 4, Monster Hunter 3/4, Resident Evil 5/6, Dragon's Dogma original). MT Framework a su vez reemplazó una fase anterior donde cada equipo interno tenía su propio motor — ineficiencia que Capcom resolvió centralizando (Onimusha 3 engine como base, desde 2004).

Timeline:

- **2014**: empieza desarrollo de RE Engine (sobre base MT Framework 2.9, según Capcom Database).
- **2017**: Resident Evil 7 Biohazard — primer título shipped. VR day-1.
- **2019**: RE2 Remake + Devil May Cry 5 (dos AAA en meses). Capcom premiado en CEDEC Awards 2020.
- **2020**: RE3 Remake.
- **2021**: Resident Evil Village (water + volumetric RT).
- **2023**: RE4 Remake + Street Fighter 6 + Exoprimal. Capcom Open Conference RE:2023 (abre cajas técnicas).
- **2024**: Dragon's Dogma 2 (primer open world).
- **2025**: Monster Hunter Wilds (máxima escala). Street Fighter 6 en Switch 2.
- **2026+**: Pragmata y Resident Evil Requiem integran path-tracing vía colaboración con NVIDIA. Capcom avanza **Codename REX / RE neXt Engine**, evolución incremental (no reemplazo).

**Equipo (Capcom IR 2025)**: R&D Foundational Technology Department ≈ **200 ingenieros**, ~**160 dedicados a engine dev**. Arrancó con ~20 y creció 7x. "Engine team" y "game teams" son orgs separadas; la mobility de developers entre juegos no requiere reaprender el motor.

**Modelo "games-as-tech-iterator"**: cada shipping de juego alimenta features de vuelta al engine core, que habilita el siguiente juego. RE7 trajo photogrammetry → RE2/3 la pulió → Village añadió RT/water → RE4 añadió strand hair + particles → DD2 añadió open-world primitives → MH Wilds añadió ecosistemas. Asset sharing explícito entre títulos (según Capcom IR 2025).

## Architecture (what's known from GDC talks + CEDEC talks)

De **GDC 2019 "DirectX 12 Optimization Techniques in Capcom's RE Engine"** (AMD/Capcom, slides en gpuopen.com, sobre RE2 + DMC5) y **Capcom Open Conference RE:2023**:

- **Deferred renderer** (G-buffer clásico) para opaques, **Forward+** (clustered) para transparents y partículas. Cluster-based shading para acotar cost de muchas luces.
- **PBR** estándar (metallic/roughness), con extensión específica para **skin (subsurface scattering)** y **hair (anisotropic + multi-scattering)**.
- **DirectX 12** nativo (PS5/XSX), con path DX11 para PS4/XBO. **Async compute** usado intensivamente: GPU particles, post-process, light culling en paralelo a G-buffer fill.
- **Material Editor** node-graph → compila a shader permutations. Soporta nodes especializados (wrinkle maps para skin, strand output para pelo). Externally hackeado via REMesh-Editor + MDF2 (material description files).
- **Scripting**: **C# 8.0 / .NET** sobre VM propia **REVM**. REVM convierte C# → IL → AOT propia. Durante dev, prioridad iteración (MicroCode interpretado); en build de release, prioridad perf (C++ codegen). GC custom. (Fuente: Capcom COC RE:2023 session 14).
- **Path tracing** añadido incrementalmente, colaboración NVIDIA para Pragmata y RE Requiem (GDC Festival of Gaming 2026, sesión de Hitoshi Mishima + Calvin Hsu).
- **TAA** es el default AA; crítica común (Wccftech/ResetEra) — suaviza detalle fino, especialmente hair y lejanía.

## Photogrammetry + character pipeline

Para RE7 Capcom construyó un **3D scan studio dedicado** con **~140 cámaras DSLR**: array de 100 para full-body + 40 para facial scanning (fuente: Gamereactor, 80.lv, TechRadar). **Más de la mitad de los assets de RE7 vinieron por photogrammetry**.

Workflow:
1. Capture: cámaras fire simultaneous → 100+ fotos por ángulo.
2. Photogrammetry reconstruction (software externo, tipo RealityCapture/Agisoft [SPEC]) → mesh denso + texture albedo.
3. Clean-up artist en DCC (Maya/ZBrush).
4. Retopology + bake maps → LOD chain.
5. Import a RE Engine con material auto-assign (Material Assign Tool, detallado en dev blog Autodesk 2025 sobre DD2).
6. Skin → subsurface scattering shader; ojos → wet eye shader.

Photogrammetry aplicado a: **architecture, organic materials, facial capture, cloth wrinkles**. En casos creativos, **scans de carne real** (meat processing plant, para texturas de criaturas) y **claymation** para monsters imposibles (mold físico → scan → digital). Hero faces conocidos: Mia Winters (RE7), Leon/Ada (RE2/4R), Ethan/Mother Miranda (RE8), Itsuno's Pawns (DD2).

**Hair rendering** (Capcom COC RE:2023 session 09, "Resident Evil 4 Hair Discussion"):
- Dos paths: **card-based** (clásico, barato, mayoría de NPCs y titles cross-gen) + **strand-based** (nuevo, hero characters solamente).
- Strand data creada en **Ornatrix** → exportada Alembic → import a RE Engine → ShaderGraph driving color + anisotropy.
- **Un solo artista** responsable de strands en toda la casa (Capcom); no es escalable a NPC crowds.
- No hay hard limit de characters-con-strand, limitado por VRAM/perf budget.

## RE4 Remake breakthroughs

RE4 Remake (2023) es iterativamente el RE7 más maduro, no revolución. Avances concretos:

- **Strand hair** debut en hero characters (Leon, Ashley, Luis). Multi-scattering lighting.
- **Ray-traced GI / reflections / shadows / AO** opcionales (Capcom añadió RT en updates posteriores sobre la base ya presente en RE Village).
- **Particle system** reescrito: fuego del pueblo (village burn scene), ash, sparks interactivos con wind.
- **Volumetric fog** denso con light-shaft interaction (lejos del fog boxy de MT Framework).
- **Water interaction** mejorado: lago de Salazar, wet-material state machine.
- **SSR** pulido, combinable con RT reflections en high-end.
- **Subsurface scattering** pipeline tuneado — piel de Leon comparada favorablemente con Naughty Dog work por Digital Foundry.

RE4R es el "RE Engine a 100%" en corridor/action-horror. Excelente ejemplo de "stop improving engine, ship game, integrate learnings".

## Dragon's Dogma 2 challenges

DD2 (2024) es el primer **open-world title** en RE Engine — y se nota. Eurogamer: "Dragon's Dogma 2 expanded RE Engine functionality for open world games". El motor fue built for tight corridors; retrofit a mundo abierto trajo:

**Qué añadieron al engine** (Autodesk dev blog 2025):
- **Character Creation System** unificado para player + NPCs + Pawns (1000+ unique NPCs).
- **Goblin Editor**: variación procedural de monsters.
- **Dungeon Editor**: grid-based modular assembly, 7000+ building asset types.
- **Weather System**: 15 weather types x 3 regions, wind system (5 strengths) afectando grass/cloaks/hair/smoke, VolumetricFog, CloudScape shadows.
- **Characterization**: re-target generic humanoid anims a body types distintos sin mocap por variant.
- **DLSS 3 nativo** (primer RE Engine title, según PCGamesN).

**Qué falló (Digital Foundry analysis)**:
- **CPU-bound en ciudades**. Cada NPC tiene full physics presence + AI schedule; con decenas juntos, incluso Ryzen 7 5800X3D e Intel i9-13900K stutter.
- **Workaround de Capcom**: NPC draw distance limitada agresivamente + player puede literalmente **matar NPCs para liberar CPU** (exploit reconocido, no feature).
- **Traversal stutter** al loadear chunks (streaming open-world primitives no eran first-class).
- **Frame-time spikes** consistentes.

Lección: **el engine no tenía open-world streaming budget discipline baked-in**. Tenían que engrampar sistemas sobre fundamentos corridor-centric. DD2 sufrió en performance press reviews a pesar de recepción crítica mixta-positiva.

## Monster Hunter Wilds (2025)

El título más ambicioso en RE Engine a la fecha. Requirements:

- **Mapas ~2x tamaño** de MH entries anteriores.
- **Seamless open world**, sin loading screens entre camp y hunt.
- **Multiple environmental variations** por mapa (desert → storm → windfall).
- **Real-time day/night cycle + dynamic weather** (dunas cambian, monsters reaccionan).
- **Herds**: múltiples large monsters a la vez, con alpha-leader semantics. Director Yuya Tokuda: "capacity to calculate the AI of multiple different groups at the same time".
- **Seikret** mount (bird-like, reemplaza Palamute) para traversal vertical.
- **4-player co-op** concurrent, cada hunter con armor PBR detallado.

**Performance reality** (Steam Community discussions + reviews tech):
- VRAM 9.2-9.8 GB durante hunts multiplayer en biomes complejos (10 GB GPU → bordeline).
- Stutter issues amplios en launch (PC y consoles), heredando los problemas de DD2.
- "Stretches the RE Engine too far" (Gfinity), Engine optimización por debajo de lo que Capcom acostumbra.
- Capcom Tokuda: direct collaboration con engine team porque features que pedía no existían en third-party engines. Ventaja política del in-house; pero no saca gratis el trade-off técnico.

Interpretación: MH Wilds muestra que **el engine puede shippear escala open-world**, pero al ceiling de performance. Requiere otra iteración (REX) para dar headroom.

## Cross-gen strategy

Cómo RE Engine corre PS4 (Jaguar, 8GB) y PS5/XSX simultáneamente (ninguna release PS4 exclusive perjudicando next-gen, ni viceversa):

- **LOD budgets configurables por platform profile**. Mismo scene tree, diferentes mesh/texture/shadow densities. PS4 baja mesh LOD, corta RT, reduce volumetric fog resolution, shadow cascades más agresivas.
- **Texture streaming virtualized** agresivo; RE Engine aprovecha SSDs de next-gen ("5-10 seconds to load" en DD2 PC con NVMe).
- **Dynamic resolution scaling** universal. Targets 60 Hz con DRS entre 1080p-1440p en PS5.
- **Async compute donde está disponible** (PS5/XSX/PC DX12), fallback a synchronous en PS4/XBO.
- **Same codebase**: no hay rama "PS4 version" del engine. Compile flags + runtime platform profile.
- **Input/output abstraction** layer delgado; los game teams no tocan hardware-specific code.
- RE Village en particular es el paragón: excelente en PS4 y stunning con RT en PS5, mismo binary conceptual.

## Tooling + iteration

- **RE Engine Studio** (editor interno, no public-facing): integra nivel, scene, material, particle, animation, scripting. Similar UX a UE editor.
- **Pipeline DCC → engine**: FBX + custom formats (.mesh propietario via MDF2 materials). Plugins para Maya/ZBrush/MotionBuilder. Ornatrix para strand hair.
- **C# hot reload**: REVM interpreter path permite iterar scripts sin recompile del juego. (Capcom COC RE:2023 session 14: "priority is iteration speed, MicroCode output + interpreter").
- **Asset hot reload** [SPEC, inferencia + práctica standard en engines modernos]: sí hay live-edit while running según entrevistas, aunque el blog de Autodesk sobre DD2 no menciona explícitamente hot-reload. Probable: textures/materials/scripts sí; code C++ core no.
- **Real-time motion capture preview** (Capcom COC RE:2023 session 12): actores en mocap ven su performance en engine a tiempo real, sin post-process pass.
- **REFramework** (community, not official): mod loader externo que confirma arquitectura interna (scripting reachable, introspección vía Lua injection, VR support).

## En qué es bueno

- **Productividad shipping**: 10+ AAA en ~8 años (RE7 → RE2R → DMC5 → RE3R → Village → RE4R → SF6 → Exoprimal → DD2 → MH Wilds → Requiem/Pragmata 2026). Cadence que ni Ubisoft ni Rockstar igualan con motores in-house.
- **Cross-gen flexibility**: shipa PS4 + Switch 2 + PS5 + PC sin forks del engine.
- **Photogrammetry pipeline maduro**: capex de scan studio ya amortizado, hero faces at bar con cualquier AAA competitor.
- **Performance characteristics estables**: en corridor/arena genres, RE Engine es referencia (RE4R, SF6, DMC5 son canónicos en tech press).
- **Modding community decente en PC** (Fluffy Mod Manager, REFramework, NexusMods). No es UE-tier, pero existe.
- **Iteration speed interna**: C# + REVM + in-house tools → artists y scripters iteran sin rebuilds largos.
- **Asset reuse entre títulos**: piezas de RE2 acabaron en RE4R; lighting tech de Village en RE4R. No es gold-plating cada vez.

## En qué falla

- **Open-world strain**: DD2 + MH Wilds muestran techo claro. Streaming primitives, NPC-density simulation, CPU budget, todo sufre al salir de corridor assumptions.
- **CPU-bound NPC simulation**: diseño donde cada NPC tiene full physics/AI presence no escala a crowds; workaround "matar NPCs para performance" en DD2 es embarrassing.
- **Character plateau**: faces excellent, skin excellent, pero **hair mayormente sigue card-based**; strand solo en hero characters; cloth-physics modesta vs Nanite-era cloth de UE5.
- **Menos documentado públicamente** que Unity/UE → community/onboarding harder. Capcom Open Conference (2019, 2023) publicó slides, pero sigue menos rico que UE docs o Unity Learn.
- **TAA flojo**: smear/ghosting perceptible (crítica recurrente en Digital Foundry y reddit reviews de SF6/RE4R).
- **Ray tracing tardío**: RT integrado post-hoc desde Village; path tracing solo via colab NVIDIA para Pragmata/Requiem 2026, no first-class.
- **Sin ECS canónico documentado**; arquitectura OO tradicional con Actor/Component pattern. Escala peor que data-oriented en escenas masivas (MH Wilds).

## Qué podríamos copiar (mecanismo concreto) para ALZE Engine

1. **Games-as-tech-iterator cadence**. No dejar que engine team drift ahead of shipping. Cada "ALZE release" debería corresponder a un game/demo real que consume features nuevas. Iteración corta: ship → learn → extend → ship. Capcom's 10-titles-in-8-years es la prueba — don't build the perfect engine in a vacuum.

2. **Node-graph Material Editor compilando a shader permutations**. Mecanismo concreto: `MaterialGraph { nodes, connections }` → compile pass genera GLSL/HLSL con #defines para features presentes. Permutation cache on disk para build incremental. Artists nunca escriben shader manualmente.

3. **Photogrammetry-ready pipeline**: aunque ALZE no tenga scan studio, mantener la **hero asset pipeline** lista: acepta mesh denso + textures 8K + color-calibrated albedo (chartt-based), proceso de retopology opcional, LOD auto-chain. Así cuando exista presupuesto de scan o colaboración con studio externo, integrar un hero asset es zero-effort.

4. **Cross-platform LOD budgets expresados en engine-config**. Mecanismo: un archivo `profile_low.toml` / `profile_mid.toml` / `profile_high.toml` con caps numéricos (max_shadow_cascades, max_dynamic_lights, texture_budget_mb, mesh_lod_bias). Artists y level designers ven warning si scene excede budget del target platform. Sin **manual per-platform assets**.

5. **C#-style scripting VM con doble path interpreter/AOT**. Mecanismo: en dev build, interpreter (iteration speed: edit-save-instantly-reload); en release build, AOT a nativo. REVM model. Para ALZE con equipo pequeño, incluso usar Wren o Lua con el mismo patrón funciona — lo clave es el **doble path**, no el lenguaje.

6. **Asset hot-reload while game running**. Mecanismo: file watcher daemon en editor → cuando detecta cambio en `.tex`/`.mat`/`.mesh`/`.script`, emit engine event → unload+reload solo ese asset handle, sin restart del juego. Crítico para iteration loop en level design y VFX tuning.

7. **Engine team separado de game teams pero colocados**. Game directors pueden caminar a engine team y pedir features directas (Yuya Tokuda cita explícita). Evita el "engine team builds what they think is cool, game team gets surprised at ship time".

## Qué NO copiar

- **Assumir corridor-genre defaults**. Capcom commited a corridor/arena scope — open world tuvo que forzarse encima. Si ALZE no decide genre temprano, **debe** defender primitives de streaming/LOD/spatial-partitioning como first-class desde v0. No es trivial retrofit.
- **NPC simulation pesada sin budget explícito**. Cada actor con full physics+AI no escala. Desde v0 aplicar **tiered simulation**: hero (full), mid-range (reduced AI tick), background (stateless animation + LOD crowd shader).
- **Photogrammetry discipline como commit prematuro**. Requiere capture team dedicado (cameras, cross-polarizer lights, calibration chart, software licenses Agisoft/RC, talento retopology). No bootstrap-friendly para small team. Mejor: **accept high-quality 3rd-party scans** (Quixel/scans open sourced) y construir pipeline para importarlos, sin operar el scan studio in-house.
- **Scripting VM AAA-scale antes de tener shipping game**. REVM es impressive, pero Capcom puede pagar 160 engine engineers. ALZE con equipo pequeño: embed Wren/Lua off-the-shelf al comienzo, swap a VM custom solo si se justifica con shipping pain real.
- **Menos docs públicos**. Capcom mantiene mucho interno; para ALZE (especialmente si piensa en community/modding), documentar desde día 1 con examples, no slides de conferencia cada 4 años.

## Fuentes consultadas

- [RE Engine — Wikipedia](https://en.wikipedia.org/wiki/RE_Engine)
- [MT Framework — Wikipedia](https://en.wikipedia.org/wiki/MT_Framework)
- [Capcom IR 2025: RE ENGINE Integrated Report](https://www.capcom.co.jp/ir/english/data/oar/2025/re-engine.html)
- [Capcom Open Conference RE:2023 — RE ENGINE's Past and Future](https://www.capcom-games.com/coc/2023/en/session/10/)
- [Capcom Open Conference RE:2023 — C# 8.0 / .NET Support](https://www.capcom-games.com/coc/2023/en/session/14/)
- [Capcom Open Conference RE:2023 — Resident Evil 4 Hair Discussion](https://www.capcom-games.com/coc/2023/en/session/09/)
- [Capcom Open Conference RE:2023 — Real-Time Motion Capture Preview](https://www.capcom-games.com/coc/2023/en/session/12/)
- [Capcom Open Conference RE:2023 — RE ENGINE Philosophy](https://www.capcom-games.com/coc/2023/en/session/03/)
- [GDC 2019 — DirectX 12 Optimization Techniques in RE Engine (AMD)](https://gpuopen.com/gdc-presentations/2019/gdc-2019-s4-optimization-techniques-re2-dmc5.pdf)
- [Autodesk dev blog — In-House RE ENGINE Tools For Dragon's Dogma 2 (2025)](https://blogs.autodesk.com/media-and-entertainment/2025/04/10/ideas-and-ingenuity-behind-capcoms-in-house-re-engine-tools-for-dragons-dogma-2/)
- [Creative Bloq — How Monster Hunter Wilds was made possible by Capcom's RE Engine](https://www.creativebloq.com/3d/video-game-design/how-monster-hunter-wilds-was-made-possible-by-capcoms-secretive-re-engine)
- [80.lv — Resident Evil: Requiem Shows Full Potential of RE Engine](https://80.lv/articles/resident-evil-requiem-shows-the-full-potential-of-capcom-re-engine)
- [80.lv — Resident Evil 7: Photogrammetry for VR](https://80.lv/articles/resident-evil-7-the-use-of-photogrammetry-for-vr)
- [Gamereactor — New RE Engine improves 3D modelling with photogrammetry](https://www.gamereactor.eu/new-re-engine-improves-3d-modelling-with-photogrammetry/)
- [TechRadar — How photogrammetry brought Resident Evil's world to life](https://www.techradar.com/news/how-photogrammetry-brought-resident-evils-world-of-undeath-to-life)
- [Push Square / Digital Foundry — Capcom Must Improve DD2 PS5 Performance](https://www.pushsquare.com/news/2024/03/capcom-simply-must-improve-dragons-dogma-2-ps5-performance-concludes-digital-foundry)
- [Wccftech — Street Fighter 6 Benchmark: Another RE Engine Flex](https://wccftech.com/street-fighter-6-benchmark-test-another-re-engine-flex-with-caveats/)
- [GamingBolt — Pragmata/RE Requiem Path Tracing in RE Engine](https://gamingbolt.com/pragmatas-resident-evil-requiems-re-engine-path-tracing-integration-explained-in-hour-long-video)
- [PCGamesN — DD2 DLSS support, first RE Engine game natively](https://www.pcgamesn.com/dragons-dogma-2/dlss)
- [Capcom Press Release 2020 — CEDEC Awards First Prize for RE ENGINE](https://www.capcom.co.jp/ir/english/news/html/e200909.html)
- [REFramework (community mod loader)](https://reframework.dev/)
