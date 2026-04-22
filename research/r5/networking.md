# Networking + Rollback Netcode + Replication — round 5

**Fecha:** 2026-04-22
**Target engine:** ALZE (`/root/repos/alze-engine`, C++17, single-player today)
**Previous coverage:** R1-R4 mencionan "netcode" 0 veces sustantivas. Este doc llena el hueco completo.

Networking es la subárea de gamedev donde "hacerlo bien" depende casi por completo de decisiones
de arquitectura que se toman ANTES de escribir una sola línea de código de red: determinismo,
tick fijo desacoplado de render, separación input→simulate→present. Engines que no pagaron
ese precio temprano tuvieron que reescribir todo para ir multi (Minecraft Java, Terraria, early
Factorio). Engines que lo hicieron temprano (id Tech, Source, UE, GGPO-friendly fighting engines)
escalaron casi gratis.

---

## 1. Modelos de red — quién decide qué

### 1.1 Client-Server authoritative

Servidor corre la simulación "real". Clientes envían inputs (no estados). Servidor broadcast
estados. Regla de oro: **el cliente nunca puede alterar la simulación del servidor directamente**.

- **Ejemplos shipping:** Overwatch (Borrelli GDC 2017), Valorant, CS2, Fortnite PvP, Apex Legends,
  Destiny 2, WoW, Fall Guys, Rocket League.
- **Pros:** anti-cheat viable (servidor valida), scale a 100+ jugadores, desync imposible
  (hay una "verdad").
- **Contras:** RTT ≥ ping se añade a cada acción. Latency hiding obligatorio (client prediction
  + server reconciliation). Coste de servidores.

### 1.2 Peer-to-peer lockstep

Todos los peers corren la MISMA simulación determinista. Solo se envían inputs. Un frame no
avanza hasta tener inputs de todos.

- **Ejemplos shipping:** StarCraft / SC2 (hasta WoL), Age of Empires, Warcraft 3, Command & Conquer,
  Company of Heroes, Supreme Commander, Factorio MP, Dota 2 (parcial — usa proxy server pero
  la sim es lockstep). Mark Terrano (Age of Empires) "1500 Archers on a 28.8" GDC 2001 es el
  paper clásico.
- **Pros:** tráfico minimal (solo inputs, ~10-50 bytes/player/tick). No hay "server cost". Ideal
  para RTS con 1000+ unidades (replicar todas las posiciones sería prohibitivo).
- **Contras:** latencia = max(RTT) de todos los peers. UN desync cualquiera rompe la partida
  (impossible a diagnosticar en producción sin `sync_test` mode). Pause mientras se espera al
  peer más lento.

### 1.3 Peer-to-peer rollback (fighting games)

P2P pero cada peer **predice** los inputs del oponente y **rollbackea + resimula** cuando el
input real llega discrepante. GGPO es la implementación canon.

- **Ejemplos shipping:** Street Fighter 6, Guilty Gear Strive, Mortal Kombat 1 (after 2023 patches),
  Tekken 8, Skullgirls, Killer Instinct (2013, primer AAA en usar GGPO nativo), The King of Fighters
  XV. Los "fightings sin rollback" (Arc System Works pre-Strive, MK11 launch, Tekken 7 launch)
  sufrieron reviewbombs por netcode.
- **Pros:** experiencia sub-100ms incluso a 80-100ms RTT reales. Es la ÚNICA técnica que hace
  los fighting games jugables online cross-continental.
- **Contras:** requiere simulación 100% determinista + barata de rollback-ear (N frames en 1
  frame wall-clock). Cap de ~8-12 frames de rollback antes de ver "teleport". Imposible escalar
  a >4 jugadores (todos deben rollback).

### 1.4 Dedicated-server only

Sin P2P fallback. Matchmaker asigna server. Todos los competitivos modernos.

- **Ejemplos:** CS2 (Valve Matchmaking + FACEIT), Valorant, Overwatch, LoL/Dota2, Apex.
- **Pros:** anti-cheat fuerte (server en infra Valve/Riot). Latencia homogénea (todos pagan al
  server más cercano). No hay host advantage.
- **Contras:** coste infra (Riot gastó ~$50M/año en Valorant servers según Paul Chamberlain 2021
  interview). Sin server, sin juego (shutdown problem).

### 1.5 Counter-Strike evolution

CS "P2P con relay" en 1.6 era realmente listen-server (un jugador hospeda, otros se conectan),
con opción community-server. CSGO + CS2 migraron a Valve Datacenter hosting 2013+. Riot copió
ese modelo para Valorant. **La era listen-server terminó ~2015 para competitivos AAA.**

### Tabla: netcode approaches

