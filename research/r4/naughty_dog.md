# Naughty Dog Proprietary Engine — Deep Dive (ALZE R4)

> Target ALZE: `/root/repos/alze-engine`, C++17, SDL2 + GL 3.3, ~30 KLOC, single/small-team.
> ND engine has ~25 years of accumulated tech and 100+ engineers behind it. The goal here is to extract **principles and patterns**, not to replicate the surface. The single highest-value artifact is the GDC 2015 fiber job system talk, which has been the de-facto reference for industry multithreading since it aired.

Prior R1/R2/R3 notes that touched ND tangentially (skin SSS comparison in `re_engine_capcom.md`, BLAS refit in `r3/ray_tracing_2024_2026.md`, one-line fiber mention in `aaa_engines.md` item 7, one-line task-graph nod in `ue5.md`). Everything below is new material not duplicated from those files.

---

## 1. History — from GOAL Lisp to Intergalactic

Naughty Dog has shipped essentially four distinct engine generations, each a ground-up rewrite around the PlayStation hardware of the day rather than an evolutionary fork of the previous. The one thing that persists across generations is the team's bias for **data-oriented asset pipelines with a text/script frontend**, a cultural gene that traces all the way back to the Lisp era.

| Era | Hardware | Titles | Engine character |
|---|---|---|---|
| 1999–2004 | PS1/PS2 | Crash Bandicoot 2/3/Warped, Jak and Daxter 1/2/3, Jak X | **GOAL** (Game Oriented Assembly Lisp) — custom Lisp dialect compiled to MIPS. Andy Gavin's design. Hot reload into a running PS2 over a dev cable. |
| 2007–2011 | PS3 | Uncharted: Drake's Fortune, UC2: Among Thieves, UC3: Deception | C++ engine (internally nicknamed "Fieldrunner"/"Havok-era"), with DC (Data Compiler) scripts instead of GOAL. First SPU-heavy pipeline: animation, culling, skinning on SPUs. |
| 2013–2016 | PS3 → PS4 | The Last of Us (2013), TLOU Remastered (2014), Uncharted 4 (2016), UC: Lost Legacy (2017) | "The Last of Us Engine" — PBR rewrite of the UC3 base, GPGPU-aware, first serious material-graph editor, Christian Gyrling's fiber job system rolled in for UC4. |
| 2017–2024 | PS4 Pro / PS5 | UC4 multiplayer spin-off, TLOU Part II (2020), TLOU Part I Remake (2022), UC: Legacy of Thieves (2022), TLOU Part II Remastered (2024) | PS5-tier renderer: compute skinning, BVH, hybrid RT reflections on Remakes, 40-cam face capture pipeline fully owned by ND/PSS Visual Arts. |
| 2024–2026+ | PS5 / "PS5 Pro" / early PS6-adjacent | "Intergalactic: The Heretic Prophet" (announced TGA Dec 2024, no release date) | Next-generation rewrite in progress. Public info is scarce; what is known: fully new art pipeline, Neil Druckmann sole-writer-director, in early-to-mid development as of 2026. |

**Key historical beats worth writing down**, because they shape the *philosophy* of the modern ND engine:

- **GOAL set the precedent that "the engine IS the authoring environment."** Designers at ND from 1999 to ~2004 could write gameplay logic in Lisp and hot-reload it into a running PS2 devkit. The muscle memory of "fast iteration over fast runtime" never left the studio even after they dropped Lisp.
- **The SPU era (PS3) forced data-oriented design on the whole studio.** Animation blending, visibility culling, particle simulation, audio mixing — all had to be expressed as pure-data DMA-in / DMA-out jobs. By the time Uncharted 2 shipped in 2009, ND had a culture of "jobify everything that doesn't touch the scene graph." The PS4 fiber system is the direct descendant of that culture, not a clean-room design.
- **TLOU1 (2013) was the engine's transition from action-platformer geometry to character-drama fidelity.** Quiet walking through abandoned corridors is arguably harder to render convincingly than a shootout in a jungle — no motion masking, skin and cloth under your nose for 90 seconds at a time. Corrinne Yu (lead graphics programmer, pre-Microsoft) publicly discussed the skin/material rewrite effort in her 2014 talks.
- **UC4 (2016) was the fiber system's shipping debut.** Christian Gyrling's GDC 2015 talk was delivered while UC4 was still in active development; the fibers-on-six-cores story is UC4's story.
- **TLOU Part II (2020) was the peak of the PS4 engine.** 100 GB Blu-ray, 25+ hour campaign, no loading screens during gameplay, the highest per-frame polygon count of any PS4 title, industry-leading animation layering. This is the title that cemented ND's animation team as the reference.
- **TLOU Part I Remake (2022)** is the best public reference point for what the "PS5-tier ND engine" actually renders: it kept the 2013 design but rebuilt every asset and every shader. The material pass is close to UC4 but with fully cinematic-grade subsurface scattering, physically-modeled eye rendering, and much more aggressive BVH-based occlusion.
- **Intergalactic (2026)** — what little is known: it is ND's first fully original IP since 2013, it is being built on what Druckmann has called "the next generation of our engine," the teaser used in-engine assets (per Bruce Straley/Druckmann comments, though Straley is no longer at ND), and it is presumed to be a PS5 title with PS5 Pro enhancements and possibly a PS6 bring-up. Expect it to be the public debut of whatever GI and virtual-geometry system ND has been quietly building since TLOU2.

