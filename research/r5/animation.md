# Animation Systems — Round 5 Deep Dive for ALZE

**Scope:** cross-cutting engine system research. Animation was covered lightly in R2 (RE Engine photogrammetry) and R4 (`naughty_dog.md` §3 layer stacks + active ragdoll; `anvil_ubisoft.md` §8 motion matching + PFNN + LMM; `fromsoftware.md` skeleton/FSM quirks). This doc is the full technical survey, from skin weights up to neural animation, with concrete LOC/MB/ms numbers and a v1/v2/v3 applicability plan for `/root/repos/alze-engine` (C++17, GL 3.3 today, Vulkan v2 tomorrow).

**Out of scope:** visual scripting of animation events (belongs in `editor_architecture.md`), audio-driven gameplay triggers (belongs in `audio.md`).

---

## 1. Skeletal animation — bones, skinning, math

### 1.1 Bone hierarchy and transforms

A character rig is a **tree of bones**; in the industry the root is typically a "pelvis" or "hips" joint, with spine/head/limbs branching off. Every bone `b` has:

- A **local (bind-pose relative) transform** `L_b` — usually stored as (translation, rotation, scale). Rotation is a unit quaternion (16 bytes for TRS10 packed) or a 3x3 rotation matrix (36 bytes). Animation clips sample this local transform over time.
- A **bind pose** `B_b` — the rest pose local transform.
- A **model-space transform** `M_b = M_parent * L_b` — computed at runtime via a single DFS walk. This is the matrix you multiply the vertex by.
- An **inverse bind matrix** `IB_b = (M_b^{rest})^{-1}` — precomputed at asset build. The final skinning matrix per bone is `S_b = M_b * IB_b`, which is what the GPU actually consumes.

Typical counts: humanoid ~60 bones (UE Mannequin 78, Ozz sample 67, MetaHuman 80+ plus faces 100+), quadrupeds ~80, hero characters with full finger/face rigs 150-300.

### 1.2 Skin weights

Each vertex is bound to N bones with weights summing to 1. Industry default is N=4 (packed as 4 byte indices + 4 float16 or normalized byte weights = 16 bytes per vertex of skin data). Some engines cap at 8 (UE5 default since 5.1, selectable) for highly deformed areas.

Why 4? GPU register pressure. An 8-bone skin requires twice the bone matrix fetches per vertex; at 4 you fit the skinning math in ~20 instructions. UE5 "compressed skin" format reduces to 2 bytes per weight index + 1 byte per weight.

**Skin weight authoring** is done in DCC (Maya Skin Weights, Blender Weight Paint, Houdini captureweight). Automated tools: Maya's "Geodesic Voxel Binding" (Dionne 2013 at Autodesk), Pixar's "Smooth Skinning Decomposition with Rigid Bones" (Le+Deng 2014 `https://binh.graphics/papers/2014s-ssdr/ssdr_lowres.pdf`).

### 1.3 Linear Blend Skinning (LBS)

For each vertex `v`:

```
v' = sum_{i=0..N-1} w_i * (S_{b_i} * v_bind)
n' = sum_{i=0..N-1} w_i * (S_{b_i}^{-T} * n_bind)   // normals use inverse-transpose
```

LBS is **fast, simple, and broken**. The famous "candy wrapper" artifact: when a bone twists 180°, the blended matrix collapses the volume. Shoulders, wrists, hips show it most.

### 1.4 Dual Quaternion Skinning (DQS)

Kavan, Collins, Zara, O'Sullivan 2007, "Skinning with Dual Quaternions", I3D 2007 (`https://www.cs.utah.edu/~ladislav/kavan07skinning/kavan07skinning.pdf`, archive `https://web.archive.org/web/2024*/www.cs.utah.edu/~ladislav/kavan07skinning/kavan07skinning.pdf`).

Represent rigid transform as a dual quaternion (8 floats: real + dual part), blend in dual-quat space, normalize, convert back to matrix. Eliminates candy-wrapper. Costs ~40 GPU instructions vs ~20 for LBS. Also: does NOT handle non-uniform scale (engineers bolt on a separate scale path).