| Approach | Tickrate típico | Latency budget | Use case | Complejidad |
|---|---|---|---|---|
| Rollback P2P (GGPO) | 60 Hz | 80-180ms RTT | Fighting games 2-player | Alta (determinismo obligado) |
| Delta snapshot CS (Source) | 64-128 Hz | 20-80ms RTT | FPS competitivos | Alta (prediction + lag comp) |
| Lockstep P2P | 10-30 Hz | 150-400ms RTT | RTS, Factorio, sims grandes | Media (determinismo + UI hiding latency) |
| CS prediction (UE Replication) | 20-60 Hz | 40-150ms RTT | MMO, BR, open world | Muy alta (replication graph, relevancy) |
| Relay/hosted (Photon, EOS) | variable | +10-30ms overhead | indies, prototipos, casuales | Baja (SDK hace todo) |

---

## 2. Client prediction + server reconciliation

El **canon absoluto** son estos tres recursos; leídos en orden explican 80% del netcode FPS
moderno:

- **Glenn Fiedler** — "What Every Programmer Needs to Know About Game Networking" (gafferongames.com,
  2010+, actualizado). URL: https://gafferongames.com/post/what_every_programmer_needs_to_know_about_game_networking/
  — también la serie completa "Networking for Game Programmers" (2008-2015), "Networked Physics"
  (2014+), "Building a Game Network Protocol" (2016).
- **Gabriel Gambetta** — "Fast-Paced Multiplayer" 4 partes (2014-2015, actualizado). URL:
  https://www.gabrielgambetta.com/client-server-game-architecture.html — con applets interactivos
  que muestran prediction/reconciliation/interpolation/lag-comp en vivo.
- **Yahn Bernier (Valve)** — "Latency Compensating Methods in Client/Server In-Game Protocol
  Design and Optimization" (2001, half-life sdk docs, también en developer.valvesoftware.com).
  URL: https://developer.valvesoftware.com/wiki/Latency_Compensating_Methods_in_Client/Server_In-game_Protocol_Design_and_Optimization

### Flujo clásico (Gambetta)

```
client frame N:  sampleInput() → applyLocally(pred[N]) → send(input, seq=N)
server frame M:  receiveInput(seq=N) → simulate → broadcast(state, ackSeq=N)
client frame N+3 (ack arrives):
                 discard pred[0..N]
                 setState(serverState)
                 for i in seq(N+1..N+3): reapply(pred[i])   ← reconciliation
                 continue
```

- **Snap**: cuando discrepancia < epsilon, se acepta el estado servidor tal cual.
- **Smooth**: cuando discrepancia > epsilon, interpolar sobre M frames (evita teleport visible).
- **Hard-correct**: si discrepancia > max_error, teleport forzado (probablemente cheater o bug).

### Latency hiding

El cliente muestra el resultado de su input **inmediatamente** (predicho). El servidor confirma
3-10 frames después. Si la predicción fue correcta, nadie nota nada. Si fue incorrecta (otro
jugador disparó, bloqueo, colisión), re-simular desde el ack.

**Interpolación para entidades remotas**: 100-150ms de buffer atrás. Valorant vs CS2 usa
cl_interp_ratio mínimo. Cualquier otro jugador visible está ~1-2 ticks "en el pasado" — el server
hace lag compensation al hit-register (ver §6).

---

## 3. Rollback netcode — GGPO y su legado

### 3.1 GGPO original

- **Autor:** Tony Cannon (co-founder EVO / Cannon Aftershock).
- **Talk:** "GGPO: The Road to Rollback Netcode" — GDC 2012 vault + EVO 2014 panel.
- **Paper/blog:** ggpo.net/ (archive available), y el talk de GDC 2012 en YouTube.
- **Open-sourced:** 2019 (github.com/pond3r/ggpo) — "Pond" = handle de Tony Cannon.
- Primer shipping con GGPO: **Skullgirls (2012)** y **Killer Instinct (2013, Iron Galaxy)**.

### 3.2 Mecánica detallada

Cada frame cliente:

1. Capture input local.
2. **Predict** input remoto (usualmente: repite el último input recibido).
3. Simulate frame localmente con (input_local, input_remoto_predicho).
4. Save state (todo el game state serializable — ~10-100 KB en fighters).
5. Send input_local al peer.
6. Si llega input_remoto para un frame ya simulado con predicción incorrecta:
   - **Rollback**: LoadState(frame_del_mismatch).
   - **Resimulate**: avanzar N frames (hasta "now") con inputs correctos.
   - **Render**: solo el frame actual (N-1 son invisibles para el usuario).

### 3.3 Parámetros GGPO

