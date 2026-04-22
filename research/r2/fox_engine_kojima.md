# Fox Engine (Kojima Productions / Konami, 2010-2018)

> "The concept of the Fox Engine is photo-realism." - Hideo Kojima, GDC 2013
>
> A case study in what happens when a proprietary AAA engine is tied more to a person and a team than to a parent company. Fox Engine was one of the most technically impressive engines of the PS3/360->PS4/Xbox One transition - and it is now effectively dead.

## Overview

- **2008**: Internal R&D begins at Kojima Productions after the release of Metal Gear Solid 4. Stated goal by Hideo Kojima: "the best engine in the world". Also framed as the studio's move off single-platform (PS3-only) into multi-platform production.
- **June 3, 2011**: Publicly revealed at Konami's pre-E3 press conference as a codenamed "Fox Engine" tech demo (jungle scene, man + horse + dog), assets derived from what became Metal Gear Solid V.
- **Aug/Dec 2011**: Kojima posts facial and cloth-transparency tests on Twitter.
- **March 2012**: "Classified" lighting samples - photo-real staff-room recreation.
- **June 2012**: Confirmed running on PS3, Xbox 360 and PC. Target was explicitly the outgoing generation, not next-gen only.
- **September 2013**: First shipped title - **Pro Evolution Soccer 2014**.
- **March 2014**: **Metal Gear Solid V: Ground Zeroes** (PS3/360/PS4/XBO/PC) - Fox Engine's "calling card" for Kojima Productions.
- **September 2015**: **Metal Gear Solid V: The Phantom Pain** - the magnum opus.
- **Late 2015**: Kojima/Konami split. Kojima Productions dissolved as a Konami subsidiary in December 2015 and re-founded as an independent studio with Sony.
- **2018**: **Metal Gear Survive** (last Metal Gear shipped on Fox Engine, internal team without Kojima).
- **2021**: **PES 2021** - last Fox Engine PES.
- **2021->2023**: Konami transitions eFootball onto Unreal Engine 4, then gradually Unreal Engine 5. Fox Engine essentially dormant.
- **2026**: Fox Engine is a historical footnote; new Metal Gear remakes (e.g. Metal Gear Solid Delta: Snake Eater) are being built on Unreal Engine 5, not Fox.

Hideo Kojima was the tech champion - the public face, the producer who greenlit an expensive in-house engine rebuild instead of licensing Unreal. Around him the named Fox Engine leads were **Junji Tago** (technical director), **Hideki Sasaki** (CG / art director), and **Masayuki Suzuki** (lighting artist / lead) - the trio that gave the GDC 2013 talk. Long-time Kojima collaborator **Yoji Shinkawa** (art director, character / mech concepts) was not an engine engineer but his art sensibility defined what the engine needed to render. Producer **Kenichiro Imaizumi** handled business. When the split happened, Kojima, Shinkawa and Imaizumi left together to found the new (Sony-backed) Kojima Productions; many engine engineers stayed at Konami or scattered to other studios.

## Technical breakthroughs for its era

Fox Engine's big bet was **pushing a next-gen-looking deferred PBR pipeline onto PS3/Xbox 360** as a baseline - at a time when most Western AAA engines (Frostbite 3, CryEngine 3, id Tech 6) were going HDR+deferred *on* PS4/XBO as the primary target. Fox had to do it on 256-512 MB of RAM and last-gen GPUs, then scale up.

Concretely:

- **Deferred shading with lightweight G-buffer.** Four B8G8R8A8 render targets: albedo+opacity, normal+view-dependent roughness, specular (roughness/intensity/materialID/SSS-translucency), and reversed-Z 32-bit depth. Compact by design to fit last-gen memory bandwidth.
- **Physically-based materials**, view-dependent surface roughness, material-ID-driven shading branches.
- **Baked spherical-harmonic GI** (9 coefficients per zone) regenerated per frame against the camera's current zone - cheap enough for PS3, good enough to pass for dynamic.
- **4K x 4K shadow maps per shadow-casting light**.
- **Dual SSAO** (Line Integral SSAO at half-res + Scalable Ambient Obscurance 11-tap) bilaterally upscaled.
- **Screen-space reflections** at half resolution, 4-tap depth collision.
- **Sprite-scatter DoF** at 1/2, 1/4, 1/8, 1/16 res to manage overdraw on last-gen GPUs.
- **3D LUT color grading** (16^3) with trilinear interpolation.
- **Procedural sky with volumetric clouds** (see below).
- **Real-time dynamic weather** feeding **four pre-baked cubemap probes** per zone (sunny / cloudy / rainy / stormy), blended by current weather state.
- **Hair cards + anisotropic shading** that held up to cinematic cutscene framing without pre-baked Maya hair.