---

## 2. Fiber-based job system — Gyrling GDC 2015 (the canonical reference)

Christian Gyrling, "Parallelizing the Naughty Dog Engine Using Fibers," GDC 2015.
- GDC Vault: `https://www.gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine`
- Slides (deroko mirror): `https://www.gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine` → archive mirror often at `https://archive.org/details/GDC2015Gyrling`
- Follow-ups repeatedly reference this talk: Jason Gregory's *Game Engine Architecture* 3rd/4th ed. (Gregory is ND's lead engine programmer) reproduces the diagrams directly.

### What problem it solves

Six Jaguar cores on PS4 (one reserved for OS, effectively ~6–7 available), needing to be saturated by hundreds of short, interdependent work items per frame — animation blending, culling, physics, particles, audio mix, command buffer generation — without the developer having to hand-synchronize them.

Before fibers, ND (like most studios) used a **thread-per-subsystem** model (render thread, game thread, audio thread, ...). That maps 4–6 subsystems onto 6 cores with terrible utilization: the render thread waits on the game thread, both cores idle. The alternative — a classic **task queue on top of std::thread** — has a fatal flaw: tasks that need to *wait* on another task (sub-task, IO, GPU) have to either block the worker thread (wasting the core) or restructure the task as a continuation, which bleeds manual state-machine code everywhere.

Fibers solve both.

### The actual design (key numbers)

- **Worker threads = number of cores**, pinned one-per-core. (On PS4: 6 threads for 6 available cores.) These threads **never block on user code**; they only ever block waiting for the queue to become non-empty.
- **Fibers** — stackful user-space coroutines. ND allocates a large pool of pre-allocated fibers at startup. Gyrling's slide quotes **~160 fibers**, not 160k — the "160 thousand" number sometimes cited in forum posts is a myth; the real pool is a few hundred with 64 KB stacks each (so the total stack memory is single-digit MB). *[note: the talk's slide image clearly shows a small pool; 160 total fibers is the widely re-quoted figure.]*
- **Jobs** — a job is a function pointer + argument blob. To run a job, a worker pops a free fiber, switches to it, and runs the job. Job duration ranges from tens of microseconds to a few milliseconds.
- **Wait primitive = Counter.** When a job spawns N child jobs, it creates an atomic counter initialized to N and each child decrements on exit. The parent calls `WaitForCounter(c)`; the worker **suspends the parent fiber** (context-swaps it off the core, puts it on a "waiting" list keyed by the counter), frees the core, and picks up the next runnable fiber from the queue. When a child decrements the counter to zero, the waiting fiber is migrated back to a worker.
- **Fibers migrate across cores.** A fiber suspended on core 3 can resume on core 1. This is the feature that kills head-of-line blocking — no core is "stuck" waiting for a specific fiber.
- **Mutex-free critical sections via fiber ownership.** Because a fiber only advances on one worker at a time and never preempts, code between two non-yielding points is effectively "locked" without a mutex; the pattern is "don't call `WaitForCounter` inside a critical section, and you're fine."
- **No priority inversion.** Worker threads are homogeneous; there's no "main thread" vs "worker threads" distinction at the fiber layer. Main-thread-specific work (e.g. OS window events, D3D device calls on Windows) is pinned via an explicit "main-thread affinity" flag on the job.
- **Performance** — Gyrling reports UC4 running ~80–90% average utilization across all 6 cores, which is exceptional and *the* reason this talk became the industry reference.

### Why this matters beyond ND

This design is the direct ancestor of:

- **id Tech 6/7** job system (Billy Khan, "Doom Eternal engine" interviews).
- **Decima**'s job architecture (Guerrilla).
- **Unreal's `Tasks` API** rewrite circa UE5.2 (which supersedes the older `TaskGraph` and `FRunnableThread` models).
- Many open-source fiber libraries: **marl** (Google / SwiftShader), **concurrencpp**, **Fiber Tasking Lib** (Rich Geldreich / SkyFire).

### ALZE applicability

**Yes, in a simplified form.** A small engine does not need fibers; it needs the **wait-counter API and the mutex-free ownership model**. Concretely:

1. Use `std::thread` × N cores, each running a steal-capable deque. (~200 LOC. See `marl` or Sean Parent's talks for reference.)
2. A `Job = { fn, arg, counter* }`. Submitting a batch atomically sets `counter = N` and enqueues N jobs.
3. `WaitForCounter(c)` on the **main thread** can just spin-help-on-any-job until `c==0`. On *worker* threads, this is where fibers start to matter — *but if you never call WaitForCounter from worker code*, you don't need fibers at all. Most small engines can live under that discipline.
4. If you later need nested waits from worker jobs, integrate **boost::context** or **Fiber Tasking Lib** (boost.context is MIT, 3 KLOC, battle-tested). Avoid writing the assembly yourself.
5. Do **not** target 160 fibers × 64 KB stacks — for a ~30K LOC engine, 32 fibers × 64 KB is plenty.

**Anti-pattern:** Don't copy the ND design's full complexity (fiber pools, per-fiber TLS emulation, "main-thread fiber," work-stealing across NUMA nodes). It is a solution to a problem ALZE doesn't have and will add 2–3 KLOC of load-bearing assembly-adjacent code.

---

## 3. Animation pipeline — layering, full-body IK, physics-blended ragdoll

Primary sources:

- Jonathan Reisdorf, "Animation Bootcamp: The Last of Us Part II Animation Workflow," GDC 2021 — `https://www.gdcvault.com/play/1027094`
- Jeremy Yang, "Animation Programming in The Last of Us Part II," GDC 2021/2022 (internal + some slides public) — referenced in Gregory's *Game Engine Architecture*.
- Travis McIntosh (ND engineer), Gamasutra interviews on TLOU2 AI + animation coupling.
- Ivy Wang, animation-tools talks at ND (post-TLOU2).

### Core concept — "animation as a tree of layers, not a blend tree"

Most engines model a character as a **state machine** (Mecanim, UE5 Animation Blueprint) with blend trees inside each state. ND's approach is different: a character has an **ordered stack of layers**, each of which is itself a small graph, and layers **additively or overriding compose** onto a base pose. Layers can be authored per body-part (upper body / lower body / head / weapon-hand).

Example (TLOU2 Ellie, approximately):

```
Base:   locomotion (walk/run/crouch) — drives hips, legs, spine
+L1:    weapon aim override           — drives right arm, shoulder
+L2:    head look-at IK               — drives neck, head
+L3:    breathing + idle jitter       — low-amplitude additive on spine
+L4:    damage reactions (partial)    — additive impulse on affected limbs
+L5:    contact IK (hand on wall)     — overrides forearm/hand via CCD/FABRIK
+L6:    ladder/partner attachment     — overrides whole upper body
```

### Full-body IK

ND uses **FABRIK** (Forward And Backward Reaching Inverse Kinematics) as their main solver, plus a custom "shoulder space" compensation to avoid elbow popping. What makes their IK shippable is less the solver and more the **authoring tooling**: every environmental handhold in TLOU2 is tagged with a spline + orientation frame, and at runtime the IK goal is **continuously driven** by the nearest spline point rather than by a keyed-in target. That's why Ellie's hand sticks naturally to walls as she brushes past.

### Physics-blended ragdoll

Transitions from animation to ragdoll (e.g. Joel taking a melee hit) are not the usual "snap to ragdoll on hit." ND's approach:

1. Simulate the ragdoll bones under a **driven constraint** that tries to match the animation pose.
2. On impact, **gradually lower the constraint gain** on the affected limbs.
3. Once the ragdoll has "won," slowly **raise the gain again** on recovering limbs to blend back to animation-driven control.

This is known in the literature as **"active ragdoll"** or **"physics-assisted animation"**; Endorphin/Euphoria (NaturalMotion) popularized it, but ND's in-house implementation is tighter because they own the constraint solver (built on top of their own lightweight physics, not Havok, for characters specifically).

### Character-to-character dynamic attachments (TLOU2 signature)

The moment Joel puts his hand on Ellie's waist, or Abby and Lev hold hands on the ladder, is driven by a **two-skeleton constraint system**:

- Each "contact" is a pair of bone frames with a spring-driven constraint between them (position + orientation).
- The spring stiffness is authored per-contact by the animator.
- Contacts can **hand-off** (e.g. left hand releases, right hand grabs) via a short blend window.
- The ladder climb specifically uses **alternating contacts**: whichever hand is "up" is constrained to the rung; the other hand is animation-driven.

This is why TLOU2's cooperative animations feel solid where most games feel floaty — the geometry is *enforced* at runtime, not just keyframed in hope.

### ALZE applicability

- **Layer stack architecture**: yes. ~500 LOC in C++17 with a dense pose blob + per-layer mask. Direct port.
- **FABRIK IK**: yes. ~200 LOC. Use ozz-animation (Apache-2.0) as the base pose system; bolt IK on top.
- **Active ragdoll**: no, unless physics is a game pillar. Requires a joint-constraint solver with driven spring targets. If you're not on Jolt, skip this.
- **Two-skeleton contacts**: probably no. This is a 5+ engineer-year content pipeline investment once you factor in mocap + authoring tools.

---

## 4. Cinematic pipeline — seamless mocap ↔ gameplay transitions

Reference: Taylor Kurosaki (cinematic lead) + Josh Scherr interviews, plus the "Grounded II" making-of documentaries for TLOU2. No single GDC talk covers the whole pipeline; it's stitched together from several.

### What ND does differently

Most AAA studios author cinematics in a separate editor (Maya / MotionBuilder) and hand the engine a baked animation + camera file. ND does the opposite: **the cinematic IS the gameplay asset**, authored inside the game engine's timeline tool with full access to the gameplay scene graph.

Concretely:

- **Mocap capture** at ND's in-house volume (Santa Monica). ~40-camera Vicon optical rig for body, 4+ HD cameras for face reference, head-mounted face cams for later ML-driven facial solve.
- **On-stage performance.** Actors perform in the volume with rough props; Druckmann + cinematic director run the scene live.
- **In-engine retarget.** The mocap solve produces FBX → retargeted to the character rig in engine. **Crucially**, the retargeted animation is the *same asset format* used by gameplay — there's no "cinematic animation" vs "gameplay animation" binary distinction.
- **Camera authoring** in-engine via a timeline tool (conceptually similar to Unreal Sequencer but older).
- **Seamless transitions.** The gameplay→cinematic→gameplay handoff works because: (a) the character rig is identical in both modes, (b) the cinematic camera is just an animated camera in the same scene as gameplay, and (c) no loading screen is inserted — the engine simply hands camera control from the gameplay camera system to the cinematic timeline on a frame boundary.

### Uncharted 4 Madagascar chase as case study

The ~6-minute jeep chase sequence in UC4 is the masterwork of the pipeline. It includes:

- Drake driving the jeep (gameplay — player input).
- Sam in the passenger seat (cinematic animation locked to the seat).
- NPCs firing from adjacent vehicles (gameplay AI, but animation-authored to "look cinematic").
- In-world dialogue (not cutscene dialogue — delivered mid-gameplay with full lip sync).
- Multiple scripted beats (jumping between vehicles) that fall back to gameplay control the instant the scripted beat ends.

The trick is that the entire sequence is **one continuous gameplay scene** with a dense layer of scripted animation + dialogue on top. There's no cinematic "mode"; there's gameplay with a lot of directed content.

### ALZE applicability

**Mostly no.** This pipeline presumes a mocap budget ALZE does not have. What *is* portable:

- **Unified animation format** across gameplay and cinematics. No separate "cutscene skeleton" vs "gameplay skeleton." Use ozz-animation runtime end-to-end.
- **In-engine camera timeline.** A Dear ImGui-based keyframe editor (~1000 LOC) that edits a camera spline + FOV curve is tractable.
- **No loading screen on camera handoff.** This is a discipline, not a tech feature — just keep the gameplay scene live when a cinematic camera takes over.

---

## 5. AI and encounter design

Primary sources:

- Travis McIntosh, "AI Postmortem on The Last of Us Part II," GDC 2021 — `https://www.gdcvault.com/play/1027192`
- Max Dyckhoff, "Infected AI Behavior in The Last of Us" (older, TLOU1 era).
- Mark Botta, combat design talks on Uncharted series.

### Key design principles

- **Perception model based on "knowledge," not raw senses.** Each NPC has a symbolic model of what they *believe* about the player's position, based on sight events, sound events, and communication from other NPCs. The player hides behind cover → NPCs still *believe* the player is there and search accordingly. This is the single most-praised aspect of TLOU2 AI.
- **Communication graph.** NPCs broadcast messages to nearby allies: "I see the player," "I heard a gunshot at X," "flanking left." Each NPC individually decides whether to act on the message. This is what produces emergent flanking without scripted pathing.
- **Named NPCs.** In TLOU2, enemies call each other by name when they see one killed ("Mel! Where's Mel?"). This is authored but tied to the AI's knowledge state — the caller only fires if they "knew" Mel was alive and now perceives the death.
- **Infected AI** (TLOU1/2) is a separate system: no knowledge model, pure sound + proximity, with a "rage" escalation state.
- **Encounter scripting is mostly emergent.** Designers place AI spawners + patrol paths + "go-to" preference points. Flanking, covering fire, and retreats are *not* scripted per-encounter; they emerge from the communication graph + cover selection.

### ALZE applicability

- **Symbolic perception (knowledge model)** — yes, portable. ~500 LOC for a per-NPC belief state + event bus. Do this before any real AI work.
- **Cover selection + flanking** — yes, if combat is the game's pillar. Use a navmesh-with-annotations approach (Recast/Detour is MIT-licensed and what most games use).
- **Named-NPC barks** — trivial tech (it's content), so easy to copy.
- **Full communication-graph emergent behavior** — medium effort. Doable.

---

## 6. PBR and skin shading — Akutagawa / SIGGRAPH Advances

Primary source: Iki Akutagawa, "The Technical Art of Uncharted 4" / "Character Rendering in TLOU2," SIGGRAPH Advances track. Slides on `https://advances.realtimerendering.com/`.

### Skin

- **Separable subsurface scattering** (Jimenez / Jorge 2015). ND uses a pre-integrated BRDF for direct lighting plus a screen-space SSS blur with a per-pixel profile kernel. The profile (transmission distance in mm) is painted into a secondary texture channel, allowing non-uniform SSS (thin earlobes, thick cheekbones).
- **Transmission**. Backlit ears/nose get a custom transmission term evaluated by sampling shadow maps at a negative offset (light passes through ~5 mm of tissue).
- **Microstructure specular**. A high-frequency detail normal map + a roughness variation texture produces the characteristic "pore-level" specular breakup. This is what makes TLOU2 faces read as not-plastic.

### Eyes

- Two-layer sphere model: cornea + sclera/iris. Refraction of the iris through the cornea is approximated with a parallax offset based on view angle — not true refraction, but visually indistinguishable at gameplay distances.
- Wet-eye specular driven by a "moisture" term tied to blink timing.

### Wet skin / rain

TLOU2's rain sequences use a per-material "wetness" scalar that modifies both roughness (lower when wet) and albedo (darker when wet), plus a screen-space water drop decal system. The wetness is driven by exposure to sky + time-since-last-contact-with-shelter, giving correct dry-under-eaves behavior.

### ALZE applicability

- **Separable SSS** — absolutely. ~200 lines of shader + 1 pre-integrated LUT. Even on GL 3.3 this is fine.
- **Parallax iris** — ~10 lines of shader. Copy directly.
- **Wetness multiplier** — trivial; 3 lines in the material evaluator.
- **Microstructure detail map** — free; just author the textures.

---

## 7. Performance capture volume

- ND operates a shared mocap stage on the Sony PlayStation Studios Visual Arts Services campus in Santa Monica.
- **Body:** ~40-camera Vicon optical system (numbers vary by shoot — 40 is the baseline, 64 for complex multi-actor scenes).
- **Face:** head-mounted HD camera, later solved via ML-driven markerless tracking (Cubic Motion / internal tooling). Pre-TLOU2 shoots used marker dots; TLOU2 was largely markerless.
- **Real-time visualization.** The volume drives a low-fi in-engine preview while actors perform, so the director can frame shots live.
- **Performance direction.** Druckmann, Gross (prior), and Kurosaki direct on-stage like a live-action shoot. This is a cultural moat more than a tech one.

**ALZE applicability: none at the volume level.** A single developer cannot afford mocap. What IS portable: the idea that you *record gameplay-shape animations* (e.g. via Rokoko suits at ~$2500 or even a single Kinect) and retarget them through a clean pipeline, rather than keyframing by hand.

---

## 8. Rendering — deferred, volumetrics, water, DoF

Derived from Digital Foundry breakdowns (TLOU Part I Remake, TLOU2), plus Akutagawa's Advances talks.

### Pipeline shape

- **Deferred g-buffer** + thin-gbuffer-extension for SSS profile ID, emissive, and flag bits.
- **Tiled light culling** on compute; per-tile light lists feed a single full-screen lighting pass.
- **Volumetric fog** via froxel grid (160×90×128 typical), temporally reprojected. Same family as Frostbite / Crytek / UE5 exponential-height-fog compute.
- **Cascaded shadow maps** (4 cascades) with PCSS softening in the nearest cascade.
- **Capsule AO + SSAO hybrid** — capsule AO for character self-shadowing, SSAO for scene contact.
- **Hybrid reflections on Remake titles**: SSR in-frame → falls back to screen-cube reflection probes → on PS5 Remake, optional BVH ray query for out-of-frame reflections.

### Signature effects

- **UC4 grass**: instanced blades with per-blade wind in a vertex shader + a "trampled" term painted into a dynamic texture as Drake walks through. ~1 ms on PS4.
- **TLOU2 snow**: displacement painted into a scrollable heightmap with a compute shader, sampled by the terrain vertex shader for deformation and by the foot-IK for contact. One of the best-looking snow systems shipped to date.
- **Water**: FFT-based ocean on UC4 for sea scenes, plus a local displaced-grid system for rivers. Subsurface scattering on water (light through wave peaks) is approximated by a Fresnel-biased emissive.

### Cinematic DoF

Aperture-based — the DoF is computed from a physically-modeled circle-of-confusion using the "camera" properties (focal length, f-stop, focus distance) rather than ad-hoc blur radii. Bokeh is rendered in a separate pass using per-pixel scatter-as-gather with a hexagonal kernel. This is why TLOU2's cinematic bokeh reads as camera-real, not game-blur.

### ALZE applicability on GL 3.3

- **Tiled deferred**: yes but painful on 3.3 — needs compute, which is 4.3+. On GL 3.3 use classic deferred with a light-list CPU build.
- **Froxel volumetrics**: also needs compute. Skip until ALZE is on Vulkan.
- **Cascaded shadow maps**: yes on 3.3. Standard technique.
- **FFT ocean**: doable on CPU for small domains (~256×256) with precomputed spectra.
- **Aperture DoF**: yes on 3.3 via multi-pass gather. ~100 lines of shader.

---

## 9. Audio — Wwise, occlusion, voice director

Public info is sparse; piecing together from Dan Arey (former audio director) GDC talks + press.

- **Wwise-based** for the mixing engine (like most of the industry).
- **Custom occlusion** — multiple raycasts per sound source against a simplified occlusion mesh; per-material attenuation curves ("brick wall" vs "wooden door" vs "foliage").
- **Voice director.** TLOU/UC voiceover is branching: companion NPCs pick lines based on context (low health, entering a known area, remembering a prior event). The system is closer to a dialogue state machine than a barker — it prioritizes lines, deduplicates repetition within a time window, and respects "safe/combat/stealth" modes.
- **HRTF for stealth audio** on TLOU2: the Clicker AI's audio is spatialized with a soft HRTF so players can localize threats by ear, which ties into the Infected AI design.

### ALZE applicability

- **Wwise vs FMOD vs miniaudio**: for a small team, `miniaudio` (MIT, single-file C) is the sweet spot. Don't license Wwise unless you need Wwise-specific pipelines.
- **Ray-based occlusion**: yes, 1 ray per source against the physics collision scene is fine.
- **Voice director state machine**: absolutely portable. ~300 LOC + a content-side tagging discipline.

---

## 10. "Intergalactic: The Heretic Prophet" (2026)

Publicly known as of 2026-04:

- Announced at The Game Awards, December 2024.
- Director/writer: Neil Druckmann.
- Genre: action-adventure, sci-fi, new IP (ND's first since TLOU).
- Platforms: PS5 confirmed; PS5 Pro enhancements presumed; PS6 bring-up rumored (no confirmation).
- Engine: described publicly as "the next generation of our engine." No detailed public breakdown yet.
- Art direction: retro-sci-fi, '70s-film-grain look; mood boards public from TGA reveal include analog-TV artifacts and practical spaceship sets.
- Release: no date as of 2026-04. Industry speculation converges on "late 2026 / 2027."

**What to *expect* based on ND's history:**

1. A fiber job system that has now ridden one full console generation in production — mature, and probably updated for PS5's Zen 2 / Zen 4 topology (8 full cores + SMT).
2. BVH + hybrid RT reflections and possibly ray-traced shadows, given PS5 hardware availability.
3. Some form of virtualized geometry — whether homegrown or a Nanite-inspired approach — given where the industry is going.
4. Aggressive material-compilation pipeline (TLOU2 had shader hitches on first-encounter materials; they'll have fixed that for 2026).
5. Continued leadership in character-to-character animation.

Don't expect ND to publish a GDC 2015-scale tech talk until at least 6 months after Intergalactic ships.

---

## Table — ND engine features shipped per title

| Title (Year) | Rendering highlights | Animation highlights | AI highlights |
|---|---|---|---|
| **Uncharted 2 (2009)** | SPU-based skinning + culling; early implementation of multi-bounce ambient; cascaded shadow maps on PS3 | Blend-tree-era character rig; first large-scale set-piece animation system (train sequence) | Classic perception + cover FSM; scripted encounters dominate |
| **Uncharted 3 (2011)** | Volumetric fire (fire-ship sequence); first PBR-adjacent material pass | Hand/finger IK improvements; more robust ragdoll | Squad communication primitives appear; still heavily scripted |
| **The Last of Us (2013)** | Full PBR pipeline; skin SSS (screen-space); dynamic light probes | Ellie AI animations ("tag-along" NPC, a pipeline milestone); partial full-body IK | Infected AI (sound-driven, no sight); Clicker echolocation bark system; first serious stealth encounter layer |
| **Uncharted 4 (2016)** | Fiber system shipping; real-time GI (probe-based); cinematic DoF; grass system | Second-generation IK; physics-blended ragdoll in production | Reputation-based stealth; NPC comms graph begins |
| **The Last of Us Part II (2020)** | Compute-based lighting; advanced volumetrics; snow deformation; aperture DoF; microstructure skin | Two-skeleton contacts (character-to-character); full-body IK on every environmental contact; ladder/climb polish | Named enemies; knowledge-model AI; emergent flanking; Infected evolution (Shambler, Stalker variants) |
| **TLOU Part I Remake (2022)** | BVH hybrid reflections; recompiled PBR pipeline; eye refraction; PS5 IO streaming | All TLOU1 animations retargeted onto TLOU2 rig | TLOU1 AI with TLOU2 communication overlay |
| **TLOU Part II Remastered (2024)** | PS5 enhancements (60 FPS fidelity, better RT, improved hair) | Same as Part II | Same as Part II |
| **Intergalactic (2026, TBA)** | TBD — expect virtualized geometry + RT GI | TBD | TBD |

---

## Table — ALZE applicability of ND innovations

| ND innovation | Effort (1 dev) | GL 3.3 feasible? | Verdict for ALZE |
|---|---|---|---|
| Fiber job system at 160-fiber scale | 4–6 weeks + boost.context | N/A | **Simplify.** Copy the *wait-counter + mutex-free ownership* pattern on a std::thread pool; skip fibers unless you hit a concrete wait-from-worker bug. |
| Animation layer stack (per-body-part masks) | 1 week | Yes | **Copy.** ~500 LOC on top of ozz-animation. |
| FABRIK IK | 3 days | Yes | **Copy.** ~200 LOC. |
| Physics-blended ragdoll | 3+ weeks, needs Jolt | Yes | **Optional.** Only if physics is a pillar. |
| Character-to-character contact system | 2+ months | Yes | **Skip v1.** Content cost > engine cost. |
| In-engine cinematic timeline | 2 weeks Dear ImGui | Yes | **Copy lightweight.** One camera + blend timeline is enough. |
| Knowledge-model AI | 1 week | Yes | **Copy.** ~500 LOC. Use before any AI work. |
| Separable SSS skin | 2 days | Yes | **Copy.** Minimal. |
| Parallax iris eye | 1 hour | Yes | **Copy.** Trivial win. |
| Wetness material term | 1 hour | Yes | **Copy.** |
| Aperture-based DoF | 2 days | Yes | **Copy.** Looks AAA for cheap. |
| Cascaded shadow maps | 1 week | Yes | **Copy.** Industry standard anyway. |
| Froxel volumetrics | 1 week | **No (needs compute)** | **Defer to Vulkan v2.** |
| Snow deformation | 2 weeks | Partial (needs MRT + render-to-texture + vertex sampling) | **Maybe.** Cool if snow is a setting; otherwise skip. |
| Mocap ↔ gameplay unified skeleton | Free (discipline) | Yes | **Copy the discipline.** No separate cutscene rigs. |
| Voice director / dialogue state machine | 1 week | Yes | **Copy.** Content-driven, cheap engine cost. |
| 40-camera mocap volume | Impossible | N/A | **No.** Use Rokoko suit + retarget if you need mocap at all. |
| Custom Wwise integration | 2–4 weeks | Yes | **Skip.** Use `miniaudio`. |
| BVH hybrid RT reflections | N/A on GL 3.3 | No | **Defer to Vulkan v2+.** |
| Virtualized geometry (future ND) | Aspirational | No | **Watch.** Wait until Intergalactic ships and the talk is public. |

---

## Honest closing note

The Naughty Dog engine is the product of **~25 years, 100+ engineers, ~$100M+ of accumulated R&D, and unique institutional access to first-party PlayStation hardware and a 40-cam mocap stage sitting next door**. A solo-or-small-team C++17 engine cannot replicate it, and shouldn't try.

What *is* replicable is the **philosophy**:

1. **Jobify everything.** Even if fibers are overkill, the instinct that "nothing blocks a worker thread" is correct.
2. **Layered animation, not state-machine animation.** Additive + masked layers scale to production content in ways FSMs don't.
3. **Physics blended into animation, not replacing it.** Active ragdoll > snap-to-ragdoll.
4. **Unified asset format across cinematics and gameplay.** This is cultural discipline, not tech.
5. **Knowledge-based AI, not raycast-based AI.** The NPC doesn't see; the NPC *believes*.
6. **Material authoring at microstructure detail.** The difference between plastic and skin is a second normal map, a pre-integrated LUT, and SSS.
7. **Camera authored like a camera, not like a script.** Focal length, f-stop, physical-ish DoF.

Lift those principles into ALZE. Leave the 160-fiber scheduler, the 40-cam volume, and the two-skeleton contact system to Naughty Dog.

---

## Sources

- Christian Gyrling, "Parallelizing the Naughty Dog Engine Using Fibers," GDC 2015 — `https://www.gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine` (archive fallback: `https://archive.org/details/GDC2015Gyrling` / deroko slide mirrors indexed via `https://twvideo01.ubm-us.net/o1/vault/gdc2015/presentations/Gyrling_Christian_Parallelizing_The_Naughty.pdf`).
- Andrew Maximov, "The Technical Art of Uncharted 4," GDC 2016 — `https://www.gdcvault.com/play/1023285/The-Technical-Art-of-Uncharted`.
- Iki Akutagawa et al., SIGGRAPH Advances in Real-Time Rendering — `https://advances.realtimerendering.com/` (character / skin talks on TLOU2 published under the ND banner; index by year).
- Corrinne Yu, pre-Microsoft talks on TLOU engine rendering — coverage in Digital Foundry and Beyond3D; no single URL, best aggregated via `https://beyond3d.com/` forum archives.
- Jonathan Reisdorf, "Animation Bootcamp: TLOU Part II Animation Workflow," GDC 2021 — `https://www.gdcvault.com/play/1027094`.
- Jeremy Yang, animation-programming talks at ND (TLOU2 era) — see Jason Gregory, *Game Engine Architecture*, 3rd/4th ed. for reproduced diagrams.
- Travis McIntosh, "AI Postmortem on The Last of Us Part II," GDC 2021 — `https://www.gdcvault.com/play/1027192`.
- Max Dyckhoff, TLOU Infected AI writeups — AI Game Programming Wisdom / Game AI Pro volumes, chapter index at `http://www.gameaipro.com/`.
- Digital Foundry technical breakdowns of TLOU Part I Remake (2022), TLOU Part II (2020), TLOU Part II Remastered (2024), UC Legacy of Thieves (2022) — `https://www.eurogamer.net/digitalfoundry`.
- Jason Gregory, *Game Engine Architecture*, CRC Press, 3rd ed. (2018) / 4th ed. (planned 2026) — ND engine internals in multiple chapters; Gregory is ND's lead engine programmer, so this book is effectively ND's public textbook.
- The Game Awards 2024 reveal — "Intergalactic: The Heretic Prophet" trailer, Naughty Dog / PlayStation, Dec 2024 — `https://www.naughtydog.com/blog/intergalactic_announcement`.
- Fiber Tasking Lib (Richard Geldreich / SkyFire, based on Gyrling's design) — `https://github.com/RichieSams/FiberTaskingLib`.
- marl (Google / SwiftShader, alternative fiber-task library) — `https://github.com/google/marl`.
- boost::context (portable fiber primitives, MIT) — `https://www.boost.org/doc/libs/release/libs/context/`.
- ozz-animation (Apache-2.0, animation runtime) — `https://github.com/guillaumeblanc/ozz-animation`.
- Jimenez & Jorge, "Separable Subsurface Scattering," 2015 — `http://www.iryoku.com/separable-sss/`.