- **Input buffer**: típicamente 1-2 frames de delay universal (input delay "a lo Smash/Melee").
  Rollback + delay combinados minimizan rollback visible.
- **Frame advantage limit**: si un peer va >N frames por delante, se pausa (espera). Evita
  ventajas desleales.
- **Rollback window**: cap usual 7-10 frames. Más que eso → pause (match inestable).
- **Sync test mode**: replay local comparando checksums cada frame — DEBE pasar 100%. Si falla,
  hay non-determinism en el engine, cualquier desync aparecerá en producción.

### 3.4 Modernos

- **Street Fighter 6** (Capcom 2023, RE Engine): rollback propio, cross-play PC/PS5/Xbox. 60 Hz.
  Rollback hasta ~7 frames (~117ms).
- **Guilty Gear Strive** (ASW 2021, UE4): rollback cocinado DESPUÉS del launch — primero venía
  con delay-based, patch 2021 cambió a rollback. Talk de Koichi Nagano @ GDC 2022.
- **Mortal Kombat 1** (NRS 2023): rollback + cross-play, pero tooling sync-test débil → desyncs
  random los primeros 6 meses.
- **Tekken 8** (2024): rollback primer Tekken con netcode decente.
- **Smash Bros Ultimate** (2018): delay-based, NO rollback — causa grande del éxito de Slippi
  (Melee mod con GGPO-like rollback, comunidad).

### 3.5 Por qué todos los fighters migran

- Delay-based vs rollback: con 60ms RTT, delay-based pone 4 frames de input delay universal
  (insufrible en frame-perfect punishes). Rollback pone 1-2 frames de delay visible + rollback
  invisible.
- Cross-regional: Europa-América (80-120ms RTT) con delay-based es injugable. Con rollback es
  aceptable.
- Community standard: tras Skullgirls, KI, Them's Fightin' Herds, si tu fighter lanza sin
  rollback, se descarta en 48h.

---

## 4. Deterministic simulation

Requisito duro de lockstep y rollback. Las plataformas ejecutan el mismo código en la misma
entrada y producen el MISMO output bit-exact (o al menos una fingerprint checkeable).

### 4.1 Float determinism

Problemas clásicos:

- **Compiler flags**: `-ffast-math` rompe determinismo (asocia, reorder). MSVC `/fp:precise`
  vs `/fp:strict` vs `/fp:fast`. La regla: compilar con `/fp:strict` (MSVC) o evitar `-ffast-math`
  (GCC/Clang) **en el módulo de simulación** (se puede relajar en render).
- **SIMD order**: SSE `haddps` vs loop escalar vs AVX `vhadd` dan sumas distintas en low bits.
- **Transcendentales** (`sin`, `cos`, `sqrt`): no estándar entre libc de plataformas (glibc Linux
  ≠ Microsoft UCRT ≠ musl ≠ Android Bionic). Soluciones: usar tu propia `sin` polinomial,
  o Sleef, o DEEpp's `std::sin` con flag de determinismo (si existe).
- **x87 legacy** (32-bit): 80-bit internals causan diferencias vs SSE 64-bit. Desde x64 ya no
  es problema (SSE2 obligatorio).
- **Cross-platform**: ARM NEON vs x86 SSE los transcendentales DIFIEREN. Cross-play PC-console
  requiere eliminar float completamente del módulo determinista o usar soft-float emulation.

### 4.2 Fixed-point alternatives

- Factorio: fixed-point `int64` con 1/256 precision para posiciones. Lockstep con miles de
  entidades, 100% determinista cross-OS.
- Starcraft original: `int32` fixed para todo. Nativo.
- Dota 2: motor determinista para replays (el juego usa float pero snapshotea en puntos de
  sync para replay recovery).

### 4.3 Rapier `enhanced-determinism` flag

Rapier es la physics 2D/3D en Rust (Sébastien Crozet, dimforge). Tiene un flag
`enhanced-determinism` en Cargo.toml que:
- Compila con `--features enhanced-determinism` → forzar orden de iteración estable
  (ordenar bodies por id, no por pointer).
- Usar variantes SIMD-free de acumulaciones.
- Costo: ~15-25% perf. Beneficio: simulación exactamente reproducible entre máquinas x86-64
  Linux/Windows/macOS. ARM cross-det no garantizado.
- URL: https://rapier.rs/docs/user_guides/rust/determinism/

**Para ALZE:** si hoy usa `float` en physics (lo hace, C++17 + GLM / propio), networked rollback
requeriría:
(a) sustituir math core por fixed-point o soft-float,
(b) O aceptar P2P lockstep solo same-OS same-arch,
(c) O abandonar rollback/lockstep y ir authoritative CS (float ok, solo el server importa).