The volumetric cloud work in particular is historically important: MGS V's sky was one of the first shipped AAA implementations of ray-marched, noise-driven volumetric clouds cheap enough to run in an open-world game loop. Guerrilla's **Nubis** system (Andrew Schneider, SIGGRAPH 2015 "The Real-Time Volumetric Cloudscapes of Horizon Zero Dawn") made the technique the industry standard - and MGS V, arriving the same year, is part of that cohort that taught everyone post-2015 how to do procedural clouds well.

## MGS V: The Phantom Pain (2015) rendering

One codebase, one engine, two huge open-world biomes:

- **Afghanistan** - arid high-altitude desert, hard rim lighting, dust haze, rocky escarpments, sandstorm weather pattern.
- **Africa (Angola-Zaire border)** - lush tropical forest, heavy vegetation, tropical downpours, humidity fog.
- **Mother Base / Camp Omega** - modular player-built oil-rig platforms at sea, rebuilt/expanded across the campaign - rendered with the same lighting stack as open-world.

All four environments share the same shader graph, same material system, same SH irradiance + cubemap probe pipeline. Art-direction drove the difference, not engine forks.

Gameplay-relevant rendering systems:

- **Full 24h time-of-day cycle** with continuous sun/moon, probes regenerated per TOD bucket.
- **Dynamic weather state machine** (sunny / cloudy / rainy / sandstorm / fog / pouring) blended smoothly.
- **Weather as first-class gameplay signal**: sandstorm reduces AI sight radius but keeps binocular-tagged enemies visible (player's tactical edge); rain masks footstep sound; fog breaks line-of-sight; you can call down a supply drop of a weather-modification signal too.
- **Lighting changes patrol behaviour** via shared perception systems - dawn/dusk guards patrol differently, flashlights cast real shadow cones.
- Performance envelope: PS4/XBO ran near-flawless 60 fps with V-sync in Digital Foundry tests; Xbox One at 1600x900, PS4 at 1920x1080. For an open-world game in 2015 this was exceptional.

Hospital prologue scene budget per frame (per Courrèges' graphics study): **2,331 draw calls, 623 textures, 73 render targets**. Tight by 2015 standards.

## Photogrammetry pioneer

MGS V is one of the earliest AAA games to build a hero-actor around full-face photogrammetry + performance capture:

- **Kiefer Sutherland** was scanned for Venom Snake / Big Boss face, then performance-captured for voice and facial animation. Sutherland stated this was his first facial capture for a videogame.
- Fox Engine's art pipeline was explicitly built around **3D photogrammetry, laser capture and HDR photography** as primary material-authoring inputs - not hand-painted textures.
- The pipeline: **hero actor scan -> cleanup to neutral pose -> blendshape library -> in-engine rig + cloth -> performance-capture driven** - prefigures what RE Engine (Capcom, 2017), Epic's MetaHuman framework (2021+) and The Coalition's Gears 5 face scanning would standardise.
- The **view-dependent roughness** term in Fox's G-buffer was specifically engineered to make scanned-pore skin read correctly at cutscene distances and at gameplay distances without re-authoring - a subtle but important insight.

For 2014-2015 this was cutting-edge. Most AAA studios were still hand-sculpting hero characters.

## PES / eFootball usage

Sports is a different rendering problem - tight stadium volume, 22 hero actors in view, crowd of ~50k, bright sunlit or floodlit pitch - and Fox adapted well:

- **Player-likeness photogrammetry** for top-tier players (real scanned faces).
- **Real-time stadium lighting** with unified source feeding players, crowd and pitch ("all graphical features in-game from players, to crowd and stadium now benefit from a single source of real-time lighting") - unusual for sports games of the era which often separated lighting models per layer.
- **Volumetric atmospherics** in stadium air (sun shafts through open roofs, floodlight god-rays).
- **Grass shading with wetness** - rain actually altered pitch behaviour and ball roll, not just texture swap.
- **Crowd rendering** - impostor-based but lit from the same probe stack.
- **PES 2014 (Sept 2013)** was Fox Engine's first shipped product. Ran through **PES 2021**. Seven consecutive PES years on the same engine - longer than MGS's run.

## Post-Kojima decline

The split happened in December 2015. Konami kept the engine and the rights; Kojima kept the directorial vision and his core inner circle. What followed:

- **Metal Gear Survive** (2018) shipped on Fox Engine, developed by the remaining Konami team. Commercially and critically lukewarm. No new mainline Metal Gear followed.
- **PES 2022 -> eFootball 2022** (Sept 2021) was the transition title. Konami pivoted eFootball to a free-to-play service model on **Unreal Engine 4**. Launch was disastrous on fan and press reception (rushed, missing features, visual regression).
- Reports and community tracking (incl. eFootball leakers on X/Twitter in 2025-2026) indicate eFootball is still a hybrid - Fox Engine elements + UE4 - with a **full Unreal Engine 5 port targeted for eFootball 2027**.
- **Metal Gear Solid Delta: Snake Eater** (Konami's own MGS3 remake) shipped on **Unreal Engine 5**, not Fox - a tacit admission that Fox Engine is no longer a viable platform, even inside Konami.
- The Fox Engine team dispersed. Many engineers either followed Kojima indirectly (via consulting / referrals), moved to Western studios, or left games entirely. Konami never rebuilt the tech leadership around Fox.

Result: an engine that shipped ~11 titles in seven years is effectively abandoned a decade after its public debut.

## Why Kojima didn't take it

Three reasons, in rough order of how often each is cited:

1. **IP / contractual.** Fox Engine was developed by Kojima Productions *as a Konami internal division*. That makes it Konami property. When KP was dissolved in 2015 and Kojima founded a new independent studio, Konami kept the engine, the Metal Gear brand, and all the tooling. Kojima left with approximately zero code.
2. **Kojima's deliberate choice - partnership over ownership.** After the split he went on a "tech tour" visiting Western studios. Guerrilla Games famously gifted Kojima a **USB stick with the full source of what would become the Decima Engine**, with essentially no strings attached except one condition: co-develop it. Kojima accepted. For ~6 months the code was passed back and forth between Amsterdam and Tokyo until the two codebases merged. The engine was named **Decima** after the Japanese island of Dejima, the 17th-century Dutch trading post - honouring the Netherlands-Japan partnership.
3. **Maintenance burden.** Building and maintaining an engine is a 50+ engineer, multi-year cost centre. Kojima's new studio was ~60-100 people for Death Stranding. Taking Decima from Guerrilla meant Kojima got a production-grade, Horizon-Zero-Dawn-proven engine without paying the maintenance bill alone. Decima powered **Death Stranding (2019)** and **Death Stranding 2: On the Beach (2025)**. It was the right call.

## En qué fue bueno

- **Production value per watt.** Fox Engine extracted photo-real-adjacent visuals out of PS3/360 hardware. Nobody else was doing dynamic-weather-plus-PBR-plus-SH-GI on last-gen at 60 fps.
- **Lighting stack.** Probe-per-weather-state cubemaps blended with SH GI was an elegant compromise between baked quality and dynamic TOD response.
- **Photogrammetry pipeline** - hero-actor scan-to-game workflow was ahead of the industry in 2013-2014.
- **Author tools.** Kojima's tooling was repeatedly praised by people who used it - the Fox editor, animation graph, cutscene tool, Mother Base modular builder all got specific positive mentions. Kojima historically over-invests in editor UX; it shows.
- **Cross-genre port.** The same engine ran stealth-action open-world (MGS V) and competitive sports (PES) - few engines of the era managed both genres at AAA quality without significant re-platforming.
- **Hair and cloth**. MGS V hair and Quiet's cloth simulation held up for a decade.

## En qué falló

- **Bus-factor catastrophe.** The engine walked out the door with its champion, spiritually if not legally. Without Kojima, Sasaki, Tago and the senior leads aligned on a roadmap, Konami could not evolve it.
- **Zero licensing diversification.** Fox Engine was never licensed to any third party. There was no external community, no forum, no documentation for outsiders. When the internal team scattered, there was no one else who understood the codebase.
- **Tied to a single IP holder** which was also the one in the divorce. Konami controlled the engine *and* the brand (Metal Gear). A founder-departure scenario left the engine technically alive but institutionally orphaned.
- **Post-hero-engineer decay.** Metal Gear Survive and later PES titles visibly regressed. Fans noticed and said so publicly. The engine wasn't getting new investment; it was being kept on life support.
- **No next-gen roadmap.** There was never a shipped Fox Engine title designed for PS5 / Xbox Series X native. That alone effectively ended it as a AAA competitor by 2020-2021.
- **Documentation gap.** What existed was internal, Japanese-language, tribal knowledge. When the tribe dispersed, knowledge evaporated.

## Lección del case study

Proprietary engines are only as durable as the team that champions them. If your two or three senior engineers leave - or your charismatic producer-tech-champion leaves - the engine often dies within 3-5 years, even if the parent company formally retains it.

For ALZE (small team, proprietary tech):

- **Write code for 10-year maintenance by strangers, not for 3-year iteration by the original team.** Document every non-obvious decision. Comment the "why", not the "what".
- **No single point of failure engineer.** Every subsystem should have at least two people who could defend it in a design review.
- **Publish architecture docs externally** (blog, talks, SIGGRAPH-style writeups). The act of writing for outsiders forces clarity and creates searchable institutional memory.
- **Succession plan for every senior role.** If your rendering lead quit tomorrow, who owns the pipeline? If you can't name them, that is your top risk.
- **Separate engine IP from game IP where possible.** Fox Engine was fatally entangled with Metal Gear. Decima was not entangled with Horizon - so Guerrilla could gift it to Kojima. That flexibility is an asset.

## Qué podríamos copiar (mecanismo concreto) para ALZE Engine

1. **Volumetric cloud rendering.** Noise-based ray-marched clouds shading in 1-2 ms on constrained GPU. The MGS V sky plus the Schneider/Guerrilla SIGGRAPH 2015 Horizon Zero Dawn paper ("The Real-Time Volumetric Cloudscapes of Horizon Zero Dawn") is still the canonical reference. Add Nubis Evolved (Horizon Forbidden West) for fly-through clouds.
2. **Dynamic TOD + weather as first-class gameplay signal.** Not a cosmetic layer - a state that AI perception, audio masking, material wetness, pitch-slide physics, and visibility all subscribe to. One event bus, many listeners. MGS V showed this is the difference between "weather" and "weather that matters".
3. **Four-probe-per-zone lighting cache.** Pre-bake cubemap probes for N weather states (sunny / cloudy / rainy / stormy) per zone, blend at runtime. Cheap, ships easily on constrained hardware, avoids full real-time GI cost.
4. **Photogrammetry hero pipeline.** Hero actor scan -> cleanup to neutral -> blendshape library -> in-engine rig -> performance-capture retarget. Cheaper per hero than it looks, and the visual ceiling is much higher than stylised sculpting. MetaHuman is now the commodity version of this.
5. **HDR + deferred on constrained hardware.** Fox did this on PS3 / 360. If ALZE wants Steam Deck / Switch 2 / web / low-end PC viability, a compact 4-RT G-buffer (128 bits/pixel) with view-dependent roughness is still a very solid baseline in 2026.
6. **View-dependent roughness**. Cheap trick, big payoff for scanned-skin / fabric - put it in the normal G-buffer alpha.

## Qué NO copiar

- **Hero-engineer dependency without succession planning.** Do not let one person be the only one who understands the renderer. Ever.
- **Custom engine + custom IP + single publisher.** If the same entity owns the engine, the IP and the studio, any founder/publisher dispute locks everything. Separate these.
- **Zero external documentation.** Fox Engine's knowledge lived in one office, in one language, in a handful of heads. ALZE cannot afford that.
- **No licensing strategy.** Even if you never intend to license externally, write as if you might. It disciplines the code and creates optionality if the company pivots.
- **Skipping a next-gen / next-platform roadmap.** Fox had no shipped PS5 title. Engines that miss a platform generation do not survive it. Budget for the next platform before the current one ships.
- **Investing in author-tools UX but not in engineer-onboarding UX.** Fox's editors were great *for Kojima's team*. They were impenetrable to outsiders. Both matter.

## Fuentes consultadas

- [Fox Engine - Wikipedia](https://en.wikipedia.org/wiki/Fox_Engine)
- [Metal Gear Solid V - Graphics Study, Adrian Courrèges (2017)](https://www.adriancourreges.com/blog/2017/12/15/mgs-v-graphics-study/)
- [GDC Vault - "Photorealism Through the Eyes of a FOX: The Core of Metal Gear Solid Ground Zeroes" (GDC 2013, Kojima Productions)](https://www.gdcvault.com/play/1031807/Photorealism-Through-the-Eyes-of)
- [Internet Archive - GDC 2013 Kojima Fox Engine talk](https://archive.org/details/GDC2013Kojima)
- [MGS V Graphics & Performance Guide - NVIDIA GeForce](https://www.nvidia.com/en-us/geforce/news/metal-gear-solid-v-the-phantom-pain-graphics-and-performance-guide/)
- [Fox Engine - PCGamingWiki](https://www.pcgamingwiki.com/wiki/Engine:Fox_Engine)
- [Fox Engine - Metal Gear Wiki](https://metalgear.fandom.com/wiki/Fox_Engine)
- [Metal Gear Solid V: The Phantom Pain - Wikipedia](https://en.wikipedia.org/wiki/Metal_Gear_Solid_V:_The_Phantom_Pain)
- [CBR - "Metal Gear Solid: Here's What Happened to Konami's DOOMED Game Engine"](https://www.cbr.com/metal-gear-solid-konami-doomed-game-engine-fox-engine/)
- [ScreenRant - "Hideo Kojima's Fox Engine Abandoned By Konami For Unreal Engine 5"](https://screenrant.com/hideo-kojima-metal-gear-fox-engine-konami-unreal/)
- [GameRant - "Konami Has Scrapped Hideo Kojima's Fox Engine"](https://gamerant.com/konami-scraps-hideo-kojima-fox-engine/)
- [Decima (game engine) - Wikipedia](https://en.wikipedia.org/wiki/Decima_(game_engine))
- [Decima - Death Stranding Wiki](https://deathstranding.fandom.com/wiki/Decima)
- [PlayStation.Blog - "The Hideo Kojima Death Stranding interview: Strands, Decima and Guerrilla Games" (2017)](https://blog.playstation.com/2017/02/23/the-hideo-kojima-death-stranding-interview-strands-decima-and-guerrilla-games/)
- ["The Real-Time Volumetric Cloudscapes of Horizon Zero Dawn" - Schneider et al., SIGGRAPH 2015 (PDF)](https://advances.realtimerendering.com/s2015/The%20Real-time%20Volumetric%20Cloudscapes%20of%20Horizon%20-%20Zero%20Dawn%20-%20ARTR.pdf)
- [Guerrilla Games - "Nubis: Authoring Real-Time Volumetric Cloudscapes with the Decima Engine"](https://www.guerrilla-games.com/read/nubis-authoring-real-time-volumetric-cloudscapes-with-the-decima-engine)
- [Guerrilla Games - "Nubis, Evolved"](https://www.guerrilla-games.com/read/nubis-evolved)
- [Andrew Schneider - schneidervfx.com](https://www.schneidervfx.com/)
- [Kojima Productions - Wikipedia](https://en.wikipedia.org/wiki/Kojima_Productions)
- [80.lv - "Talking with Konami: Lighting in PES"](https://80.lv/articles/talking-with-konami-lighting-in-pes)
- [Softpedia - "PES 2015 Uses Refined Fox Engine for Realistic Player Faces, Movement"](https://news.softpedia.com/news/PES-2015-Uses-Refined-Fox-Engine-for-Realistic-Player-Faces-Movement-449602.shtml)
- [Operation Sports - "PES 2022: What to Expect From the Unreal Engine (Part One)"](https://www.operationsports.com/pes-2022-what-to-expect-from-the-unreal-engine-part-one/)
- [MetalGearInformer - "Interview with the people behind the Fox Engine"](https://www.metalgearinformer.com/interview-with-the-people-behind-the-fox-engine/)
- [GameDeveloper - "FOX engine brings Kojima Productions one step closer to photo-reality"](https://www.gamedeveloper.com/programming/fox-engine-brings-kojima-productions-one-step-closer-to-photo-reality)
- [GameDeveloper - "Video: Using the Fox Engine to build Ground Zeroes"](https://www.gamedeveloper.com/design/video-using-the-fox-engine-to-build-i-ground-zeroes-i-)
- [Kiefer Sutherland - Metal Gear Wiki](https://metalgear.fandom.com/wiki/Kiefer_Sutherland)
- [Digital Foundry - MGS V The Phantom Pain hands-on (NeoGAF thread)](https://www.neogaf.com/threads/digital-foundry-hands-on-with-metal-gear-solid-v-the-phantom-pain.1102881/)
- [MGS V weather & locations detail - GamingBolt](https://gamingbolt.com/metal-gear-solid-5-the-phantom-pain-weather-and-locations-detailed)
- [GameSpot - "Kojima: Fox Engine all about photorealism"](https://www.gamespot.com/articles/kojima-fox-engine-all-about-photorealism/1100-6405719/)
