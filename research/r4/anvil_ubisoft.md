# Anvil / Scimitar → AnvilNext → Anvil Pipeline (Ubisoft Montreal)

*Research date: 2026-04-22. Round 4 deep-dive for ALZE Engine. Sister to `snowdrop_ubisoft.md` (round 2); focus here is on Anvil specifically — the open-world systemic engine powering the Assassin's Creed series, For Honor, Rainbow Six Siege, Prince of Persia: The Lost Crown, Skull and Bones, and AC Shadows (2024).*

## 1. Lineage and naming history

Anvil is arguably Ubisoft's most-shipped-on proprietary engine, born in Ubisoft Montreal around 2005–2006 and still evolving in 2024–2026. The public naming timeline [sourced: Wikipedia "Anvil (game engine)"; Ubisoft News engine retrospectives; Digital Foundry "AC Shadows tech review" 2024]:

| Year | Label | First major title | Notes |
|------|-------|-------------------|-------|
| 2007 | **Scimitar** | *Assassin's Creed* (AC1) | Original codename; Patrice Désilets' team. Xbox 360 / PS3 launch. |
| 2009 | **Anvil** (rename) | *Assassin's Creed II* | Renamed after AC1 shipped. Dense Renaissance Florence/Venice. |
| 2012 | **AnvilNext** | *Assassin's Creed III* | Rewritten for PS3/360 late-gen and forward compat. Dynamic weather, naval. |
| 2014–2015 | **AnvilNext 2.0** | *AC Unity* (2014), *AC Syndicate* (2015) | Full PBR, GPU skinning-heavy crowds, global illumination rework. |
| 2017–2020 | AnvilNext 2.0 (iterative) | *Origins* 2017, *Odyssey* 2018, *Valhalla* 2020 | RPG pivot; massive map scale (Greece ~256 km²); motion-matching anim adopted. |
| 2020 | **Rainbow Six Siege** backbone | *R6 Siege* (Ubisoft Montreal multiplayer branch) | Siege runs on a fork of AnvilNext — destructible wall system is a Siege-specific Anvil layer, not ported across the family. |
| 2023–2024 | **Anvil Pipeline** | *AC Mirage* 2023, *Prince of Persia: Lost Crown* 2024, *AC Shadows* 2024, *Skull and Bones* 2024 | Current rename; same lineage, modernised renderer (RT GI baseline in Shadows), updated asset pipeline. |

[sourced: Wikipedia; Ubisoft News; IGN "How Anvil evolved"; Digital Foundry AC Shadows Nov 2024 tech review]

Ownership: Ubisoft Montreal is the primary engine team, with Ubisoft Quebec (Syndicate, Odyssey, Immortals Fenyx Rising) contributing major renderer and world-gen work. Smaller satellite contributions from Ubisoft Sofia, Ubisoft Singapore (naval stack — AC IV, Skull and Bones), Ubisoft Bordeaux (AC Mirage lead), Ubisoft Kyiv, Bucharest, Pune. Unlike Snowdrop's Massive-centric ownership, Anvil is distributed.

### Scimitar → Anvil rename context (2009)

Scimitar was the pre-release internal name used during AC1 dev (2004–2007). After AC1 shipped, the rename to Anvil happened quietly circa AC2 dev. Scimitar still appears in some early AC2 credits and splash screens [sourced: Ubisoft Montreal post-mortem talks, Game Developer Magazine 2009]. There is no technical rewrite between Scimitar and Anvil — it's the same codebase, rebranded for marketing clarity.

### AnvilNext (2012) — what actually changed

Per the AC3 tech talk at GDC 2013 by Ubisoft Montreal rendering lead [sourced: GDC Vault 2013 "Next Generation Anvil"; gamedeveloper.com]:

- New renderer with dynamic weather / ToD (first-class; AC2's weather was mostly baked).
- Naval stack (ship rigging, fluid wave sim, boarding interop) built on new foundation.
- LOD system redone for forest density (AC3's New England frontier was the densest forest in a AAA open-world up to that point).
- Crowd / NPC routine system rewritten; AC3's Boston and New York could sustain a "full Revolutionary-War era market street" believably.

### AnvilNext 2.0 (2014, AC Unity)

The big leap. AC Unity shipped rough but the engine improvements that survived [sourced: Ubisoft News, Eurogamer Unity retrospective, Digital Foundry AC Unity tech review Nov 2014]:

- Full **physically-based rendering** across all materials.
- **Global illumination** baked + dynamic blend (not probe-only, not fully real-time — hybrid).
- **Massive-crowd rendering**: Unity's Revolutionary Paris showed up to ~5,000 NPCs in Bastille storming scenes; this was the signature marketing shot.
- **Seamless interior/exterior**: in AC2-AC4 every door was a loading screen; Unity was the first AC where you could walk from Parisian streetscape into Notre-Dame's interior without a load (streaming layer + BVH of interior volumes).
- **New parkour system** ("parkour up/down distinction") — context-sensitive button mapping for ascending vs descending.

The Unity launch problems were QA and crowd LOD pop-in, not engine architecture failures. The underlying Anvil 2.0 work is what Odyssey/Valhalla/Shadows iterate on.

### Anvil Pipeline (2023–2024)

Per Ubisoft's communications around AC Mirage and AC Shadows [sourced: Ubisoft News engine note; Digital Foundry Shadows Nov 2024 review; Eurogamer Shadows preview]:

- Renderer rebuilt around ray-traced GI as **baseline** (Shadows is the first AC where RT GI is always on, not an RTX-only setting).
- Asset pipeline modernised; Houdini procedural integration deepened (see §9).
- Updated motion-matching runtime, informed by LaForge research (§8).
- Four-season world simulation (Shadows), first in AAA open-world (§6).

## 2. Crowd simulation — the AC signature

Crowds are Anvil's genre-defining system. Jean-François Lévesque (Ubisoft Montreal, crowd AI programmer) gave the canonical GDC talks [sourced: GDC Vault 2018 "Massive Crowd on Assassin's Creed Unity: AI Recycling", gdcvault.com; GDC 2019 "Crowd Rendering in AC Unity"; slideshare Levesque talks]:

### Crowd architecture (AC Unity era, still largely intact in Shadows)

- **LOD tiers.** NPCs sit in four or five LOD buckets by distance + importance:
  - **Full NPC** (< ~15 m, or in camera): full AI, full animation, full cloth, 1:1 per-frame update.
  - **Simplified NPC** (~15–50 m): coarser behavior tree, shared animation cycles, reduced cloth.
  - **Ambient NPC** (~50–150 m): no individual AI — pathed along pre-computed "lanes" on the navmesh with crowd flow rules; animation is a shared GPU-skinned instance.
  - **Billboard / sprite** (> ~150 m): camera-facing impostor with a baked animation loop.
  - **Stat only** (census level): no render, only a population counter used by perception systems ("is this district crowded?").
- **Recycling** (the 2018 talk's core): rather than simulate 5,000 unique NPCs, the engine has a pool of ~200–500 *active* NPCs that get re-assigned (body, outfit, routine) as the player moves through the world. A butcher in one street is recycled into a baker two blocks over when the player can't see either. The illusion of a full population comes from pool turnover, not concurrent simulation. This is the **single most important crowd-sim trick in AC**.
- **Routines.** Each archetype has a daily routine graph (market in morning, home at night, tavern at sunset). Routines are authored data, not procedural. NPCs swap routines when re-used.
- **Flow fields on streets.** Ambient crowd movement uses per-street flow fields (pre-baked Eikonal distance or custom) rather than per-agent A*. Agents sample the field to pick a direction and avoid neighbours with steering.
- **Perception cone for reactive behavior.** Crowd NPCs notice the player (parkour above them, bumping, drawing weapon) via a perception cone that triggers gasps/scatter animations; this is the layer that makes crowds feel alive versus just visually dense.

### Crowd rendering (GPU side)

- **Instanced GPU skinning.** Ambient and billboard tiers batch into a handful of draw calls per outfit family; per-instance randomisation (colour variant, hat, beard) is done with per-instance constants.
- **Shared animation atlases.** A dozen walk/stop/talk cycles baked as vertex-animation textures or traditional GPU skinning with a shared animation buffer; ambient NPCs pick an animation index at spawn.
- **Cloth LOD.** Full NPCs get PhysX/Havok cloth; mid-tier gets a cheap vertex-shader cloth wave; ambient gets nothing.

### Concrete numbers

- AC Unity Bastille scene: marketing claimed "up to 10,000 NPCs on-screen". The realistic concurrent count was ~5,000, with the recycling pool making it look larger [sourced: Ubisoft marketing + Eurogamer analysis Nov 2014].
- AC Odyssey Athens: Levesque-era routine design; no published concurrent number.
- AC Shadows Kyoto: DF analysis noted "densest Kyoto streets hold hundreds of visible NPCs with minimal pop-in" [sourced: Digital Foundry Shadows review].

## 3. Parkour / traversal system

AC's climb-anywhere parkour is the other genre-defining system. Public info on its implementation is thinner than crowds, but the general design is documented [sourced: GDC 2015 "Animation Bootcamp: AC Unity parkour" with Colin Graham; Ubisoft News AC Unity tech deep dive; Ars Technica AC Unity interview]:

### Climbable-surface detection

- **Collision tagging.** Every climbable edge in an AC city is tagged at level-build time. Artists mark ledges, window sills, rooftop crenellations, tree branches as climbable via metadata on the collision mesh. This is not runtime-derived — it's authored. (Unlike *Uncharted*'s hand-placed anim triggers, AC's tags are dense and geometric.)
- **Forward + up raycasts.** When the player presses run/jump toward a wall, the engine fires forward raycasts from the player's chest + head + an "up" raycast from the reach zone. Hit points with climb tags feed a candidate list.
- **Scoring function.** Candidates are scored by angle vs player forward, height delta (want to grab shoulder-height edges preferentially), and continuity (prefer candidates that lead to another tagged ledge within the next animation window). The highest-scoring candidate is chosen.
- **Contextual input.** Hold-run + up-left stick bias = climb trajectory; release = leap or hang. The input space is 2D (stick + button) but the climb animation state machine has hundreds of transitions keyed by tag metadata.
- **Motion-matched climbs** (Valhalla onward). The climb system was partially fused with motion matching — the engine picks a short climb animation clip whose start/end pose best match the current root + the target grab point [sourced: Ubisoft News Valhalla animation blog; LaForge Motion Matching paper 2020 — see §8].

### Prince of Persia: The Lost Crown (2024)

Lost Crown (Ubisoft Montpellier) runs on Anvil and represents a 2D side-scrolling evolution of the parkour stack [sourced: Ubisoft News Lost Crown postmortem; IGN tech preview]:

- The 2D metroidvania format actually simplifies the grab-detection problem — candidates are in a plane, not a volume. Montpellier used this to push *speed* and *combo fluidity* (wall runs, dash chains, time-loop rewind mechanics) instead of climb variety.
- The engine under the hood is still Anvil, which is notable: it proves Anvil can ship a stylised 2D side-scroller, not just open-world 3D. Compare to the Snowdrop / Mario + Rabbids proof point.
- Rewind mechanic (the "Memory Shards" allowing you to place a time-loop anchor and return) is built on Anvil's determinism / snapshot layer — not publicly documented in detail.

### AC Mirage (2023) — parkour regression

Mirage (Ubisoft Bordeaux) tried to return to AC1-style parkour feel — tighter, more reactive, less flowing. Technically this was tuning of existing Anvil systems (more aggressive candidate scoring for low-height ledges; tighter animation snap to pose) rather than new tech [sourced: Ubisoft News Mirage dev diary; Eurogamer Mirage review].

## 4. Open world — scale and density

Anvil is a systemic open-world engine. Scale table [sourced: various Ubisoft World Map blogs; Digital Foundry map-size comparisons; Reddit r/assassinscreed compiled tables]:

| Title | Year | Approx map area | Density feel | Notable |
|-------|------|-----------------|--------------|---------|
| AC2 | 2009 | ~4 km² (city-based) | Very dense | Renaissance Florence + Venice as separate zones |
| AC Unity | 2014 | ~1 Paris (~3 km²) | Extremely dense | Every Parisian building enterable (not literally; ~1/3 actually) |
| AC Origins | 2017 | ~80 km² | Medium | First "RPG-scale" AC; Egypt |
| AC Odyssey | 2018 | ~256 km² | Medium-high | Ancient Greece; includes sea |
| AC Valhalla | 2020 | ~140 km² | High | England + Norway + fragments of other regions |
| AC Mirage | 2023 | ~8 km² | Very dense | 9th-century Baghdad only; tighter, more AC1-like |
| AC Shadows | 2024 | ~100 km² | High | Feudal Japan (Kyoto, Osaka, countryside, four-season) |

### Streaming

- **Tile-based streaming.** World divided into ~64 m or ~128 m tiles; tiles stream in based on camera distance + predicted direction (parkour tends to aim up + forward — predict accordingly).
- **Priority queues.** Critical geometry (collision, navmesh) streams first; decals, foliage, small props stream later. On PS5/SSD-era (Mirage, Shadows) this is much less visible than on PS4 era (Origins, Odyssey).
- **Interior streaming.** Since AC Unity, interiors are streaming cells rather than separate levels. The trade: navmesh has to bridge the interior/exterior boundary, and occlusion culling has to switch from outdoor portal-free to indoor BSP/portal methods as you cross a doorway.

### Comparison vs RAGE (Rockstar) / RDR2

RAGE (RDR2, 2018) and Anvil (Valhalla, 2020) occupy the same "dense open world" space but with different philosophies [sourced: rage_rockstar.md round 2; Digital Foundry RDR2 vs Valhalla comparisons]:

- **Density**: RDR2's western wilderness is less population-dense but more *reactive* (each NPC has a persistent schedule, weather-reactive dialogue, dynamic hunger). AC's world is more crowded in cities but less individually reactive — the crowd is an aggregate, not a set of persistent persons.
- **Simulation budget**: Rockstar spends more CPU per NPC on fewer NPCs. Ubisoft spends less per NPC on more NPCs.
- **Quest density**: AC (RPG era) has 50–150 sidequests + 500+ activities; RDR2 has ~100 sidequests but each is hand-built cinema. Different values.
- **Weather**: both have first-class dynamic weather; RAGE's wetness accumulation (puddles, mud) is arguably still the gold standard. Anvil's four-season Shadows work (§6) is the reply.
- **Animation**: RAGE's Euphoria (NaturalMotion procedural ragdoll + behavior) is unique; Anvil never adopted Euphoria-style animation and instead went the motion-matching route (§8).

## 5. Stealth AI / detection

Stealth is an AC fundamental; the detection system has evolved across the series [sourced: GDC 2012 "AI in AC: Revelations" (speculative — confirm venue); AC Unity dev blogs; For Honor GDC 2017 combat talks adapting parts of Anvil AI]:

### Cone-of-vision + noise

- **Perception cones**: each enemy NPC has a view cone (typically ~60–90 degrees horizontal, 30–60 degrees vertical) with a graded opacity curve — objects in the center detect faster than objects at the edge.
- **Detection meter**: filling-bar UI driven by a per-target "suspicion" float. Rate of fill is modulated by lighting (darker = slower — revived from Splinter Cell heritage), distance, player speed, silhouette-break (crouched behind a bush counts partial line-of-sight).
- **Noise propagation**: footstep noise radiates as a sphere; ceiling and wall materials attenuate. Running on wood rooftops broadcasts further than sneaking on grass.
- **Alert state machine**: Unaware → Investigating (check last-known-position) → Hunting (search pattern) → Combat → Lost (return to patrol). Each state has distinct animation set and perception tuning (alerted NPCs have longer view cones, faster detection fill).
- **Group perception**: NPCs share detection state within a range; one alerted guard can propagate alert to nearby guards via a "shout" system. Bodies found propagate similarly.

### For Honor adaptation

For Honor (2017, Ubisoft Montreal) is a competitive melee fighter, not stealth — but it reuses parts of Anvil's perception and state machine for its Art of Battle combat [sourced: GDC 2017 "Art of Battle" For Honor design talk with Jason VandenBerghe]:

- The three-stance guard/strike system maps stances to a camera-relative "threat cone" that's descended from the AC vision cone.
- Lock-on targeting uses a variant of the AC stealth line-of-sight math to decide which nearby enemy is the "intended" target.
- The perception/alert state machine gave For Honor its Barbarian/Knight/Samurai AI bots — AC-stealth AI rewired for team-fight coordination rather than patrol.

### Splinter Cell heritage

Splinter Cell's light-based detection predates Anvil; Ubisoft Montreal teams migrated some Splinter Cell devs into AC, and the light-sensitivity of detection in AC1 and later (especially Shadows 2024, which re-emphasised hiding in shadow) traces to that Splinter Cell DNA [sourced: Ubisoft Montreal historical writeups; Shadows 2024 preview coverage].

## 6. Dynamic weather + four-season system

Weather across Anvil [sourced: Ubisoft News Shadows dev blog on seasons; Digital Foundry Shadows Nov 2024 review; Eurogamer Shadows preview Oct 2024]:

### Shared Ubisoft weather tech

There is cross-pollination between Anvil, Snowdrop, and Dunia weather systems [sourced: Ubisoft Engineering blog on shared weather stack; Snowdrop Avatar post — Snowdrop volumetric clouds share R&D with Anvil's Shadows clouds]:

- **Volumetric clouds** with dynamic shape (shared Horizon/Frostbite-style cloud-volume code across the industry — Ubisoft's flavour uses a Perlin-Worley density function + a sun-occlusion sampling pass).
- **Wet-surface BRDF modulation** — when raining, surface roughness drops and specular up; this is a shader-side hack that's consistent across Ubisoft engines.
- **Wind propagation** — a per-region wind vector field that drives foliage, cloth, fabric flags. Shared between Anvil and Dunia.

### AC Shadows four-season system

This is the genuine world-first [sourced: Ubisoft Shadows dev blog; Digital Foundry Shadows review]:

- The Shadows open world persists across four seasons (spring, summer, autumn, winter) with seasons advancing in-game over real play-time (no hard schedule — tied to progression checkpoints).
- Season transitions affect: foliage colour, leaf density, snow accumulation, water surface (freezing ponds in winter), NPC clothing layer, AI behavior (winter has sparser street crowds, different festival routines).
- This is not just a shader swap — it's a world-state re-bake of lighting probes + foliage LODs + navmesh modifications (frozen lake = walkable in winter, swimmable in summer).
- Tech implementation involved: four sets of lightmap + probe data per tile, authored foliage variants keyed by season, AI routine overlays per season. DF called it "one of the most impressive open-world simulation systems in an AAA game to date."
- Caveat: some reviewers noted that season transitions can feel abrupt in practice (snapping rather than smoothly interpolating) — probably because a full re-bake can't be smoothly lerped.

No other AAA open-world engine has shipped a four-season persistent system in 2024 — RAGE (RDR2), Decima (Horizon), and REDengine (Cyberpunk) stick to weather + ToD.

## 7. Ray tracing adoption

Ray tracing timeline in Anvil [sourced: Digital Foundry Shadows, Mirage tech reviews; Ubisoft Shadows engineering blog Oct 2024]:

- **AC Odyssey / Valhalla (2018, 2020)**: rasterization only; SSR + baked GI + screen-space shadows. No RT despite PC RT hardware being available by Valhalla's launch.
- **AC Mirage (2023)**: rasterization-first with some PC-only RT reflections option. Not a baseline.
- **AC Shadows (2024)**: **RT GI baseline** on PS5, Series X, PC. This is the big shift — no rasterization-only fallback path on current-gen consoles. The title reportedly would not render correctly without the RT GI pass [sourced: Ubisoft Shadows engineering blog; DF review].
- **Implementation**: Shadows uses a hybrid RT GI — screen-space first pass for nearby irradiance, then world-space RT probes queried at larger distance, with DDGI-style temporal stability. Reflections on RT also baseline (planar-probe fallback on Series S).
- **Performance**: Shadows targets 30 fps on Series S with RT GI + RT reflections on; PS5 and Series X hit 40 fps in performance mode with dynamic res, 30 fps in quality mode at native 4K. PC with RTX 4080+ pushes path-traced mode (PC-only, in a later patch) [sourced: DF review Nov 2024; PC Gamer benchmarks].

### Why the jump at Shadows specifically

The four-season world + dense Kyoto crowd + open-world scale meant baking lighting was becoming untenable — four seasons × ten lighting times of day × hundreds of tiles = thousands of bakes to maintain. RT GI eliminates that pipeline cost. The team traded **bake time at content-authoring stage** for **GPU time at runtime**. The same argument Snowdrop made for Avatar [sourced: massive.se ray tracing post; parallel to Ubisoft Shadows engineering blog].

## 8. Animation — motion matching + LaForge research

This is the section where Ubisoft's public research is most useful to ALZE. Ubisoft LaForge is Ubisoft's Montreal-based ML research lab [sourced: LaForge website montreal.ubisoft.com/en/our-engagements/research-and-development/].

### Motion-matching foundations

Classic motion matching [sourced: Kovar+Gleicher SIGGRAPH 2002 "Motion Graphs"; Simon Clavet GDC 2016 "Motion Matching and The Road to Next-Gen Animation" — the Ubisoft For Honor talk that kicked off industry adoption]:

- You record a large animation database (tens of thousands of frames of mocap).
- Each frame has a feature vector: foot trajectory over next N frames, hip velocity, facing, etc.
- At runtime, for each animation "step" (~0.2 s), query the nearest-neighbour frame in the DB whose feature vector best matches the desired pose trajectory (future root, gamepad input prediction, constraints).
- Play the retrieved clip for one step, re-query, repeat.
- Result: animation that naturally blends mocap clips without hand-authored state machines.

Simon Clavet's GDC 2016 For Honor talk is the canonical introduction. For Honor shipped with motion matching for its combat-out-of-combat transitions. AC Odyssey (2018) and especially Valhalla (2020) used motion matching more broadly across locomotion.

### Phase-Functioned Neural Networks (Holden et al 2017)

Daniel Holden + Taku Komura + Jun Saito, "Phase-Functioned Neural Networks for Character Control", SIGGRAPH 2017 [sourced: theorangeduck.com/page/phase-functioned-neural-networks-character-control; ACM SIGGRAPH 2017 proceedings].

- Holden was at Edinburgh (with Komura) for this paper, joined Ubisoft LaForge afterward.
- Idea: replace the nearest-neighbour DB lookup with a neural network whose weights depend on a cyclic "phase" variable (0→1 around each gait cycle). The network predicts the next pose given input direction + phase + current pose.
- Trained on ~1.5 hours of locomotion mocap.
- Result: small (~10 MB) model can generate unlimited locomotion variety including uneven-terrain adaptation — without the DB memory footprint of classical motion matching.
- This paper is hugely influential; Holden's page + codebase are public with PyTorch impl.

### Learned Motion Matching (Holden et al 2020)

Daniel Holden + Orestis Kanter + Maksym Perepichka + Jun Saito, "Learned Motion Matching", SIGGRAPH 2020, Ubisoft LaForge [sourced: theorangeduck.com/page/learned-motion-matching; Holden+Kanter+Saito ACM SIGGRAPH 2020; Ubisoft LaForge research catalog].

- Direct response to classical motion matching's memory cost — a large mocap DB is GB-scale, not shippable at scale on consoles with tight memory budgets.
- Three networks compress the DB:
  1. **Decompressor** — from feature vector + latent code, reconstruct a full pose.
  2. **Stepper** — given current latent code, predict latent code for next frame (so most frames don't need a DB lookup).
  3. **Projector** — when an input change (player turned) requires a new clip, map the feature vector to the nearest latent code (a learned k-NN).
- Together the three networks replace the DB: 3 × ~10 MB networks can stand in for a 500 MB mocap database with ~imperceptible quality loss.
- The paper explicitly targets AAA games; it's presented as an industrial engineering result, not just research.
- Deployed in AC Valhalla (confirmed in talk) and Watch Dogs: Legion (in-engine mentions). Shadows 2024 likely uses a further evolution [sourced: Ubisoft LaForge blog; **inferred for Shadows** — no explicit confirmation yet at time of writing].

### Related LaForge papers

- **"Recurrent Transition Networks for Character Locomotion"** — Yi Zhou et al, Ubisoft LaForge, 2018/2019. Transitioning between mocap clips via RNN. Referenced in the task prompt, attributed to Holden/Saito/Komura — actually the Zhou paper; but Holden et al have related 2016 work on "Fast Neural Style Transfer for Motion Data" [sourced: theorangeduck.com archive].
- **"DReCon: Data-Driven Responsive Control of Physics-Based Characters"** — Kevin Bergamin et al, Ubisoft LaForge + McGill, SIGGRAPH Asia 2019. Physics-based character locomotion driven by motion-matching DB + RL controller. Deployed in Rainbow Six Siege's rappel system and various character animations [sourced: ACM SIGGRAPH Asia 2019; Ubisoft LaForge].
- **"Character Controllers using Motion VAEs"** — Hung Yu Ling + Fabio Zinno + George Cheng + Michiel van de Panne, 2020 SIGGRAPH, EA + UBC. Parallel research line; Ubisoft LaForge cites it alongside their own.

### Applicability of the research to ALZE

The LaForge papers are **the most stealable public research from any AAA Ubisoft team**. Reasons:

- Each paper has clean math and reference PyTorch code.
- The networks are small (10s of MB), runnable at 60 Hz on CPU even without GPU ML accel.
- The motion DB can be mocap you record yourself on a mid-range rig, or crowd-sourced (Mixamo).
- No engine-specific runtime — they're C++ math + small tensor ops.

See §11 table for concrete stealability.

## 9. Tools + DCC pipeline

Anvil's tool stack is less publicly documented than Snowdrop's (Ubisoft Montreal doesn't publish a "history of Anvil" blog like Massive did). What's known [sourced: GDC 2015 "Building Paris" AC Unity talk; Ubisoft Montreal job postings; Houdini case studies houdini.sidefx.com; personal blog posts from ex-Ubisoft tech artists]:

### Internal tools

- **Frag** — Ubisoft's internal asset pipeline tool (inferred from job postings; details opaque).
- **Crafter** (speculated) — a procedural level-building tool mentioned in some AC Unity retrospectives; **low confidence on the exact name**.
- **Anvil Editor** — the main content editor. Not a unified-graph-everything tool like Snowdrop's — more traditional (scene hierarchy tree, per-object inspector, some graph tools for specific systems like quest scripting and materials).
- **Quest / mission editor** — graph-based; used by narrative designers to script Odyssey/Valhalla's quest tree with dialogue branching.
- **Crowd / routine editor** — authoring NPC routines (see §2); mostly data-table-driven with a timeline view rather than a graph.

### DCC integration

- **Maya** — primary character art + animation DCC. Ubisoft Montreal's animation team pipelines to Anvil via a Maya plugin suite (Art-to-Engine, internal name).
- **Houdini** — used for procedural world generation (especially Odyssey's Greek terrain, Valhalla's England, Shadows' Japan). Houdini Engine integration for in-editor reproduction of Houdini networks. SideFX has a published case study on AC Odyssey terrain [sourced: sidefx.com Odyssey case study].
- **Substance Painter / Designer** — the Substance suite is near-universal for PBR authoring at Ubisoft.
- **ZBrush** — character sculpt.
- **In-house content-paint tools** — Ubisoft has internal tools for vegetation painting, vertex paint, decal placement. Names not public.

### Pipeline patterns

- Asset is authored in DCC, exported via a custom plugin to an intermediate format, processed through a cooker that bakes to Anvil's runtime format.
- Perforce is the version control backbone (same as most AAA; confirmed in Ubisoft postings).
- Cross-studio asset sharing goes through a Ubisoft-wide Perforce cluster with inter-studio depot policies.

The pipeline is less designer-friendly than Snowdrop's graph-first model — Anvil retains more "programmer in the middle" friction. This is a consistent critique of the engine (see §12).

## 10. Cross-studio collaboration

AC games regularly list 10–15 Ubisoft studios in credits. AC Valhalla credits list ~14 studios (Montreal lead, Quebec, Sofia, Singapore, Kyiv, Bordeaux, Bucharest, Pune, Chengdu, Shanghai, etc). Skull and Bones (2024) listed Singapore as lead with support from several others.

### How Anvil facilitates this

Based on public Ubisoft postings + GDC talks + ex-Ubisoft blog posts [sourced: Ubisoft Careers pages; GDC 2019 AC Odyssey production talk]:

- **Standardised asset format.** All studios export to the same Anvil intermediate format; the cooker is centralised at Montreal.
- **Shared trunk / branch strategy.** A primary Anvil trunk lives at Montreal; studios branch for their own titles and merge improvements back.
- **"Satellite studio" pattern.** Smaller studios (Pune, Chengdu) often own specific content verticals — quest content for a region, crowd asset creation, cinematic cleanup — rather than engine subsystems.
- **Engine subsystems owned distributed.** Naval is Singapore. Parkour animation is Montreal. Motion matching research is LaForge (Montreal). Weather is partially shared across Montreal/Quebec.
- **Common internal tools.** Frag / Anvil Editor / Maya plugin suite are installed studio-wide; a new studio joining an AC production does not have to rebuild tooling.

### Comparison to Frostbite (EA)

EA's Frostbite has a decade-long cross-studio history: DICE (origin, Battlefield), Bioware (Dragon Age, Mass Effect), Motive, Ghost Games, EA Canada [sourced: EA engineering posts; "Frostbite and RPGs" GDC 2018 Bioware talk]:

- Frostbite's reputation is that non-DICE studios (especially Bioware) struggled to adapt Frostbite's shooter-oriented core to RPG (Anthem, Andromeda). Frostbite's cross-studio story is *painful in practice* — tools that work at DICE don't always work at non-shooter studios.
- Anvil's story is *relatively* smoother. AC is already systemic-open-world at every studio; the genre fit is consistent. When Ubisoft tries to push Anvil into adjacent genres (For Honor's melee, R6 Siege's tactical shooter, Skull and Bones' ship MMO) the friction emerges — R6 Siege's branch diverged so far from Anvil trunk that it's effectively a separate codebase [sourced: Ubisoft engineering discussion around R6; **inferred in part**].
- **Lesson for multi-project engines**: a single engine across many genres requires either genre-specific forks (Ubisoft's pragmatic approach: Anvil fork for R6 Siege, Dunia fork for Far Cry's CryEngine heritage) or very strong abstraction layers (UE5's approach: generic enough to run almost anything). Frostbite sat in the middle and suffered.

## 11. "Anvil tech per title" summary table

| Title | Year | Crowds | RT | Anim | Weather | World size |
|-------|------|--------|-----|------|---------|-----------|
| AC2 | 2009 | ~100 visible | none | state-machine | baked ToD | ~4 km² city |
| AC3 | 2012 | ~200 visible | none | state-machine + naval | dynamic (AnvilNext 1st) | ~40 km² frontier+cities |
| AC Unity | 2014 | ~5k recycled | none | state-machine | dynamic | ~3 km² (dense Paris) |
| AC Syndicate | 2015 | ~3k | none | state-machine | dynamic | ~4 km² London |
| AC Origins | 2017 | ~1k | none | motion match (basic) | dynamic | ~80 km² |
| AC Odyssey | 2018 | ~1k | none | motion match | dynamic + sea | ~256 km² (incl sea) |
| AC Valhalla | 2020 | ~1k | none | learned MM | dynamic | ~140 km² |
| AC Mirage | 2023 | ~2k dense | optional RTR (PC) | motion match | dynamic | ~8 km² |
| AC Shadows | 2024 | ~2k dense | **RT GI baseline** | learned MM (gen2?) | dynamic + 4 seasons | ~100 km² |

(Values approximate; sources: Eurogamer map comparisons, DF reviews, Ubisoft marketing.)

## 12. ALZE applicability — what's actually stealable

| Anvil / LaForge technique | Stealable for ALZE? | Cost | Priority |
|---------------------------|---------------------|------|----------|
| **LaForge Learned Motion Matching** (Holden 2020) | **YES** — papers + PyTorch repo public, runs on CPU, ~30 MB model | Days of implementation for a working prototype | **HIGH** — biggest win for character quality vs effort |
| **Phase-Functioned Neural Networks** (Holden 2017) | YES — simpler than LMM, good starting point | 1-2 days prototype | HIGH — implement before LMM as a warm-up |
| **Crowd LOD tiers + recycling pool** | YES conceptually — the recycling idea is language-agnostic and not engine-specific | ~1 week for a basic 3-tier crowd | MED — nice-to-have but ALZE isn't crowd-heavy |
| **Perception cone + alert state machine for AI** | YES — textbook AI, dozens of open implementations | 2-3 days for a polished version | MED |
| **Climbable-surface tagging** for parkour | YES — works with a tagged collision mesh; doesn't require engine-specific features | Depends on game design need | LOW unless ALZE has a parkour game |
| **Four-season world state re-bake** | NO — content pipeline cost too high for a small team | Months of artist time + 4x lightmap storage | LOW |
| **Mass-crowd GPU instancing** (thousands of NPCs) | PARTIAL — instancing is standard; the *scale* isn't | 1-2 weeks for a batched-instance renderer | LOW-MED |
| **Anvil Editor-style unified scene editor** | NO — too big; pick a narrower scope (Dear ImGui-based inspector) | Years | N/A |
| **Cross-studio engine-sharing infrastructure** | N/A — ALZE is solo/small team | N/A | N/A |
| **RT GI baseline** | NO (at v1 OpenGL 3.3) — MAYBE v2 (Vulkan) — YES (v3 aspirational with RT HW) | 2-3 weeks once prereqs exist | deferred |
| **Dynamic weather + wet BRDF** | YES — straightforward shader + parameter system | 1 week | MED |
| **Motion-matched parkour** | Contingent on ALZE adopting motion matching first | Days after MM exists | LOW |
| **Houdini procedural terrain** | YES via Houdini Engine indie license | Days of integration once Houdini Engine is set up | MED if ALZE needs big terrain |

### Priority ranking for ALZE short term

1. **Implement Phase-Functioned Neural Networks** as a learning exercise — it's the single most pedagogically clean LaForge paper.
2. **Upgrade to Learned Motion Matching** once PFNN is working. The 3-network decomposition is the real production trick.
3. **Add perception cones + alert state machines** as a generic AI building block.
4. **Don't try to ship 5,000 crowd NPCs** — the production budget isn't there. A tier-3 crowd (50 visible, 200 recycled pool) is plenty for a solo/small-team project.
5. **Defer RT GI** until the base renderer is in Vulkan — it's not meaningful on GL 3.3.

## 13. Primary references

| Author | Year | Venue | Reference |
|--------|------|-------|-----------|
| Jean-François Lévesque | 2018 | GDC | "Massive Crowd on AC Unity: AI Recycling" — gdcvault.com / slideshare |
| Jean-François Lévesque | 2019 | GDC | "Crowd Rendering on AC Unity" (follow-up) |
| Simon Clavet | 2016 | GDC | "Motion Matching and The Road to Next-Gen Animation" — For Honor — gdcvault.com |
| Daniel Holden, Taku Komura, Jun Saito | 2017 | SIGGRAPH | "Phase-Functioned Neural Networks for Character Control" — theorangeduck.com + ACM DL |
| Daniel Holden, Orestis Kanter, Maksym Perepichka, Jun Saito | 2020 | SIGGRAPH | "Learned Motion Matching" — Ubisoft LaForge — theorangeduck.com + ACM DL |
| Kevin Bergamin et al | 2019 | SIGGRAPH Asia | "DReCon: Data-Driven Responsive Control of Physics-Based Characters" — ACM DL |
| Yi Zhou et al | 2018 | — | "Recurrent Transition Networks for Character Locomotion" — arxiv + LaForge |
| Colin Graham | 2015 | GDC | "Animation Bootcamp: AC Unity parkour" — gdcvault.com |
| Jason VandenBerghe | 2017 | GDC | "For Honor: Designing Art of Battle" — gdcvault.com |
| Digital Foundry | 2024 Nov | Eurogamer | "AC Shadows tech review" — eurogamer.net/digitalfoundry |
| Digital Foundry | 2023 | Eurogamer | "AC Mirage tech review" |
| Digital Foundry | 2014 | Eurogamer | "AC Unity tech review" |
| Ubisoft News | 2024 | news.ubisoft.com | "AC Shadows engineering — seasons, RT GI" |
| Ubisoft LaForge | ongoing | montreal.ubisoft.com | LaForge research catalog |
| SideFX | — | houdini.sidefx.com | "AC Odyssey procedural terrain case study" |
| Wikipedia | — | en.wikipedia.org | "Anvil (game engine)" |

Archive fallback: for any of the above, archive.org/web captures at year-end snapshots typically exist.

## 14. Honest closing note

Ubisoft Montreal's **engine** is a 20-year-old, 14-studio-contributed, multi-million-LOC proprietary beast. ALZE will never reproduce it and shouldn't try.

What ALZE **can** take from Ubisoft is the **LaForge research output**, which is the cleanest AAA-studio public research catalogue in the industry. The Phase-Functioned Neural Networks and Learned Motion Matching papers in particular:

- Have clean math and reference code.
- Solve an actual animation quality problem (locomotion feel) that ALZE will hit.
- Are small (tens of MB models), runnable without GPU ML hardware.
- Are unencumbered by proprietary engine coupling — they're general character-animation techniques, not Anvil-specific systems.

The recipe is:

1. Read `theorangeduck.com/page/learned-motion-matching` end to end.
2. Clone Holden's public repos.
3. Record or download ~30 minutes of locomotion mocap.
4. Prototype PFNN, then LMM.
5. Bolt the result onto ALZE's character controller.

That path gives ALZE AAA-comparable locomotion quality for weeks of engineering, not years. It's the single best "steal" from the Ubisoft corpus.

Everything else in Anvil — the crowd scale, the four-season world, the 256 km² maps, the RT GI baseline, the cross-studio pipeline — is **not** stealable for a small team. Admire it, study it, and move on. The research is the gift; the engine is the trap.

*Companion files: `snowdrop_ubisoft.md` (round 2) for the sister Ubisoft engine; `r5/animation.md` (upcoming) for the broader motion-matching landscape.*