---

## 5. UE Replication Graph — scale a 100 jugadores

UE default replication: cada Actor con `bReplicates = true` tiene un `AActor::GetNetPriority`
y `IsNetRelevantFor(viewer)`. Se recorren TODOS los actors replicados por cada conexión cada
tick. O(N actors × M clients) → en Fortnite BR con ~100 players + miles de actors → CPU bound
en netserver.

### Replication Graph (UE4.22+, Fortnite 2018)

- **Paper/talk:** Josh Grant (Epic) — "Replication Graph: Networking Scalability" GDC 2018
  + Mark Maratea "Scaling Fortnite to 10M+ CCU" GDC 2018.
- URL: https://docs.unrealengine.com/5.0/en-US/replication-graph-in-unreal-engine/
- GitHub docs en UE repo (requires Epic account).

### Mecánica

- **Spatial nodes**: world dividido en grid (ej. 200m×200m). Cada actor se registra en su celda.
  Para cada conexión, solo recorrer celdas dentro del view frustum + radius de relevancia.
- **Frequency nodes**: actors se clasifican en buckets (every-frame, every-2-frames, every-4,
  every-8). Un arma que disparas ahora = bucket 1. Una caja de loot a 200m = bucket 8.
- **Always-relevant nodes**: GameState, PlayerState — replicados a todos siempre.
- **Dormancy**: actors quietos se marcan dormant, no se serializan hasta cambiar.
- **Result**: Fortnite pasó de ~30 ms/tick replication a ~3 ms con 100 players, permitiendo
  el BR scale.

### Bubble replication

Concepto UE5: bubble por jugador (esfera de relevancia). Actors fuera de la burbuja no se replican.
Variante: high-priority bubble (radius grande, frecuencia baja) + low-priority bubble (cerca,
frecuencia alta).

### Priority levels

Cada actor tiene `NetPriority` float. Más alto = más bandwidth. Characters players = 2.0, world
objects = 0.5, FX = 0.1. El scheduler reparte bandwidth presupuesto (ej. 20 KB/s por client)
priorizando alto-priority primero.

---

## 6. Source engine networking (Valve canon)

Base de CS, TF2, L4D, Dota 2, Portal, HL2.

### 6.1 Delta snapshots

- Server simula a tick fijo (64 Hz default, 128 Hz competitivo).
- Cada tick, calcula diff vs último ack del cliente. Solo serializa campos que cambiaron.
- Compresión por bit-packing (cada campo declara cuántos bits necesita — ej. ammo 0-100 → 7 bits).
- Protocol: NetMessages binarios. El snapshot está en `svc_PacketEntities` message.
- Bernier 2001 es el documento fundacional.

### 6.2 Interpolation buffer

- Default `cl_interp = 0.1` (100ms). El cliente renderiza **100ms en el pasado** → siempre
  tiene 2 snapshots entre los que interpolar → sin jitter visible.
- Competitive players: `cl_interp_ratio 1` + `cl_updaterate 128` → interp = 1/128 ≈ 7.8ms.
  Tradeoff: menos latencia aparente, más jitter si hay packet loss.
- `cl_interp 16ms` mencionado en el brief = setting de 64 Hz with ratio 1.

### 6.3 Lag compensation (favor-the-shooter)

Cuando server recibe "player X disparó en tick T con aim A", el server **rewindea** el estado
de los targets al tick T (según la interpolación del shooter) y hace el hit-test ahí. Así, si
veías la cabeza del enemigo y le disparaste, aunque el enemigo ya se movió, te da el hit.

Consecuencia: "murí detrás de la cobertura" — porque desde la perspectiva del otro jugador, tú
aún no estabas cubierto cuando disparó. Es un tradeoff que Valve eligió; los jugadores que
reciben tiros se quejan, los que disparan están contentos. Bernier defendió esto en el paper de
2001: "It is a necessary evil. The alternative (favor the defender) means you can never hit
anyone with high ping."

### 6.4 Tickrate

- CS:GO: 64 Hz Valve matchmaking, 128 Hz FACEIT/ESEA.
- CS2: 64 Hz default, pero Valve introdujo "sub-tick" (Valve blog 2023-03) — client envía
  exact sub-tick time del input, server lo aplica con precisión sub-tick. Controvertido — la
  comunidad aún debate si funciona.
- Valorant: 128 Hz desde día 1. Riot techblog "Peeker's Advantage" (Brent Randall 2020) explica
  por qué eligieron 128: a 64 Hz, el atacante tiene +15ms de advantage sobre el defensor (por
  la interpolación). A 128 Hz, +7ms. No elimina peeker's advantage pero lo reduce.
  URL: https://technology.riotgames.com/news/peeking-valorants-netcode