Unity supports DQS optionally on the SkinnedMeshRenderer (Quality Settings "Dual Quaternion"); UE uses LBS by default with "shoulder helper bones" instead. Most AAA games still ship LBS + helper bones (twist bones at ~50% between elbow and wrist, shoulder helpers that rotate at half the shoulder's rate). Naughty Dog's TLOU2 rig documented 200+ helpers per character.

### 1.5 Compute vs vertex shader skinning

**Vertex shader skinning (classic):** skinning happens inside the regular vertex shader. Every draw pass (shadow, depth prepass, main color) re-skins. For a 100k-vertex character rendered in shadow + depth + main + 2 point-light shadows = 5x skinning work per frame.

**Compute skinning (modern, 2016+):** a dedicated compute pass outputs a skinned vertex buffer once per frame, and all subsequent passes read a static buffer. UE5 default since UE4.24, Unity since "Hybrid Renderer 2.0" DOTS. Saves work when the character is drawn multiple times; costs extra VRAM (one skinned vertex buffer per visible character).

Rough numbers: a 100k-vertex character at 4 weights skins in ~0.05 ms on a mid-tier GPU (RTX 3060). 200 characters = 10 ms of vertex work at 5 passes each vs 2 ms with compute skinning (skinned once, read 5x). ND's fiber system runs animation evaluation on CPU, skinning on GPU via compute since UC4.

---

## 2. Blend trees + state machines

Three major authoring paradigms:

### 2.1 Unity Mecanim (2012→)

- **Animator Controller** = a hierarchical FSM. Nodes are either sub-state-machines, blend trees, or leaf clips.
- **Blend tree** = N-dimensional interpolation between clips. Common shapes: 1D (speed → walk/jog/run), 2D Cartesian (stick X/Y → 8-dir strafe), 2D Directional ("1D + freeform"), Direct (per-parameter weights).
- **Transitions** are authored on arrows between states with duration + condition (bool/float/trigger parameters) + interruption rules.
- Root motion can be extracted or baked to in-place.
- Since 2018 Unity shipped `Animation Rigging` (constraint stack on top of Mecanim) + `Animation C# Jobs` (Burst-compilable custom nodes).

**Pros:** visual, artist-friendly, huge ecosystem. **Cons:** state explosion (50+ states for a complete locomotion+combat rig); transition tables are hard to debug; foot-sliding requires manual tuning.

### 2.2 Unreal Animation Blueprint (2014→)

- **AnimGraph** = dataflow graph of `FPoseLink` nodes. Unlike Unity's FSM-at-top, UE's top layer is a graph; FSMs are one kind of node inside it.
- Dedicated constructs: **State Machine** nodes, **Blend Space 1D/2D**, **Layered Blend per Bone**, **Aim Offset** (2D blend by pitch/yaw for upper body).
- **Control Rig** (2020+) adds Maya-style rigging inside UE; **IK Rig** + **IK Retargeter** (UE5 2022) replaced the old 2-bone IK node.
- The 2022 "Motion Matching" plugin (UE 5.4 preview, shipping 5.5 Nov 2024) added native PoseSearch support — Epic's first-party motion matching toolset.
- Runtime update threads: anim graph evaluates on a worker thread; blueprints (game logic) run on game thread.

**Pros:** integrated with Blueprints (gameplay can push parameters easily), modular layers, native motion matching now. **Cons:** performance tax from blueprint VM (mitigated by nativization + anim FastPath for trivial getters); complex graphs hit perf cliffs.

### 2.3 Godot AnimationTree (2018→)

- Similar concept to UE AnimGraph: a tree of `AnimationNode` subclasses (BlendSpace1D, BlendSpace2D, OneShot, StateMachine, Add, Blend2/Blend3).
- Drives an `AnimationPlayer` node; parameters exposed per-node and animatable.
- Godot 4 rewrote this with more node types, better transition fade, and the `AnimationLibrary` resource.

**Pros:** simple, open source, free to extend in C++. **Cons:** less battle-tested; no motion matching plugin; smaller asset marketplace; limited IK (bone chain constraint only, shipped in 4.0).

### 2.4 Authoring walk-to-run blend — worked example

Problem: character jogs at speed 3, runs at speed 6. You have 3 clips: walk (1 m/s), jog (3 m/s), run (6 m/s), all same stride-phase-aligned.

1. **Stride normalize.** Each clip must have the same phase at t=0 (right foot down). Normalize clip durations so a full gait cycle is 1.0 in phase space. Advance a shared phase variable per frame: `phase += speed / cycleDistance * dt`.
2. **Blend weights.** At speed S in [1,6]: `w_walk = max(0, (3-S)/2)`, `w_jog = bell(S, center=3, width=2)`, `w_run = max(0, (S-3)/3)`. Normalize to sum 1.
3. **Sample each clip at the shared phase, blend poses**, apply.
4. **Root motion.** Drive character velocity = blended root delta (so clip authoring can define actual locomotion speed, avoiding foot slide).

Failure modes: clips not phase-aligned → feet slide; root motion mixed with physics → jitter; transition too fast → "gliding." This is why motion matching exists: instead of hand-authoring 3 perfectly phase-aligned clips, the engine picks from 1000s of mocap frames automatically.

---

## 3. Motion Matching (classical)

**Origin papers:**

- Kovar, Gleicher, Pighin 2002, "Motion Graphs", SIGGRAPH 2002 — `https://research.cs.wisc.edu/graphics/Papers/Gleicher/Motion/motion-graphs-sig02.pdf` — the intellectual ancestor.
- **Simon Clavet (Ubisoft) GDC 2016**, "Motion Matching and The Road to Next-Gen Animation" — `https://www.gdcvault.com/play/1023280/Motion-Matching-and-The-Road` — the industry-moment talk. For Honor shipping reference. Slides archived at `https://www.gamedevs.org/uploads/motion-matching-road-to-next-gen.pdf` (occasional mirror).
- Michael Büttner (Ubisoft) GDC 2015, "Motion Matching - The Road to Next Gen Animation" earlier version.

### 3.1 Algorithm in one page

Input per frame:
- **Trajectory features** — desired root position + facing over next 0.5 / 1.0 / 1.5 seconds from gameplay (stick input + camera). Typically 6 x 2D = 12 floats.
- **Pose features** — current skeleton pose features: positions + velocities of left foot, right foot, hips (in root space). Typically 3 joints x 6 floats = 18.
- Combined feature vector `q` of dim ~30.

Database:
- Pre-extracted from **hours of mocap** (tens of thousands of frames). Every frame `f` in the DB gets its own feature vector `F_f` of the same dim.
- Also stored: pose sample (joint local transforms), root motion delta per frame.

Search loop (typically 10-30 Hz, not every frame):
1. Compute `q` for current frame.
2. Find frame `f*` in DB minimizing weighted L2 distance `||q - F_f||_W^2`.
3. If the current playing frame's next frame is "close enough" in distance, keep playing (no jump). Else jump to `f*` and continue playing forward from there.
4. Between searches, interpolate (blend) from old pose to new pose over ~0.15 seconds.

Search is the hot path. For 10k-frame DB x 30-dim features at 30 Hz: 9M float ops per search = ~0.3 ms with SIMD. KD-tree or PCA pre-projection bring DB >100k frames into reach.

### 3.2 What it replaces

- State machine with 50+ states for locomotion → 0 states.
- Blend tree per direction → irrelevant.
- Foot IK patches → still wanted, but less critical because mocap feet land naturally.

**Data shift:** you trade authoring labor (designing states, tuning transitions) for **mocap data + tagging**. You still need tags ("in combat", "weapon drawn", "low stance") to scope the search space by context — motion matching without filters will dance from ninja-roll to walking-upright randomly.

### 3.3 Shipping titles

| Game (year) | Studio | MM use | DB scale |
|---|---|---|---|
| For Honor (2017) | Ubisoft Montreal | Combat stance-to-stance + in/out combat | ~1 hr mocap |
| Madden NFL 18 (2017) | EA Tiburon | Player locomotion + tackle reactions | ~10 hr mocap |
| AC Origins (2017) | Ubisoft Montreal | Basic locomotion | ~2 hr |
| AC Odyssey (2018) | Ubisoft Quebec | Locomotion + weapon transitions | ~5 hr |
| AC Valhalla (2020) | Ubisoft Montreal | Locomotion + climb + combat | ~10+ hr |
| AC Mirage (2023) | Ubisoft Bordeaux | LMM (see §4) | learned, smaller RAM |
| AC Shadows (Nov 2024) | Ubisoft Quebec | LMM + hybrid animation-blueprint for cinematics | learned |
| The Last of Us Part II (2020) | Naughty Dog | Layered with their stack system (R4 §3) | hybrid |
| Ghost of Tsushima (2020) | Sucker Punch | Locomotion | mocap, ~hours |
| Star Wars Jedi Survivor (2023) | Respawn | Locomotion + parkour | motion match + custom IK |

Public references for non-Ubisoft: Daniel Holden's blog `https://theorangeduck.com/page/code-vs-data-driven-displacement`, `https://theorangeduck.com/page/learned-motion-matching`; Respawn's 2023 blog on Jedi Survivor; EA Motion Matching GDC 2017 "Motion Matching in The Last of Us Part II: Behind the Scenes" (later ND talk 2021).

---

## 4. Learned motion matching and neural animation

The "DB cost" problem: classical MM mocap databases are 500 MB-5 GB decompressed. Consoles have tight memory. The research response was to replace the DB with a neural network.

### 4.1 Phase-Functioned Neural Networks (PFNN)

Holden, Komura, Saito 2017, "Phase-Functioned Neural Networks for Character Control", SIGGRAPH 2017 — `http://theorangeduck.com/media/uploads/other_stuff/phasefunction.pdf` (archive `https://web.archive.org/web/2024*/theorangeduck.com/media/uploads/other_stuff/phasefunction.pdf`).

Idea: a 3-layer MLP whose weights are **interpolated** along the gait phase (0 to 2π). At each frame, compute phase φ from foot contact prediction; blend 4 expert networks' weights by Catmull-Rom on φ; forward pass gives next pose + phase increment.

- Input: previous pose (132 floats) + trajectory control (84 floats) + trajectory gait labels.
- Output: next pose (132 floats) + trajectory (84 floats) + phase update.
- Model size: ~10 MB weights. Trained on ~1.5 hours of mocap (locomotion variety incl. uneven terrain).
- Runtime: <0.1 ms on CPU for one MLP forward pass (no GPU needed).

**Limitation:** one phase axis (locomotion only). Combat, non-cyclic actions need extensions (Mode-Adaptive NN, Zhang+Starke+Komura+Saito SIGGRAPH 2018, `https://github.com/sebastianstarke/AI4Animation`).

### 4.2 Learned Motion Matching (LMM)

Holden, Kanoun, Perepichka, Popa (Ubisoft LaForge) 2020, "Learned Motion Matching", SIGGRAPH 2020 — `https://theorangeduck.com/page/learned-motion-matching` (same article + paper PDF `https://static-wordpress.akamaized.net/montreal.ubisoft.com/wp-content/uploads/2020/07/09154101/Learned_Motion_Matching.pdf`).

Three networks compress the classical MM database:

1. **Decompressor (feature → pose).** Given feature vector + latent, reconstruct the full-skeleton pose.
2. **Stepper (pose → next latent).** Advance the latent forward one frame.
3. **Projector (query → feature).** Map a search query to the closest DB feature (replaces linear scan / KD-tree).

At runtime: store only the 3 networks (~few MB). No more searching a big DB — the Projector handles it directly.

- Published results: 100x memory reduction vs classical MM at comparable quality.
- Runtime: ~0.2-0.5 ms CPU (3 small MLPs).
- Training: requires the big DB offline; you train once, ship the 3 networks.

**Shipping adoption:** AC Mirage (2023), AC Shadows (Nov 2024), rumored UE5 PoseSearch plugin took inspiration. R4 `anvil_ubisoft.md` §8 has more detail.

### 4.3 DeepMimic and physics-based skill learning

Peng, Abbeel, Levine, van de Panne 2018, "DeepMimic: Example-Guided Deep Reinforcement Learning of Physics-Based Character Skills", SIGGRAPH 2018 — `https://xbpeng.github.io/projects/DeepMimic/index.html`, paper `https://xbpeng.github.io/projects/DeepMimic/2018_TOG_DeepMimic.pdf`.

Train a physics-simulated ragdoll (PD-controlled joints) via PPO to imitate a reference mocap clip. Result: robust, physically correct characters that can absorb perturbations, adapt to uneven ground, recover from stumbles — unlike kinematic animation which just plays back.

- Training: tens of millions of env steps (hours on GPU).
- Runtime: a policy network + rigid-body simulation (Bullet in the paper). ~1 ms per character per frame.
- **Quality:** excellent for stunts (backflips, spinkicks), weak for fine-grained hand manipulation.

**AAA adoption timeline:** still mostly research as of 2026. Closest shipping tech:
- EA SEED's "Learning Locomotion Skills" demos.
- Ubisoft LaForge continues publishing (Physics-based Motion Capture Imitation with Deep RL).
- Pixar: research only, not in production renders.
- Games with "simulated locomotion": *Rain World*, *Human: Fall Flat*, *Gang Beasts*, *QWOP* use simple PD control; none are DeepMimic-scale.

**Realistic outlook:** neural animation + physics-based is the next AAA wave but still needs 3-5 years to become plug-and-play. For ALZE, it is v3 aspirational only.

### 4.4 Audio-to-face / speech-driven animation

Separate thread, covered lightly:
- Oculus LipSync SDK (Viseme-based, 2017).
- FaceFX (pre-bought by Epic, powered MetaHuman lip sync).
- NVIDIA Audio2Face (2021+, part of Omniverse, MetaHuman plugin in 2023).
- JALI (SIGGRAPH 2016) — Cyberpunk 2077 used it.
- VALL-E-like text→audio→face is research; MS VASA-1 2024 is the state of the art demo (face from single image + audio).

---

## 5. Physics-based animation and active ragdoll

### 5.1 Lineage

- **Karl Sims 1994**, "Evolving Virtual Creatures", SIGGRAPH 1994 — `https://www.karlsims.com/papers/siggraph94.pdf` (archive `https://web.archive.org/web/2024*/karlsims.com/papers/siggraph94.pdf`). Companion paper "Evolving 3D Morphology and Behavior by Competition", 1994. Foundational: simulated ragdolls whose morphology AND neural controller are evolved. No animation data whatsoever.
- **NaturalMotion Endorphin** (2003), later **Euphoria** (~2007), integrated in GTA IV (2008), GTA V (2013), Red Dead Redemption 1 & 2, Star Wars: The Force Unleashed. Uses biomechanical model of human body + goal-oriented behaviors ("protect head", "regain balance", "reach for ledge").
- **Active ragdoll** (academic term): a ragdoll driven by PD controllers trying to hit target joint angles from an animation clip. On perturbation, the controller gain drops → ragdoll wins → as target tracks, gain ramps up again. Naughty Dog's TLOU2 system is this (R4 `naughty_dog.md` §3.3).

### 5.2 Euphoria case study — GTA IV/V

Public info is sparse but known:
- GTA IV's Niko Bellic has a biomechanical model with ~60 joints, each with drive torque limits and "goals" (stand up, grab hand rail, brace for impact).
- Euphoria computes PD torques per joint each physics step (60 Hz), animation provides target poses.
- When hit: goal switches from "play animation" to "protect body." Result: drunk-looking reactions that never look the same twice — *because* they are simulated.

Rockstar paid a license per title to NaturalMotion (acquired by Zynga 2014); GTA VI (2025+) likely uses an in-house successor.

### 5.3 Muscle-driven simulation

Research only (not shipping). Wang+Geijtenbeek+Liu 2013-2020 series. Each joint has antagonistic muscles with contraction dynamics; controller activates muscles to hit joint targets. Much more realistic than torque-driven but 10x more expensive. NVIDIA PhysX 5 has "soft body" tissue sim for muscles; used in some film production (MetaHuman under the hood has tissue deformation).

### 5.4 ALZE implications

- **Active ragdoll** requires joint constraint solver (Jolt ships one; Bullet has `btGeneric6DofConstraint`; PhysX D6). Tractable: ~1500 LOC for a 15-joint humanoid ragdoll on top of Jolt.
- **Euphoria-quality** behaviors (stand-up, balance) are **years of research**. Not v1 or v2.
- **Hit reactions via active ragdoll**: feasible if physics is already integrated. The "blend out of ragdoll back to animation" step (Get-Up Animation) is the hard part.

---

## 6. Inverse Kinematics

IK = given a target in world space, compute joint angles so an end-effector reaches it.

### 6.1 CCD — Cyclic Coordinate Descent

Wang+Chen 1991, robotics origin. Canon reference in games: Mukundan 2007 in-book. Algorithm:

```
repeat until converged or N iterations:
  for each joint from end-effector up to root:
    rotate this joint so the end-effector points at the target
    (axis = cross(endToTarget, endToJoint), angle from atan2)
```

Fast (one rotation per joint per iteration), but tends to produce "elbow out" artifacts and joint limits have to be applied at each step. Good for simple chains (3-4 bones).

### 6.2 FABRIK — Forward And Backward Reaching IK

Aristidou, Lasenby 2011, "FABRIK: A fast, iterative solver for the Inverse Kinematics problem", Graphical Models 2011 — `http://www.andreasaristidou.com/publications/papers/FABRIK.pdf`.

Two-pass algorithm:
1. **Forward:** move end-effector to target. For each joint up the chain, move it to maintain the original bone length toward the next (updated) joint.
2. **Backward:** move root back to its original position. For each joint down the chain, maintain bone lengths.
3. Iterate until convergence (usually 5-15 iterations).

Faster than Jacobian methods, no trig in the hot loop (just normalize + scale), handles long chains well (spine, tentacles, tails). UE5 uses FABRIK as one of its IK solver options (the node is literally "FABRIK").

### 6.3 Jacobian-based IK

Girard+Maciejewski 1985 / many others. Compute the Jacobian J (change in end-effector per joint angle); invert (or pseudo-invert or DLS-damp) to get joint updates. Newton-Raphson style iteration.

- Pros: principled, handles multiple targets + constraints naturally, converges fast near solution.
- Cons: Jacobian inversion costs (SVD or DLS); numerically sensitive near singularities.
- Used in: Maya HumanIK (under the hood), robotics.

### 6.4 Two-bone analytic IK

The workhorse for foot and hand IK. Given hip + knee + ankle positions + target ankle position, **closed-form law of cosines** gives knee angle. ~30 lines of C++. Limitation: only works for 3-joint chains (upper leg, lower leg, foot). But that's exactly what you need for a limb.

### 6.5 UE5 Full Body IK (FBIK)

UE5's `FullBodyIK` node (2022+) is a **weighted constraint solver** over the whole skeleton. Given multiple end-effector targets (both feet + hands + head + pelvis), it solves a global optimization (typically Cyclic Coordinate Descent variant or Jacobian DLS) to respect all of them at once.

- Tuned for "full body contact" cases: ladders, climbing, gripping railings.
- ~0.2-0.5 ms per character at 60 Hz.
- Replaces older "2-bone IK + lookAt" hand-chained solutions.

Older UE4 used a stack of **2-bone IK nodes** one per limb — perfectly adequate for foot IK, inadequate for full-body contact.

### 6.6 Foot placement on uneven ground

Textbook:
1. Play locomotion animation normally to get "intended" foot positions.
2. Raycast from animated foot down to the world.
3. If raycast hit above animated foot position → lift foot up via IK to match hit.
4. Offset pelvis down by `min(left_lift, right_lift)` to keep stance natural.
5. Rotate foot to match ground normal (clamped, e.g. ±30° pitch).
6. Smooth offsets over ~0.1s to avoid popping.

Typical cost: 2 raycasts + 1 2-bone IK per foot = ~0.1 ms per character. Every AAA game with uneven terrain does this.

**Naughty Dog TLOU2** takes this further — dynamic contact IK on arbitrary surfaces (wall, rail, car hood) with multi-bone IK via their FBIK equivalent (R4 naughty_dog.md §3).

---

## 7. Cloth simulation

### 7.1 Position-Based Dynamics (PBD)

Müller, Heidelberger, Hennix, Ratcliff 2007, "Position Based Dynamics", JVCA 2007 — `https://matthias-research.github.io/pages/publications/posBasedDyn.pdf`.

Treat cloth as particles connected by constraints (distance, bending). Each frame:
1. Predict positions via integration of forces + velocities.
2. **Project constraints iteratively**: for each constraint, adjust positions to satisfy it. Converges in 4-20 Gauss-Seidel iterations.
3. Update velocities from `(pos - prev_pos) / dt`.

Simple, fast, unconditionally stable. Used in Bullet softbody, NVIDIA Flex, Havok Cloth, DOOM Eternal cape simulation.

**Limitation:** behavior is iteration-count-dependent — more iterations = stiffer cloth. This was the motivation for XPBD.

### 7.2 XPBD — Extended PBD

Macklin+Müller+Chentanez 2016, "XPBD: Position-Based Simulation of Compliant Constrained Dynamics", MIG 2016 — `https://matthias-research.github.io/pages/publications/XPBD.pdf`.

Adds a **compliance** parameter to each constraint. The stiffness is physically meaningful and independent of iteration count. This is the foundation of most modern cloth in games (Unity Cloth since 2021, Unreal Chaos Cloth since UE5, NVIDIA PhysX 5 cloth).

### 7.3 Hair — three major libraries

| Lib | Vendor | Tech | Shipping titles |
|---|---|---|---|
| **NVIDIA HairWorks** | NVIDIA | GPU-driven hair strand simulation + rasterization. Tessellation pipeline. DX11/12. | Witcher 3 (2015) Geralt's hair, FF XV |
| **AMD TressFX** | AMD | Similar to HairWorks, BSD-licensed. DirectCompute. | Tomb Raider 2013 (Lara), Deus Ex: Mankind Divided, Forza H5, Cyberpunk 2077 (hair) |
| **Unreal Groom** | Epic | First-party since UE4.26 (2020). Strand simulation + strand-level renderer w/ voxelization for self-shadow. | Matrix Awakens demo, Hellblade 2, many UE5 titles |
| **Bespoke** | Valve (Half-Life: Alyx → strand hair; not documented), ND, FromSoft (Sekiro: hair cards), RE Engine (card-based hair) | Per-studio. Often card-based (polygon strips) not strand-based. | Varies |

**Card-based hair** (used by most AAA from 2010-2022): hair is rendered as overlapping alpha-blended polygon strips. Faster than strands (thousands vs hundreds of thousands) but less realistic. MetaHuman uses strands when close, cards when far.

### 7.4 Perf budget per character

Rough numbers (2024, mid-tier GPU RTX 3070):

| Feature | Cost (ms per character) |
|---|---|
| LBS skinning (100k verts, compute) | 0.05 |
| DQS (same) | 0.08 |
| Cape/cloth XPBD (2k particles, 10 iter) | 0.3 |
| Full cloak/dress (10k particles, 15 iter) | 1.0 |
| Card-based hair (5k cards, render only) | 0.2 |
| Strand hair (200k strands, simulate+render) | 2-5 |
| Groom (Unreal, full quality) | 3-8 |

Most games budget **1-2 ms per hero character** for all of skin + cloth + hair. That's why you see 1 strand-hair hero + 20 card-hair NPCs, not 21 strand heroes.

### 7.5 Half-Life: Alyx cloth/hair

Valve's bespoke simulation for Alyx's jacket + hair. Public references are thin; Sebastian Schön's 2020 GDC "Half-Life: Alyx Art Tour" mentions it briefly. Implementation likely XPBD-style PBD on ~500 particles per cloth piece; the "Alyx jacket" is a signature look. Cheap enough to run on Index-era HMDs at 120 Hz.

---

## 8. Facial animation

### 8.1 Blendshapes (morph targets) vs joint-driven rigs

**Blendshapes:** per-vertex offset vectors, one per expression. To make a "smile", author a full mesh at "100% smile" and store `delta = smile_mesh - neutral_mesh`. At runtime `final = neutral + sum w_i * delta_i`.

- Pros: artist-direct ("sculpt the shape you want"), high fidelity, easy to combine.
- Cons: **memory**. A typical face has ~150 blendshapes x ~10k vertices x 12 bytes each = ~18 MB per head. Explodes with N expressions.

**Joint-driven (bone-based):** the face has ~30-80 facial joints (jaw, eye brows, cheek, lip corners) that deform the mesh via standard skinning. Artist authors "pose sets" (combinations of joint rotations).

- Pros: lightweight (no extra mesh data), composable with body skinning, animates via same pipeline.
- Cons: less expressive in skin wrinkles / detailed mouth shapes.

Most AAA combine: **base mesh skinned by face joints** + **corrective blendshapes** on top (per-pose corrections). MetaHuman is this pattern.

### 8.2 FACS — Facial Action Coding System

Paul Ekman 1976. Decomposes the face into ~46 Action Units (AU). Apple ARKit's "52 blendshapes" (2017+, documented at `https://developer.apple.com/documentation/arkit/arfaceanchor/blendshapelocation`) is a FACS-derived set of 52 controllable blendshape axes (jawOpen, mouthSmileLeft, eyeBlinkLeft, ...). It became de facto cross-app standard since: most modern mocap systems (iPhone Face Tracking, Live Link Face, Rokoko Smartsuit Face) emit ARKit-52.

### 8.3 MetaHuman pipeline

Epic MetaHuman Creator (2021) → MetaHuman Animator (2023) → integrated in UE5.

- Rig: ~500 bones (body + face + hands). Face has ~300 bones + ~800 corrective shapes.
- Face solve: live capture on iPhone → extract ARKit-52 → solve MetaHuman rig poses.
- Performance mode vs Cinematic mode (tradeoff bones/shapes count).

### 8.4 Audio-driven face

- **NVIDIA Audio2Face** (2021+) → emits ARKit-52 blendshape weights directly from audio waveform. ML model. Integrated with MetaHuman since 2023.
- **JALI** (Zell+Komura+Whited 2016) — "JALI: An animator-centric viseme model for expressive lip synchronization" — `https://www.dgp.toronto.edu/~elf/jali.html`. Cyberpunk 2077 used it for 10+ languages of NPC dialogue. CDPR talk: "Lip Sync in Cyberpunk 2077" GDC 2021.
- **Oculus LipSync SDK** — viseme-based, decades-old tech, still works for VR (cheap, simple phoneme mapping).

---

## 9. Procedural / secondary animation

Tricks that add life without keyframes:

### 9.1 Foot IK (see §6.6)

### 9.2 Hand IK on weapons

When rifle moves in the hands due to aim/recoil, use 2-bone IK on each hand so hands stay on grip/foregrip exactly. Cheap (~0.05 ms per hand).

### 9.3 Head-look / eye-look

Authored "look target" (usually a point in world or a bone). Skeleton's head joint is rotated via blended constraint to face the target, clamped to ±75°. Subtle: use 2 or 3 joints (spine top + neck + head) splitting the rotation to avoid "exorcist head." UE Control Rig's "Look At" node is this.

### 9.4 Spring chains for tails, hair, cloth-strip

For ponytails, tails, scarves, antenna. Each segment is a particle with a spring to its parent, integrated with Verlet or symplectic Euler. Per-frame cost negligible (<0.01 ms for 20 segments). Can be layered on top of animated bones or fully replace them.

Typical code:
```cpp
for (int s = 1; s < N; s++) {
    Vec3 target = parent[s].pos + restLocal[s];
    Vec3 vel = (segs[s].pos - segs[s].prevPos) * damping;
    Vec3 next = segs[s].pos + vel + (target - segs[s].pos) * stiffness * dt;
    segs[s].prevPos = segs[s].pos;
    segs[s].pos = next;
}
```

Adds gravity by pulling `next` downward. Add collision by projecting out of sphere colliders.

### 9.5 Breathing / idle sway

Tiny sinusoidal offsets on spine + shoulders. "Idle life" without idle animations. 20 LOC.

---

## 10. Runtime animation formats and compression

### 10.1 Ozz-animation

Guillaume Blanc's open-source runtime library (MIT, `https://github.com/guillaumeblanc/ozz-animation`), has been maintained since 2015. Industry reference for "small, fast, decouple-from-DCC" animation runtimes. Used inside: many indie engines, Amazon Lumberyard (historically), Roblox (allegedly internal integration), custom engines.

Features:
- Skeleton data structure (SoA bone hierarchy, cache-friendly).
- **Fixed-rate keyframe sampling** or **runtime sampling** with **cached cursor** (fast re-sample when playback is monotonic).
- Track compression: **key reduction** (remove keys that interpolate from neighbors within tolerance) + **quantization** (16-bit float translation, XYZ-3 unit-quaternion-compressed rotation to 3x16 bits).
- Runtime ~3-5x smaller than raw glTF. ~5 MB for ~5 minutes of 60 Hz 60-bone animation.
- Evaluation: ~0.02 ms per animation per character for a 60-bone skeleton. Full local-to-model evaluation ~0.005 ms.

Guillaume Blanc's blog (`http://guillaumeblanc.github.io/ozz-animation/documentation/`) covers internals.

### 10.2 Havok Animation Studio

Havok (Microsoft) proprietary. Used by FromSoftware (Dark Souls 1/2/3, Bloodborne, Sekiro, Elden Ring per public licensing announcements — R4 `fromsoftware.md` confirms Havok Anim use), many other AAA. Features similar to Ozz but with decade+ of AAA hardening: Havok Behavior (state machine tool), Havok Destruction. Per-title license fee.

### 10.3 glTF animation — export standard

Khronos glTF 2.0 supports animation natively (`animations[]` array, channels with TRS targets + samplers with keyframe input/output + interpolation mode LINEAR/STEP/CUBICSPLINE). Industry default for cross-tool transfer: Blender, Maya, Houdini, Substance all export. glTF is not a **runtime** format (you decode at load) but it is the de-facto **interchange** format. Ozz has a glTF importer converting to its compressed internal format.

### 10.4 FBX

Autodesk proprietary, defacto DCC-to-DCC standard. Verbose, supports everything. Not a shipping format (huge). Most pipelines: DCC → FBX → engine internal.

### 10.5 Compression literature

**Aras Pranckevičius' "Animation Compression" blog series** (2017) — `https://aras-p.info/blog/2017/09/05/Animation-Compression-Design-Notes/` + 5 follow-ups. Required reading. Covers:
- Raw key storage (float3 + quat = 28 bytes per bone per key).
- Keyframe reduction by error tolerance.
- Quantization (16-bit float, 10:10:10 octahedral normals, 48-bit quat).
- Spline fitting (Hermite / Bezier / Catmull-Rom curves — can 10x reduce storage vs linear keys).
- ACL library — Nicholas Frechette — `https://github.com/nfrechette/acl` — Animation Compression Library, MIT, deeply tuned. Industry-adopted (UE5 uses ACL since 5.0 replacing Epic's older compressor).

Numbers: raw 60Hz 60-bone 5-minute animation ≈ 60 * 60 * 300 * 28 bytes = 30 MB. ACL compresses to ~500 KB-2 MB (60x smaller) with imperceptible error.

### 10.6 ACL vs Ozz

| | Ozz | ACL |
|---|---|---|
| License | MIT | MIT |
| Compression | ~10-20x | ~30-60x |
| CPU decode | faster | slower |
| Integration | full (runtime + IO + FBX/glTF) | just compression, bring your own runtime |
| UE5 official | no | YES (replaced in 5.0) |

Many teams use ACL for compression + Ozz for runtime eval.

---

## 11. Comparison tables

### 11.1 Animation approaches — authoring / memory / CPU / fidelity

| Approach | Authoring effort | Data memory | CPU cost | Quality ceiling | Best for |
|---|---|---|---|---|---|
| **FSM + blend trees** (Mecanim/UE ABP) | High (state count explodes) | Low (10-50 MB anim) | Low (<0.1 ms) | Medium (sliding, awkward transitions) | Indie to AA, highly stylized |
| **Motion matching (classical)** | Low for locomotion, high for DB tagging | **High** (500 MB-5 GB mocap DB) | Medium (0.3-1 ms) | High (very natural locomotion) | AAA w/ mocap budget |
| **Learned motion matching** | Same as classical + train step | Low (few MB weights) | Low-medium (0.2-0.5 ms) | High (matches classical) | AAA consoles w/ mem pressure |
| **PFNN / neural character** | Medium (data + training) | Low (10 MB) | Low (<0.1 ms) | Medium-high (gait only) | Research, locomotion specialists |
| **Physics-based (DeepMimic/active ragdoll)** | Very high (RL training + physics + animation reference) | Low (policy) | High (1-3 ms physics + inference) | Very high for responsive motion | Research, Euphoria-style reactions |
| **Hybrid (AAA typical)** | Very high (all of the above) | High | Medium-high | Highest | AAA |

### 11.2 Cloth / hair tech

| Tech | Lib | HW req | Cost per char | Notes |
|---|---|---|---|---|
| PBD cloth (soft) | Bullet / DIY | any GPU | 0.1-0.5 ms | Cape, cloak, skirt. 200-2k particles. |
| XPBD cloth | Chaos / PhysX 5 / NVIDIA Flex | any GPU w/ compute | 0.2-1 ms | Modern default. |
| Card hair | DIY | any | 0.1-0.3 ms | Alpha cards, most games use this. |
| HairWorks | NVIDIA library | NV GPU (DX11+) | 1-5 ms | Witcher 3, FF XV. Legacy. |
| TressFX | AMD (BSD) | any DX11+ | 1-4 ms | Tomb Raider series, Cyberpunk. |
| Unreal Groom | Epic | DX12 / Vulkan | 2-8 ms | Most UE5 hero hair. |
| Valve Alyx strand | Bespoke | VR-class GPU | <1 ms | Small char count + tight budget. |

### 11.3 ALZE applicability — v1, v2, v3

| Feature | v1 (now, GL 3.3) | v2 (Vulkan, mid-term) | v3 (aspirational) |
|---|---|---|---|
| **Skeletal skinning** | Ozz-animation port + LBS in vertex shader. ~2k LOC integration. | Compute skinning in Vulkan. + DQS option. | - |
| **Clip playback** | Ozz sampling + blend1 + blend2 nodes. | + ACL compression integration. | - |
| **State machine** | Hand-coded FSM over Ozz sampler. 600 LOC. | + layer stack à la Naughty Dog. 1200 LOC. | - |
| **2-bone IK** | Analytic two-bone for feet + hands. 200 LOC. | + spring-based damping. | - |
| **Full-body IK** | - | FABRIK for head-look + climbs. 400 LOC. | UE-style FBIK solver (3k LOC). |
| **Cloth** | Spring chain for cape (Verlet, 1 chain, 300 LOC). | XPBD cloth, 2-3 pieces per character, ~1.5k LOC. | Chaos-style full solver. |
| **Hair** | Card-based (rendered as alpha cards from mesh). | +spring chain for ponytails. | Strand-based TressFX-style. |
| **Motion matching** | skip. | Classical MM + KD-tree + mocap DB. ~3k LOC + asset pipeline + DB curation ~30-60 days of authoring. | LMM (train Decompressor/Stepper/Projector offline). |
| **Neural animation** | skip. | skip. | PFNN/LMM if v2 locomotion feels insufficient. |
| **Active ragdoll** | Jolt PD controllers + target poses. 800 LOC. | + Euphoria-style behaviors (brace, balance). | DeepMimic-style learned controllers. |
| **Facial** | Joint-only face rig, 30 joints, manual keyframes. | + 52 ARKit-style blendshapes, audio-driven visemes. | MetaHuman-quality + A2F-style inference. |

---

## 12. v1 concrete recommendation

**Port ozz-animation + two-bone IK + spring-chain cloth.**

Scope estimate:

| Component | LOC | Time (solo dev days) |
|---|---|---|
| Ozz integration (skeleton load, clip sample, blend2) | ~1500 | 5-7 |
| Two-bone IK (foot + hand + head-look) | ~400 | 2-3 |
| Spring chain XPBD-lite (cape, ponytail, scarf) | ~600 | 3-4 |
| State machine mini-language (YAML authored, runtime eval) | ~800 | 4-5 |
| Tool: glTF animation importer → ozz | ~300 | 1-2 |
| Debug draw (bones, IK targets, cloth) | ~200 | 1 |
| Tests + example scenes | ~400 | 2-3 |
| **Total** | **~4200 LOC** | **~18-25 days** |

Budget: the "canonical" indie budget for a full animation stack is **~1 engineer-month**. Accept that this gives you a pre-2016 animation system (pre-motion-matching quality).

**Honest notes:**

1. **Motion matching is the single biggest quality leap you can make** — once you play a game with MM locomotion and then go back to blend-tree locomotion, you cannot unsee the foot sliding and awkward transitions. But MM requires:
   - A **mocap database**. Either record with a Rokoko suit (~$2500) or license clips from `motioncap.com`, RocketBox, Mixamo (free for limited cases). Minimum ~1 hour of locomotion mocap for a single locomotion set.
   - A **tagging pipeline** — every frame in the DB needs semantic tags (ground/air, left/right foot contact, stance, optional gameplay context). Manual ~5 days of work per hour of mocap.
   - **Feature extraction** offline — 200 LOC.
   - **Runtime search** — KD-tree over ~30-dim features, 300 LOC.
   - **Inertialization** (modern name for the "blend between MM picks smoothly") — Bobick/Davis 2017-style, ~400 LOC.
   - Total: ~1500 LOC engine + ~60 days asset work. This is a v2 feature, not v1.

2. **Learned motion matching is pure upside if you're already doing classical MM**: you pay the RAM tax of mocap once during training, ship 3 small networks. Worth implementing in v3 only after v2's MM is mature.

3. **DQS vs LBS** — start with LBS + shoulder helper bones (same as UE default). DQS adds ~20 GPU instructions and shader variant. Not worth it for v1.

4. **Facial animation is a separate project.** Even skipping MetaHuman-class quality, a workable face rig requires ~30-50 blendshapes or ~40 joints + artists who can author them. For v1, ship with **closed-mouth characters + audio-driven jaw-open blendshape + eye blink timer**. Anything more is v2.

5. **Active ragdoll is tightly coupled to your physics engine.** If ALZE is on Jolt (as planned in R1), you get joint constraints for free; the "drive ragdoll to target pose" logic is another 800 LOC. If physics is not a pillar of the game, skip active ragdoll entirely and ship "play getup animation after hit" instead.

6. **Animation compression is the easiest "10x improvement" in ALZE's shipping size.** Ozz compresses ~10-20x; swapping Ozz's compressor for ACL (Nicholas Frechette, MIT) gets another 3-5x. If animation data grows past 100 MB, integrate ACL. Until then, Ozz alone is fine.

7. **Primary reading order for ALZE implementation:**
   1. Ozz docs `http://guillaumeblanc.github.io/ozz-animation/documentation/`.
   2. Aras blog `https://aras-p.info/blog/2017/09/05/Animation-Compression-Design-Notes/`.
   3. Clavet 2016 GDC MM talk (even if not implementing MM — vocabulary is useful).
   4. FABRIK paper `http://www.andreasaristidou.com/publications/papers/FABRIK.pdf`.
   5. Matthias Müller XPBD paper `https://matthias-research.github.io/pages/publications/XPBD.pdf`.
   6. Naughty Dog layer-stack writeups (R4 `naughty_dog.md` §3 + Gregory's *Game Engine Architecture* 3e ch. 12).

---

## 13. References — canonical list

| Ref | Author | Year | Venue | URL |
|---|---|---|---|---|
| Phase-Functioned Neural Networks for Character Control | Holden, Komura, Saito | 2017 | SIGGRAPH | `http://theorangeduck.com/media/uploads/other_stuff/phasefunction.pdf` |
| Learned Motion Matching | Holden, Kanoun, Perepichka, Popa | 2020 | SIGGRAPH (Ubisoft LaForge) | `https://theorangeduck.com/page/learned-motion-matching` |
| Motion Matching and The Road to Next-Gen Animation | Simon Clavet (Ubisoft) | 2016 | GDC | `https://www.gdcvault.com/play/1023280/Motion-Matching-and-The-Road` |
| Motion Graphs | Kovar, Gleicher, Pighin | 2002 | SIGGRAPH | `https://research.cs.wisc.edu/graphics/Papers/Gleicher/Motion/motion-graphs-sig02.pdf` |
| DeepMimic | Peng, Abbeel, Levine, van de Panne | 2018 | SIGGRAPH | `https://xbpeng.github.io/projects/DeepMimic/2018_TOG_DeepMimic.pdf` |
| Skinning with Dual Quaternions | Kavan, Collins, Zara, O'Sullivan | 2007 | I3D | `https://www.cs.utah.edu/~ladislav/kavan07skinning/kavan07skinning.pdf` |
| FABRIK | Aristidou, Lasenby | 2011 | Graphical Models | `http://www.andreasaristidou.com/publications/papers/FABRIK.pdf` |
| Position Based Dynamics | Müller, Heidelberger, Hennix, Ratcliff | 2007 | JVCA | `https://matthias-research.github.io/pages/publications/posBasedDyn.pdf` |
| XPBD | Macklin, Müller, Chentanez | 2016 | MIG | `https://matthias-research.github.io/pages/publications/XPBD.pdf` |
| Evolving Virtual Creatures | Karl Sims | 1994 | SIGGRAPH | `https://www.karlsims.com/papers/siggraph94.pdf` |
| Evolving 3D Morphology and Behavior by Competition | Karl Sims | 1994 | Artificial Life IV | `https://www.karlsims.com/papers/alife94.pdf` |
| Animation Compression (blog series) | Aras Pranckevičius | 2017+ | aras-p.info | `https://aras-p.info/blog/2017/09/05/Animation-Compression-Design-Notes/` |
| ozz-animation library docs | Guillaume Blanc | 2015+ | github/blog | `http://guillaumeblanc.github.io/ozz-animation/documentation/` |
| ACL — Animation Compression Library | Nicholas Frechette | 2017+ | github | `https://github.com/nfrechette/acl` |
| UE5 Full Body IK docs | Epic Games | 2022+ | docs.unrealengine.com | `https://docs.unrealengine.com/5.3/en-US/full-body-ik-in-unreal-engine/` |
| UE5 PoseSearch / Motion Matching plugin | Epic Games | 2024 (5.4/5.5) | docs.unrealengine.com | `https://docs.unrealengine.com/5.5/en-US/motion-matching-in-unreal-engine/` |
| HairWorks overview | NVIDIA | 2014 | developer.nvidia.com | `https://developer.nvidia.com/hairworks` |
| TressFX | AMD | 2013 | gpuopen.com | `https://gpuopen.com/tressfx/` |
| Unreal Groom | Epic | 2020+ | docs.unrealengine.com | `https://docs.unrealengine.com/5.3/en-US/hair-rendering-and-simulation-in-unreal-engine/` |
| ARKit Blendshape Location (52 shapes) | Apple | 2017+ | developer.apple.com | `https://developer.apple.com/documentation/arkit/arfaceanchor/blendshapelocation` |
| JALI viseme model | Zell, Komura, Whited | 2016 | SIGGRAPH | `https://www.dgp.toronto.edu/~elf/jali.html` |
| NVIDIA Audio2Face | NVIDIA Omniverse | 2021+ | developer.nvidia.com | `https://developer.nvidia.com/audio2face` |
| Mode-Adaptive NN for quadruped | Zhang, Starke, Komura, Saito | 2018 | SIGGRAPH | `https://github.com/sebastianstarke/AI4Animation` |
| Animation Bootcamp TLOU2 | Reisdorf (ND) | 2021 | GDC | `https://www.gdcvault.com/play/1027094` |
| Smooth Skinning Decomposition with Rigid Bones | Le, Deng | 2014 | SIGGRAPH Asia | `https://binh.graphics/papers/2014s-ssdr/ssdr_lowres.pdf` |

Archive fallback for all the above: prefix with `https://web.archive.org/web/2024/`. Papers missing direct PDF: search via `https://dl.acm.org` proxy or author homepage.

---

## 14. Anti-patterns to avoid

1. **Implementing motion matching in v1 "because it's modern."** Classical MM is a 3k-LOC engine change on top of a 60-day asset-authoring project. Ship FSM-based locomotion first; upgrade when the DB is ready.
2. **Full-body IK instead of 2-bone IK for feet.** FBIK is 5x more code, 3x more runtime, and overkill for flat ground. Use 2-bone IK per limb until you hit a shipping-critical "full-body contact" case.
3. **Custom animation format.** Ozz's compressed format is already small and fast. Do not invent another binary layout; use Ozz's.
4. **Computing skinning on CPU.** GPU has 1000x the throughput. Either vertex-shader skinning (classic) or compute skinning (modern) — never CPU.
5. **Shipping blendshapes without delta quantization.** Blendshape vertex deltas are mostly near-zero; storing as float3 is 12 bytes/vertex. Delta-quantize to 6 bytes at 16-bit fixed point for 2x saving with zero perceptible loss.
6. **Active ragdoll without a get-up animation.** If you cannot transition from ragdoll back to animation-driven control, the character lies dead on the floor forever. Author a "wake up" clip and blend to it as soon as the ragdoll has settled (low velocity for >0.5s).
7. **Coupling animation state machine directly to game logic.** The state machine should **describe how the character moves** given inputs (velocity, aim, flags). Game logic sets those inputs. If game logic reaches into animation states directly, every gameplay change risks breaking animation.
8. **Ignoring root motion.** Root motion = the clip defines translation + rotation, not gameplay code. Using root motion for locomotion eliminates foot slide almost entirely. Most indie teams skip it because it feels unfamiliar. Accept the learning curve.

---

**End of animation.md — ~500 lines, round 5 deep dive.**