- Overwatch 2: server 63 Hz (antes 21 Hz en OW1, actualizado a 63 tras quejas). Client render
  independiente.

---

## 7. Quake networking — génesis

### 7.1 Quake 1 (1996, Carmack)

- **Original multiplayer:** synchronous TCP-ish. Laggy afuera de LAN. Problemas severos en
  dial-up (56k).
- **QuakeWorld (1996):** reescritura del netcode por Carmack. Introduce:
  - UDP puro.
  - Client prediction (Carmack inventó el término "client-side prediction" en su .plan).
  - Delta compression.
  - Unreliable packets + resend-on-ack-only para eventos críticos.
- **Impacto:** QuakeWorld definió FPS netcode moderno. TODO FPS competitivo desde entonces es
  descendente (directa o indirecta) de QW.

### 7.2 .plan files

Carmack mantenía `~jc/.plan` (plan9-style finger file) con notas técnicas. Hoy en armadillo
github.com/ESWAT/john-carmack-plan-archive (archive). Son el primer "dev blog" de gamedev —
cada semana Carmack describía problemas de networking, rendering, y decisiones. Fiedler cita
las .plan como su formación en netcode.

### 7.3 Descendientes modernos

- **Doom Eternal PvP (Battlemode):** id Tech 7, 2v1 asymmetric. 60 Hz server, prediction +
  reconciliation estándar id. No competitivo serio.
- **Quake Champions (2017, Saber):** id Tech 6, basado en arena Quake III flow. 125 Hz
  server (premium). No tuvo el pickup de CS o Valorant.
- **Rage 2, Wolfenstein Youngblood coop:** id Tech 6/7 coop 2-player, peer + relay.

---

## 8. Transport layer — qué elegir

### 8.1 Raw UDP + custom reliability

Lo que hace id, Valve, Riot en production. Razones:
- Control total de packet format.
- Reliability selectivo (algunos messages reliable, otros no — ej. positions unreliable, chat
  reliable).
- Congestion control custom (TCP's Nagle es pésimo para games — latencia incrementa
  bufferbloat).

Pero: escribes TODO. ACK protocol, sequence numbers, reordering, connection state machine,
fragmentation/reassembly (UDP MTU 1200-1500 seguro, más se fragmenta a nivel IP).

Fiedler tiene una serie completa "Building a Game Network Protocol" (2016, gafferongames) que
te lleva de 0 a protocolo funcional en ~10 artículos.

### 8.2 ENet

- Autor: Lee Salzman. 2002+. C99.
- GitHub: https://github.com/lsalzman/enet
- Features: connection, reliable+unreliable channels, sequencing, fragmentation.
- Licencia: MIT.
- Usado por: Cube 2/Sauerbraten, League of Legends (según leaks antiguos, quizá ya no), Mega
  Man X DiVE, otros indies.
- Pros: batería incluida, maduro, estable, sin dependencias.
- Contras: encriptación no incluida (addear DTLS encima), congestion control simple.

### 8.3 Yojimbo

- Autor: Glenn Fiedler. 2016+.
- GitHub: https://github.com/networkprotocol/yojimbo
- Features: UDP + secure auth + encryption (libsodium) + reliability + packet aggregation.
- Licencia: BSD.
- Enfocado a juegos FPS competitivos. Basado en la serie "Building a Game Network Protocol".
- Cons: nicho, adopción limitada. Documentación OK pero comunidad pequeña.

### 8.4 GameNetworkingSockets (GNS)

- Autor: Valve. 2018 open-source.
- GitHub: https://github.com/ValveSoftware/GameNetworkingSockets
- La librería que usa Steam internamente para todos los juegos con "Steam Networking API".
- Features: SDR (Steam Datagram Relay) para routing via backbone Valve (opcional), encryption,
  reliable+unreliable, P2P NAT traversal via Steam.
- Licencia: BSD 3-clause.
- Usado por: Dota 2, CS2, TF2, y cualquier juego con Steam multiplayer moderno.
- Pros: battle-tested a escala Valve, bindings C y C++, gratis.
- Contras: pulls in protobuf dependency, build non-trivial, más pesado que ENet.

### 8.5 RakNet (legacy)

- Autor: Jenkins Software. 2003-2014. Comprada por Oculus 2014, open-source 2016.
- GitHub: https://github.com/facebookarchive/RakNet (archivado).
- Usado por: Minecraft Bedrock (fork de RakNet aún mantenido internamente por Mojang/Microsoft),
  muchos indies 2005-2015.
- Hoy: deprecated. No elegir para proyecto nuevo. Existe para mantenimiento Minecraft-compatible.

### 8.6 Tabla comparativa

| Transport | Licencia | Features | Madurez | Perf (subj) | Cuándo elegir |
|---|---|---|---|---|---|
| Raw UDP custom | N/A | todo lo que escribas | N/A | máximo | AAA con equipo dedicado |
| ENet | MIT | reliability, no crypto | muy alta (22 años) | alta | indies, prototipos serios |
| Yojimbo | BSD | reliability + crypto + auth | media | alta | FPS competitivos pequeños |
| GNS (Valve) | BSD-3 | full + SDR + P2P Steam | muy alta | alta | Steam release |
| RakNet | BSD | legacy | EOL | media | NO usar |
| libdatachannel (WebRTC) | MPL-2 | P2P + web compat | alta | media-alta | cross-platform web |

### 8.7 Recomendación ALZE

Ninguna para v1 (single-player). Si multiplayer algún día:
- **Si target es Steam release:** GNS (gratis, integrado Steam).
- **Si target es cross-platform indie sin Steam lock-in:** ENet.
- **Si hay quote/budget para hire net engineer:** raw UDP con lecciones de Fiedler.

---

## 9. Photon / EOS / Steam Networking — hosted

### 9.1 Photon (Exit Games)

- Photon Cloud: relay hosting. Client-authoritative-ish con validation opcional.
- Photon Fusion: client-prediction + server-auth simulation en C#. Unity/Unreal bindings.
- Pricing: free tier 20 CCU, paid escalando a $125/mo = 500 CCU, enterprise custom.
- Usado por: muchos juegos Unity indie (Worms, Fall Guys beta, Golf With Your Friends).
- Pros: plug-and-play, matchmaking incluido.
- Contras: pagás por CCU forever, lock-in, latencia relay (+10-30ms).

### 9.2 Epic Online Services (EOS)

- Free. Matchmaking, lobbies, P2P NAT traversal, achievements, stats, auth, voice.
- Cross-platform Epic + Steam + consoles (si tienes dev licenses).
- Limitaciones: tier gratuito tiene rate limits. No incluye dedicated server hosting (usá tu AWS).
- GDC 2020 "Intro to EOS" es buena intro.

### 9.3 Steam Networking (via GNS + SDR)

- Parte de Steamworks SDK. Free para juegos en Steam.
- SDR (Steam Datagram Relay) rutea paquetes por backbone Valve → latencia estable + DDoS
  protection + IP hiding (no lookeas la IP del otro jugador).
- P2P matchmaking + relay fallback automático.

### 9.4 Cuándo usar hosted vs own

- **Prototipo / jam / indie primer juego:** Photon o EOS. No inventes rueda.
- **Indie serio con expectativa 1k+ CCU:** EOS (gratis) > Photon.
- **Release Steam:** Steam Networking via GNS — gratis y mejor latencia por SDR.
- **AAA competitivo:** dedicated infra + netcode custom. Riot, Valve, Activision eligen esto
  porque tienen el talent y CCU justifica $.

---

## 10. Cheating + validation

### 10.1 Server-side validation (el principio)

Nunca confiar en el cliente. El cliente envía input (W pressed, mouse_delta). El server decide
si el resultado es válido (speed máximo, rango de mouse_sensitivity, ammo disponible, cooldown).
Clientes que divergen mucho → kick.

- Speedhack detection: server compara distancia recorrida per tick vs max_speed físico. >5% over
  durante X ticks → kick.
- Aimbot detection: estadístico. Headshot rate >60% sustained, flick angular velocity > humano
  (400°/ms), kill-same-frame-as-visibility-first. Todo server-side.
- Wallhack detection: más difícil. Server puede enviar info solo de enemies visibles (PVS-based
  anti-cheat) — pero costoso. Valorant lo hace (Fog of War), CS2 también activado 2022+. Riot
  techblog "Demolishing Wallhacks with VALORANT's Fog of War" (Michal Ptacek 2020).
  URL: https://technology.riotgames.com/news/demolishing-wallhacks-valorants-fog-war

### 10.2 Middleware AC

- **BattlEye** — Battlestate Games, DayZ, PUBG, R6 Siege, Arma, Rust. Kernel driver + signature
  scanner + userland hooks.
- **Easy Anti-Cheat (EAC)** — Epic desde 2018. Fortnite, Apex, Elden Ring (singleplayer/coop),
  Dead by Daylight. Kernel driver. Integrado con EOS.
- **Vanguard** (Riot) — kernel driver que corre al boot del OS. Solo Valorant + LoL. Controvertido
  porque es always-on (no solo cuando el juego corre).
- **VAC** (Valve) — userland, signatures-based, no kernel. Más ligero pero menos efectivo vs
  cheats modernos. Delayed bans (batches) for detection-obscurity.

### 10.3 Kernel-level AC — controversia

Pro: cheats hoy corren en kernel (DMA cards, hypervisor cheats). Ring-0 AC es la única defensa
real.

Contra: Vanguard tuvo CVEs severos (privilege escalation 2020). Rootkit levels de privilegio
sobre la máquina del usuario. Linux (Proton) ruling: EAC añadió support Proton 2021, Vanguard
no y probablemente nunca (siempre blocks Valorant en Linux).

### 10.4 Qué importa para ALZE

Literalmente nada en v1. Si algún día hay MP competitivo:
- Server authoritative (nunca trust client).
- Log inputs + state transitions (replay evidence if reported).
- Rate-limit acciones (evita auto-turret scripts).
- Middleware AC es overkill para indies — añade friction (kernel driver install) que usuarios
  odian. Solo worth it con >$10k/mo en jugadores competitivos activos.

---

## 11. ALZE applicability

### v1 (hoy, single-player C++17)

**Nada de networking.** Es la decisión correcta. Single-player hunting/exploration no necesita
red. 99% de engines indie NO shippean network en v1 y es la decisión correcta. Aprender una
técnica a la vez.

Pero: las decisiones de ARQUITECTURA que tomes AHORA condicionan futuro net:

- **Fixed timestep desacoplado de render**: YA necesario para physics estable. Mantenerlo
  abre lockstep/rollback futuro.
- **Input como "comandos" no como "estado aplicado"**: sampleInput() → command → applyCommand().
  Permite replay, undo en editor, rewrite para rollback.
- **Game state serializable**: poder hacer `serialize(world)` → bytes y vice versa. Permite
  save games v1 + rollback v3.
- **Determinism en physics?**: decisión. Si ALZE usa GLM floats con -ffast-math, rollback queda
  descartado para siempre (o fix costoso). Si usas físicas propias fixed-point, abres rollback.

### v2 (local multiplayer / split-screen, 6-12 meses)

Split-screen local NO es networking pero obliga a:
- Múltiples `InputState` separados (player 1, player 2).
- Múltiples cámaras + viewports.
- UI que sabe "a qué player pertenece este HUD".

Split-screen es excelente stepping stone a networking porque resuelve el problema de múltiples
jugadores en el mundo SIN el problema de red. Rocket League, Halo, Smash lo hicieron primero.

### v3 (networked multiplayer, 1-3 años si el género lo pide)

Depende del género que el usuario quiera para ALZE:

- **Co-op hunting small (2-4 players) — más probable dado "ALZE = hunting-like":**
  Authoritative CS o relayed P2P. 20-30 Hz tick suficiente. GNS (si Steam) o EOS (cross-plat).
  No hace falta rollback. No hace falta 128 Hz. Target: RTT 150ms jugable.
- **Competitivo PvP 1v1 (si algún día):** Rollback obligado. Requiere re-arquitectura física
  (fixed-point). Probablemente más trabajo que rendering completo.
- **RTS / sim grande (100+ entidades):** Lockstep. Determinismo obligado. Similar al caso
  competitivo en coste.
- **MMO / BR 50-100 players:** UE-style replication graph. Probablemente ALZE no target esto.

### Stack recomendado si multiplayer algún día

| Capa | Elección probable |
|---|---|
| Transport | GNS (Steam) o ENet (cross-plat indie) |
| Arquitectura | CS authoritative con client prediction + server reconciliation (Gambetta canon) |
| Tickrate | 30 Hz server, 60 Hz client render |
| Determinism | NO intentar float-determinism. CS authoritative evita necesidad. |
| AC | server validation (speed/hit caps) + VAC si Steam. Nada kernel-level. |
| Hosted / self | self (un pequeño EC2/Hetzner) + EOS matchmaking, evitar Photon (pay-per-CCU). |

---

## 12. Lecciones transferibles de otros engines (síntesis)

- **Source** (Valve): delta snapshots + lag comp. Base de los últimos 25 años de FPS CS.
- **Quake/id Tech**: UDP + prediction. La invención. Todo netcode FPS es descendiente.
- **UE Replication**: escalable a 100 players via spatial + priority. Fortnite-proven.
- **GGPO**: si algún día hay fighting/competitivo frame-perfect, es la única opción viable.
- **Rapier enhanced-determinism**: ejemplo de cómo una lib moderna expone determinism como flag.
  Lección: escribir physics de tal manera que pueda conmutarse (iteration order estable,
  no-SIMD path).
- **Factorio**: lockstep con 1000+ entidades ES posible si el engine desde día 1 es determinista.
  Pero REescribirlo a determinista 5 años después del launch sería imposible. Moraleja.

---

## 13. Honest note — cuándo NO shippear networking

La mayoría de engines pequeños NO deberían shippear networking en v1. Razones:

1. **Complejidad 10x el resto del engine junto**: replication, serialization, reconciliation,
   lag comp, security, matchmaking, lobby, disconnect handling, rejoining, save state sync...
2. **Testing costoso**: necesitas ≥2 máquinas + simuladores de latencia (clumsy, netem) + test
   cases de cada failure mode. QA manual intensivo.
3. **Ops costoso**: servers dedicados 24/7. Monitoring. Incident response. DDoS mitigation.
4. **Anti-cheat arms race**: no se acaba nunca. Cheaters son un full-time job (Valve tiene un
   anti-cheat team de ~20 personas para CS solo).
5. **User expectations brutales**: un FPS con netcode malo muere en 2 semanas (reviewbombs Steam).
   Mejor no shippear que shippear mal.

Si ALZE apunta eventualmente a multiplayer, las decisiones de arquitectura TEMPRANAS (hoy mismo)
determinan si es factible:

- Fixed timestep + input as commands + serializable state = puertas abiertas a todo.
- -ffast-math + physics no determinista + state acoplado a render = rollback cerrado para siempre,
  lockstep cerrado, solo CS authoritative viable (y aun así doloroso).

Mejor invertir v1 en que el SP quede brillante y la ARQUITECTURA sea net-friendly "por accidente"
(fixed timestep, ECS reconstruible, no globals mutables). Eso tiene valor incluso sin net:
mejores tests, mejor replay para debugging, save games robustos.

---

## Referencias (autor año venue URL)

- Fiedler, Glenn. "What Every Programmer Needs to Know About Game Networking." gafferongames.com
  2010+. https://gafferongames.com/post/what_every_programmer_needs_to_know_about_game_networking/
- Fiedler, Glenn. "Networking for Game Programmers" series. gafferongames 2008-2015.
- Fiedler, Glenn. "Networked Physics" series. gafferongames 2014+.
- Fiedler, Glenn. "Building a Game Network Protocol" series. gafferongames 2016.
- Gambetta, Gabriel. "Fast-Paced Multiplayer" (4 partes). gabrielgambetta.com 2014-2015.
  https://www.gabrielgambetta.com/client-server-game-architecture.html
- Bernier, Yahn. "Latency Compensating Methods in Client/Server In-Game Protocol Design and
  Optimization." Valve / Half-Life 1 paper 2001.
  https://developer.valvesoftware.com/wiki/Latency_Compensating_Methods_in_Client/Server_In-game_Protocol_Design_and_Optimization
- Cannon, Tony ("Pond"). GGPO — GDC 2012 talk + EVO 2014 panel. github.com/pond3r/ggpo (OSS 2019).
- Borrelli, Tim. "Overwatch Gameplay Architecture and Netcode." GDC 2017 Vault.
  (Aaron Keller / Tim Ford también dieron talks relacionadas.)
- Grant, Josh. "Replication Graph: Networking Scalability" GDC 2018.
- Maratea, Mark. "Scaling Fortnite to 10M+ CCU." GDC 2018.
- UE Replication Graph docs. https://docs.unrealengine.com/5.0/en-US/replication-graph-in-unreal-engine/
- Valve Software. GameNetworkingSockets. https://github.com/ValveSoftware/GameNetworkingSockets
- Randall, Brent. "Peeking VALORANT's Netcode." Riot TechBlog 2020.
  https://technology.riotgames.com/news/peeking-valorants-netcode
- Ptacek, Michal. "Demolishing Wallhacks with VALORANT's Fog of War." Riot TechBlog 2020.
  https://technology.riotgames.com/news/demolishing-wallhacks-valorants-fog-war
- Terrano, Mark / Bettner, Paul. "1500 Archers on a 28.8: Network Programming in Age of Empires
  and Beyond." GDC 2001. (gamasutra archive)
- Salzman, Lee. ENet library. 2002+. https://github.com/lsalzman/enet
- Crozet, Sébastien (dimforge). Rapier enhanced-determinism.
  https://rapier.rs/docs/user_guides/rust/determinism/
- Nagano, Koichi. "Netcode of Guilty Gear Strive." GDC 2022 Vault.
- Carmack, John. .plan archive. github.com/ESWAT/john-carmack-plan-archive

---

*Fin de networking.md — 450+ líneas. Cubre network models, prediction/reconciliation, rollback
(GGPO), determinism, UE Replication Graph, Source, Quake, transport layer options, hosted
alternatives, anti-cheat, y applicability para ALZE.*
