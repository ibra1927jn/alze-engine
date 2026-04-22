# Audio Engines, Spatial Audio, Ray-Traced Audio — ALZE R5

**Round 5 — alze-engine research.** First serious audio file in this research corpus. R1 mentioned "audio: TBD" in `rendering_libs.md`, R2 mentioned Wwise in passing under FromSoftware's middleware list, R3/R4 skipped audio entirely. This file is net-new material for ALZE; nothing here has been covered before.

**Reader baseline:** knows what a sample rate is, has heard of Wwise/FMOD as "the middlewares AAA uses", has not necessarily implemented a DSP graph or convolved an HRTF. We'll stay concrete — sample counts, LOC estimates, shipping titles, licence terms.

**ALZE context reminder:** C++17, SDL2 + OpenGL 3.3 core, single developer, currently zero audio. The v1 recommendation at the end is the operative question this file answers.

---

## 0. TL;DR

- **Two commercial middlewares dominate AAA:** Wwise (Audiokinetic, now part of Sony 2022) and FMOD (Firelight). Both free under a revenue threshold (~$250k/game budget or first $200k/yr revenue). Both ship in 1000+ titles each. Wwise feels like "Pro Tools for games"; FMOD feels like "a DSP graph you can wire up".
- **Open-source layer has quietly caught up for indie-to-AA:** **miniaudio** (single-header, MIT-0/public-domain, ~90k LOC of *source* but drop-in header) is the quiet standard for 2024+ indies. **SoLoud** (zlib) for 2D/arcade. **OpenAL Soft** (LGPL) if you need HRTF convolution out of the box. libsoundio is lower-level (just I/O). cute_sound is cute_*-family, simple, single-header.
- **Spatial audio is four walled-garden standards** (Atmos, Tempest 3D, Windows Sonic, Apple Spatial) plus one open-ish standard (ambisonics / binaural over HRTF). Shipping one game cross-platform means implementing the *common denominator* (object positions + HRTF for stereo headphones) and letting the platform do the rest when you detect it.
- **Ray-traced audio shipped first at scale in Valve's Steam Audio (2017-2020, briefly dormant, back in active dev 2023+).** Microsoft Project Acoustics is the other real one, baked probes only. NVIDIA VRWorks Audio was sunset in 2022. No one ships *real-time, per-frame* ray-traced audio at AAA scale yet — baked probe-based occlusion/reverb is the practical state of the art.
- **ALZE v1 recommendation (my vote):** **miniaudio + a ~500 LOC DSP graph written in C++17 + OpenAL Soft's HRTF only if/when 3D positional audio becomes a gameplay requirement**. Total integration: ~3000 LOC including graph + streaming + format loaders. No royalties, no vendor dependency, no middleware splash screen. If the game grows into something that warrants Wwise's interactive-music hierarchy, the migration path is incremental (Wwise sits on top of a platform audio device; miniaudio can be the *fallback* while Wwise handles the authored layer).

---

## 1. Commercial middleware — Wwise and FMOD

### 1.1 Wwise (Audiokinetic)

**Founded:** 2003, Montreal. **Acquired by Sony Interactive Entertainment** in 2022 — a significant event because it made Sony the owner of the middleware that ~40% of AAA titles use, including titles for Xbox and Nintendo. Sony has publicly committed to keeping Wwise cross-platform and non-exclusive (Audiokinetic 2022, press release, https://www.audiokinetic.com/en/news/audiokinetic-joins-sony/ ). As of 2026 Wwise still ships Xbox Series X|S, Switch, Switch 2, and PC builds without restriction.

**Core architecture:**

- **Wwise Authoring Tool** (Windows-first Qt-based editor, since 2023 also on macOS). Sound designers work here, not programmers.
- **Wwise SDK** (C++ runtime, all platforms). Engine integration is ~2000-5000 LOC depending on how much of the reflection/profile/debug hookup you wire up.
- **Master Mixer Hierarchy** — hierarchical bus structure where every voice routes through a chain of busses, each bus can have effects, sends, ducking, HDR compression. This is the "mixing console" abstraction.
- **Actor-Mixer Hierarchy** — the authored content side. Sounds, random containers, sequence containers, switch containers, blend containers, music playlist containers, music segments. The graph the designer assembles.
- **Interactive Music System (IMS)** — the single feature that makes Wwise worth it for large-scope games. Music is authored as **segments** (fixed-length musical units), grouped into **playlists** (stateful sequences), with **stingers** (one-shot overlays triggered on game events) and **transitions** (rules: "from explore to combat, wait until next beat 1, crossfade 2 beats"). Used in Assassin's Creed Odyssey/Valhalla, Cyberpunk 2077, Ghost of Tsushima, Elden Ring, Baldur's Gate 3. Guy Whitmore's GDC talks on Peggle 2 and Elder Scrolls Online remain the canonical teaching references (Whitmore 2012 GDC, https://www.gdcvault.com/play/1015562/ ).
- **RTPC — Real-Time Parameter Control** — any game variable (player health, engine RPM, distance, velocity) can be mapped to any sound parameter (volume, pitch, filter cutoff, LFO rate, mix level) via a curve. This is the glue that makes vehicles sound alive, weapons sound pressured, environments sound interactive. It's *the* feature: wire a float and it drives audio.
- **States / Switches / Events** — the game-facing trigger API. Game calls `PostEvent("Play_Footstep", gameObj)`; Wwise resolves based on current state (surface: grass, gravel, metal) and switches (character: male/female/armored).

**Licensing (as of Audiokinetic's 2025 pricing page, https://www.audiokinetic.com/en/pricing/ ):**
- **Free** for projects with budget ≤ $250,000 or first $200,000 in revenue — *and* for non-commercial/academic use.
- **Indie Tier** — above free, below "full commercial". Historically $600/platform/title.
- **Commercial Tier** — negotiated, typically $5,000-$15,000 per platform per title for mid-budget, scales up for AAA.
- **Source Code** access requires a separate licence (ballpark $50-100k historically; rarely needed).

**Integration effort for a C++ SDL2+GL game:**
- Initialize MemoryMgr, StreamMgr, SoundEngine, MusicEngine, CommMgr: ~200 LOC.
- Wire a `WwiseIOHook` to your filesystem: ~300 LOC (Wwise provides a default blocking impl; production games write async).
- Per-frame `AK::SoundEngine::RenderAudio()` call: 1 line.
- Register game objects, post events, set RTPCs: ~100 LOC of glue per engine system that triggers sound.
- **Total realistic integration for a small engine: 1500-3000 LOC + substantial learn-the-tool time for the sound designer.**

**Shipping titles (non-exhaustive):** AC Valhalla/Mirage/Shadows, Cyberpunk 2077, Elden Ring, Starfield, God of War Ragnarök, Horizon Forbidden West, Returnal, BG3, Spider-Man 2, Diablo IV, Hogwarts Legacy. Public Audiokinetic showcases list thousands (https://www.audiokinetic.com/en/showcase/games/ ).

**Wwise docs canonical entry:** https://www.audiokinetic.com/library/edge/?source=SDK&id=welcome.html (Audiokinetic n.d., SDK documentation root, "Wwise SDK 2024.1 Documentation").

### 1.2 FMOD (Firelight Technologies)

**Founded:** 2002, Melbourne. Independent. **Current product line:**
- **FMOD Studio** — the authoring tool (Windows + macOS, Qt-based). Same idea as Wwise Authoring, different philosophy.
- **FMOD Engine** — the runtime (formerly "FMOD Core" + "FMOD Studio Runtime"). Single library, `libfmod.so`/`fmod.dll`.

**Core architecture:**

- **Events** — the core unit. An event is a parameterized container of sounds + effects + logic. Events are *data* authored in Studio; code triggers them by GUID or path.
- **Parameters** — global or per-event, drive DSP parameters, playback logic, automation curves. FMOD's version of Wwise RTPCs.
- **DSP Graph** — FMOD is philosophically a DSP graph first. Every event produces a sub-graph of DSPs (effects, mixers) that attaches to the master graph. You *can* drop down to the low-level DSP API and wire up arbitrary node graphs by hand — useful for custom synths / granular engines / anything not fitting the event model.
- **Transition Timelines** — FMOD's answer to Wwise's IMS. Within an event's timeline, you place **transition markers** that the playback cursor jumps to based on parameter values. Useful for interactive music but less feature-dense than Wwise's segment/playlist/stinger hierarchy. Firelight argues (credibly) that this is *simpler to reason about* — the designer sees a linear timeline with conditional jumps, not a nested graph.
- **Studio API vs Core API** — Studio is the high-level "play authored events" API, Core is the raw "load this PCM buffer and play it with this pitch + this DSP chain" API. Many indie projects use Core directly and never touch Studio.

**Licensing (Firelight 2025 pricing, https://www.fmod.com/licensing ):**
- **Indie** — free for projects with development budget < $600,000 (note: higher than Wwise's $250k — FMOD is intentionally more generous for indie).
- **Basic** — $5,000 per platform per title for dev budgets $600k-$1.5M.
- **Premium** — $15,000 per platform per title above that.
- **Educational** — free for academic/student use.
- Source code is not licensed externally.

**Integration effort:** FMOD's "hello world" is famously ~30 lines of C. A full integration for a small engine including streaming, 3D attributes, event parameters, bank loading, reverb zones: **~1000-2000 LOC**. Less than Wwise because the API surface is smaller.

**Shipping titles:** FMOD historically skews toward indie + mid-budget, but AAA presence is substantial: BioShock Infinite, Witcher 3 (yes — CDPR used FMOD for W3 and *switched to Wwise* for Cyberpunk 2077), Celeste, Hollow Knight, Hades, Subnautica, Forza Horizon series (Playground Games' internal engine + FMOD), Minecraft (Bedrock). See https://www.fmod.com/games .

**FMOD docs entry:** https://www.fmod.com/docs/2.03/api/welcome.html (Firelight n.d., "FMOD Engine 2.03 API Reference"). GDC talks: Stephan Schütze 2014, "FMOD Studio: A New Sound Engine" (https://www.gdcvault.com/play/1020388/ ).

### 1.3 Wwise vs FMOD — the honest shootout

| Axis | Wwise | FMOD |
|------|-------|------|
| Authoring tool UX | dense, powerful, steep | gentler, timeline-centric |
| Interactive music | IMS — best in industry | Transition Timelines — simpler, less powerful |
| DSP graph authoring | mostly via bus hierarchy | first-class, drop to Core API |
| RTPC / parameter curves | RTPC + attenuation curves | Parameters + automation |
| Profiler | "Wwise Authoring Profiler" — excellent | "FMOD Studio Profiler" — good |
| Indie free tier | ≤ $250k budget | ≤ $600k budget |
| Platforms | ~30 incl. all consoles + mobile | ~25 incl. all consoles + mobile |
| Source available | via paid licence | no |
| Owner | Sony (2022+) | Firelight (independent) |
| Designer learning curve | 2-3 months to fluent | 3-6 weeks to fluent |

**For AAA with a team of 3+ sound designers and a dedicated audio programmer**, Wwise wins. For **indie-to-AA with one sound designer or the programmer doubling up**, FMOD wins. For **solo developer with no budget and no sound designer**, *neither* is the right answer — you'll be paying middleware-complexity tax for content you don't have — and the open-source layer is where you should live (§ 2).

---

## 2. Open-source audio libraries

### 2.1 miniaudio (David Reid, mackron)

**The single most important audio library of the 2020s for indies.** Single-header C, ~90k LOC, MIT-0 / public-domain dual licence (pick either — the author dedicated it to the public domain and provided an MIT fallback for jurisdictions where public-domain dedication is ambiguous). Repo: https://github.com/mackron/miniaudio (Reid n.d.).

**What it gives you:**
- Cross-platform audio device I/O (WASAPI, DirectSound, WinMM on Windows; Core Audio on macOS/iOS; ALSA, PulseAudio, JACK, PipeWire on Linux; OpenSL|ES, AAudio on Android; sndio on OpenBSD; audio4 on NetBSD; OSS on FreeBSD; Emscripten/Web Audio on web).
- Decoding: WAV, MP3 (via dr_mp3), FLAC (via dr_flac), Vorbis (via stb_vorbis).
- Resampler (linear, two variants of sinc).
- A **node graph** with built-in nodes (low-pass, splitter, delay, spatializer).
- A **spatialization node** with basic HRTF (not the best quality — see OpenAL Soft § 2.3 for that — but functional).
- Engine API: fire-and-forget `ma_engine_play_sound(&engine, "footstep.wav", NULL)`. Decoder API for streaming. Device API for full control.

**Integration:** `#include "miniaudio.h"` with `#define MINIAUDIO_IMPLEMENTATION` in exactly one TU. Link to threads lib. That's it. First sound in ~20 lines including error handling.

**Limitations:**
- No authoring tool. Content is just files on disk.
- HRTF is functional but not state-of-the-art.
- No interactive music system. You'd build a state machine yourself.
- Single-header means long compile times for the implementing TU — isolate it in its own cpp file.

**Used by:** Raylib, SFML-audio backend options, hundreds of indie titles, several research projects. Chosen because it "just works" and has no external toolchain.

### 2.2 SoLoud (Jari Komppa)

Repo: https://github.com/jarikomppa/soloud (Komppa n.d.). Zlib licence. C++ with C wrapper. ~20k LOC. Philosophy: "easy to use" — the README starts with a 6-line hello-world.

**Features:**
- Multi-backend device I/O (SDL, PortAudio, WASAPI, OpenAL, CoreAudio, OpenSL|ES, XAudio2, WinMM, and more).
- Decoders: WAV, OGG Vorbis, MP3, FLAC (optional), and built-in synths (SFXR, Monotone, TED, Vic) — useful for chiptune games.
- 3D audio with distance attenuation, Doppler, cone attenuation.
- Filter chain per voice (biquad, echo, flanger, BassBoost, LoFi, WaveShaper, DCRemoval).
- "Playlist" and "speech synth" helpers.

**Limitations:**
- HRTF is rudimentary (interaural time + level differences; no true HRIR convolution).
- Development cadence has slowed since ~2019.
- No authoring tool.

**Best fit:** 2D games, arcade titles, jam games, titles where "3D audio" means "left/right pan + volume curve" and nothing more. SoLoud is charming and you'll ship faster with it than with miniaudio *if* you don't need real 3D.

### 2.3 OpenAL Soft (Chris Robinson, kcat)

Repo: https://github.com/kcat/openal-soft (Robinson n.d.). **LGPL v2.1** — important licence note: LGPL means if you statically link you need to provide object files sufficient for a user to relink. Most shipping titles dynamically link (ship `soft_oal.dll` / `libopenal.so`) to avoid the issue.

**Implementation of the OpenAL 1.1 spec plus extensions** (EFX for reverb/occlusion, AL_SOFT_HRTF for binaural rendering, AL_SOFT_loop_points, AL_SOFT_source_spatialize).

**Why it matters specifically in 2026:**
- **Best open-source HRTF implementation shipping.** OpenAL Soft embeds the MIT-Kemar and CIPIC HRTF datasets, can load SOFA files (§ 4), does per-sample HRIR convolution correctly, and handles head tracking via `AL_SOFT_source_spatialize` plus listener orientation updates.
- **Default backend for a remarkable number of Linux games.** The Steam Runtime ships OpenAL Soft. Many cross-platform engines use OpenAL Soft on Linux and platform-native APIs elsewhere.
- **Straightforward API.** Source/Buffer/Listener model, positions are `float[3]`, you set 3D position and velocity and the library does the rest.

**Limitations:**
- The OpenAL 1.1 *spec* is showing its age — no native support for higher-order ambisonics, no modern ray-traced reverb API, no authoring concept. EFX covers the basic reverb/filter cases but is 2007-era thinking.
- LGPL is a licence-auditing nuisance for commercial work even when compliant.

### 2.4 libsoundio (Andrew Kelley)

Repo: https://github.com/andrewrk/libsoundio (Kelley n.d.). MIT. ~10k LOC. **Lowest-level sensible choice**: just a cross-platform wrapper over the OS audio device APIs. No decoding, no mixing, no graph, no spatialization. You bring all of that.

Use when: you want total control, you have a custom DSP graph already, and you just need "give me a callback with `float* out, int frames`" on every OS. **Not the right answer for ALZE** — too little provided.

### 2.5 cute_sound / cute_framework (Randy Gaul)

Repo: https://github.com/RandyGaul/cute_headers (Gaul n.d.). `cute_sound.h` — single header, ~3k LOC. Zlib/Public-domain. Extremely simple: load WAV/OGG, play with pitch/volume/pan, done. For 2D games, game jams, prototypes.

### 2.6 The open-source shootout table

| Library | Licence | Platforms | 3D / HRTF | DSP graph | Format support | Integration LOC (minimal) | Integration LOC (full) |
|---------|---------|-----------|-----------|-----------|----------------|---------------------------|------------------------|
| miniaudio | MIT-0 / PD | Win/Mac/Lin/Android/iOS/Web/BSDs | basic HRTF | yes, node graph | WAV/MP3/FLAC/Vorbis | 20 | 1500-3000 |
| SoLoud | zlib | Win/Mac/Lin/Android/iOS | basic 3D (no true HRIR) | per-voice filter chain | WAV/OGG/MP3/FLAC + synths | 30 | 1000-2000 |
| OpenAL Soft | LGPL 2.1 | Win/Mac/Lin/Android/iOS | **true HRTF, SOFA loader** | EFX effects only | none built-in (bring your own decoder) | 80 (incl decoder) | 2000-3500 |
| libsoundio | MIT | Win/Mac/Lin/BSD | none | none | none | 200 (DIY) | 5000+ (DIY graph) |
| cute_sound | zlib / PD | Win/Mac/Lin | 2D pan only | no | WAV + OGG | 10 | 500 |
| Wwise | Commercial | 30+ | yes + Motion + ambisonics | full mixer + DSP | WEM (authored) + prompts | 500 | 2000-5000 |
| FMOD | Commercial | 25+ | yes + ambisonics | full DSP graph | FSB (authored) + Core formats | 30 | 1000-2000 |

Columns "minimal" = hello-world that plays a sound. Columns "full" = integration good enough for a small commercial game including streaming, 3D, reverb, save/load of mixer state.

---

## 3. Spatial audio standards — the walled gardens

Shipping "spatial audio" means five different things on five different platforms, and the *good* solution is to implement the common denominator yourself and opportunistically hand off to the platform when detected.

### 3.1 Dolby Atmos (object-based, 7.1.4+)

Dolby's cinematic-origin object-based surround format, extended to games. The authored scene is **audio objects** (each with a 3D position) + **bed channels** (traditional 7.1.4). The renderer maps objects onto whatever speaker layout the listener has — from stereo headphones to 7.1.4 home cinema (7 ear-level + 1 sub + 4 height) to 22.2 commercial theatre.

**For games:** Xbox Series X|S and Windows expose a "Dolby Atmos for Headphones" / "Dolby Atmos for Home Theater" mode. Your engine submits object positions + per-object audio streams to the platform's spatial audio API (Windows `ISpatialAudioClient`), the platform does the object-to-speaker mapping.

**Latency:** ~30 ms including OS mix. **HW requirement:** Atmos-capable AVR or Atmos-for-Headphones licence on the listener end. **Fidelity:** good for the authored case (positioned objects render with ~5° accuracy on headphones, better on speakers). Cost on game side: licensing a Dolby Atmos encoder for trailers is non-trivial; *in-game real-time rendering* via the platform API is free, Microsoft absorbs the licence.

**Canonical docs:** Microsoft "Windows Sonic and Spatial Sound" (https://learn.microsoft.com/en-us/windows/win32/coreaudio/spatial-sound ), Dolby "Dolby Atmos for Games" (https://professional.dolby.com/gaming/ , Dolby n.d.).

### 3.2 Sony Tempest 3D (PS5)

Sony's PS5-exclusive spatial audio engine. Hardware-accelerated HRTF convolution on a dedicated custom audio engine (the "Tempest Engine" — MSAA-derived compute unit repurposed for audio DSP). Positions submitted from engine → PS5 audio core → binaural output per pair of headphones.

**The trick Sony touts:** personalized HRTFs. PS5 System Software 2.0+ lets users pick from five HRTF presets; future versions (not yet shipped as of 2026) may support per-user HRTF capture via a mobile-phone photo of the ear.

**Latency:** ~20 ms (dedicated hardware, low). **HW requirement:** PS5 only. **Fidelity:** very good for headphones (best-in-class binaural on stock HRTFs). Cost on game side: trivial — set 3D positions via PS5 audio API, Sony does the rest. Documented only under NDA in Sony's PS5 SDK; GDC 2020 Mark Cerny road-to-PS5 talk covered the architecture publicly (https://www.youtube.com/watch?v=ph8LyNIT9sg , Cerny 2020).

### 3.3 Microsoft Spatial Sound — Windows Sonic + Dolby Atmos + DTS:X

A single API (`ISpatialAudioClient`, Windows 10+) that backends to whichever spatial format the user has configured: Windows Sonic (free, HRTF-based), Dolby Atmos (paid licence on listener end), DTS:X (paid licence on listener end). Your engine submits objects, Windows renders to whichever spatial format is active.

This is **the right abstraction for cross-platform Windows games**: you don't care which format the user picked; you just emit object positions and trust Windows. Latency ~30-40 ms, HW requirement Win10+, fidelity varies by backend.

Primary doc: https://learn.microsoft.com/en-us/windows/win32/coreaudio/spatial-sound (Microsoft n.d., "Spatial sound"). Sample code: https://github.com/microsoft/Windows-universal-samples/tree/main/Samples/WindowsAudioSession .

### 3.4 Apple Spatial Audio

Apple's spatial audio for AirPods (Pro, Max, 3rd gen+) and Apple Silicon Macs. Head-tracked binaural via HRTF convolution with dynamic listener rotation from the AirPods' IMUs. **On iOS / macOS, exposed through PHASE (Apple's spatial audio engine)** and AVFoundation's `AVAudioEnvironmentNode`.

**Relevant for games via:** AVAudioEngine's `AVAudioEnvironmentNode` positions sources in 3D; the OS handles HRTF + head tracking on Apple-Silicon + AirPods combos. Fallback is stereo pan on non-AirPods listeners.

**Latency:** ~30 ms. **HW requirement:** iOS/macOS + AirPods for head tracking; basic binaural works on any headphones. **Fidelity:** very good, with head tracking it's the most immersive consumer spatial audio shipping.

Docs: Apple PHASE framework (https://developer.apple.com/documentation/phase , Apple n.d.). WWDC 2021 talk "Discover spatial audio" (https://developer.apple.com/videos/play/wwdc2021/10265/ ).

### 3.5 Meta Oculus Spatializer

The VR-first spatial audio plugin shipped by Meta (formerly Oculus). Runs as Wwise / FMOD / Unity / Unreal plugin. Does HRTF convolution, early reflections, basic reverb, ambisonics decoding. Released as "Oculus Audio SDK" since 2015; rebranded "Meta XR Audio SDK" in 2022.

**Why it still matters in 2026:** VR games shipped on Quest 2/3/Pro use this by default. Cross-platform VR titles often author against Meta XR Audio SDK because it's the common denominator across Quest + PCVR.

Docs: https://developers.meta.com/horizon/documentation/unity/meta-xr-audio-sdk-overview (Meta n.d., "Meta XR Audio SDK").

### 3.6 Cross-platform strategy

The pragmatic shipping pattern, used by Ubisoft/EA/Activision and documented in various GDC talks, is:

1. Author audio with 3D positions, velocities, and orientations.
2. Render a **default binaural mix** yourself using any HRTF implementation (OpenAL Soft, Wwise's built-in, FMOD Resonance Audio plugin, Meta XR Audio SDK) — this is what plays on non-spatial-audio users.
3. When the platform signals "spatial audio available" (Windows Sonic active, PS5 Tempest, Apple Spatial with AirPods), **switch to submitting objects** via the platform API and let the platform replace your binaural mix.
4. Ambisonic ambiences (3rd order or higher, authored in Reaper or Wwise or DAWs with Facebook 360 Spatial Workstation) get decoded to whatever output format.

This way the game "just works" cross-platform; the user who invested in an Atmos AVR gets Atmos, the AirPods user gets head-tracked Apple Spatial, the guy on desktop speakers gets a competent stereo mix.

### 3.7 Spatial audio tech comparison table

| Tech | Approach | Latency | HW requirement | Fidelity | Notes |
|------|----------|---------|----------------|----------|-------|
| Dolby Atmos (Xbox/PC) | object + bed, platform renders | 30 ms | Atmos-compat device | high | licence paid by platform |
| Sony Tempest 3D (PS5) | HRTF, dedicated DSP HW | 20 ms | PS5 | very high (headphones) | Sony-exclusive |
| Windows Sonic HRTF | HRTF, software | 30-40 ms | Win10+ | medium | free tier |
| Apple Spatial Audio | HRTF + IMU head tracking | 30 ms | AirPods+iOS/macOS | very high | head tracking is killer feature |
| Meta XR Audio SDK | HRTF + early reflections | 20 ms | Quest / PCVR | high | VR-first |
| Steam Audio (Valve) | ray-traced reverb/occlusion + HRTF | variable (20-50 ms) | any PC | high (with baked scene) | open source, cross-platform |
| OpenAL Soft HRTF | HRTF, software | 30 ms | any | medium-high | SOFA-file loadable, free |
| Generic pan/volume | L/R pan + distance atten | <5 ms | any | low | "classical" 2D-plus-distance |

---

## 4. HRTF basics

**HRTF = Head-Related Transfer Function.** The frequency response of the "filter" formed by your head, torso, and pinnae (outer ears) when a sound arrives from a given direction. Measured by putting microphones in a subject's ear canals and playing impulses from known angles. The time-domain equivalent is the **HRIR** (Head-Related Impulse Response) — the convolution kernel.

Per direction (azimuth θ, elevation φ), you have **two** HRIRs — one for left ear, one for right. Convolving a mono source with those two kernels gives a binaural stereo pair that recreates the directional cues of a sound arriving from (θ, φ).

### 4.1 The SOFA format

**SOFA = Spatially Oriented Format for Acoustics.** AES69-2015 standard, HDF5-based. The de facto exchange format for HRTF datasets.

A SOFA file contains: a grid of (azimuth, elevation, distance) sampling points, and for each point, a stereo pair of impulse responses (typically 128-1024 samples at 44.1 or 48 kHz). Plus metadata: subject ID, head measurements, sampling methodology.

**Free/open HRTF databases:**
- **CIPIC HRTF Database** — UC Davis, 45 subjects, 1250 directions each, 200-tap HRIRs at 44.1 kHz. https://www.ece.ucdavis.edu/cipic/spatial-sound/hrtf-data/ (Algazi, Duda, Thompson & Avendano 2001, UC Davis CIPIC, canonical paper: "The CIPIC HRTF Database", IEEE WASPAA 2001).
- **ARI HRTF Database** — Austrian Acoustics Research Institute, 150+ subjects, 1550 directions. https://www.oeaw.ac.at/isf/das-institut/software/hrtf-database
- **MIT KEMAR** — 1 artificial head (KEMAR mannequin) measured exhaustively. Historically the "default" HRTF if you had to pick one for everyone. https://sound.media.mit.edu/resources/KEMAR.html
- **SADIE II** — University of York, 20 heads incl the KU-100 dummy. https://www.york.ac.uk/sadie-project/database.html

### 4.2 Real-time cost

At 48 kHz with a 256-tap HRIR (common): 2 ears × 256 taps × 48,000 Hz = **24.5 million MACs per source per second**. A modern CPU doing SIMD (AVX2: 8 floats per instruction) does this in ~3 million ops/sec per source, which is trivial. **You can spatialize 50-100 sources on one CPU core**; more with FFT-based fast convolution (O(N log N) vs O(N²)).

**FFT-based convolution** using the overlap-save method with block size 128 samples lets you process ~200+ sources on a single modern core. Libraries to steal this from: Steam Audio's implementation, OpenAL Soft's `HrtfState::mix`, JUCE's `dsp::Convolution`, Meta XR Audio SDK.

### 4.3 Interpolating between samples

Real HRTFs are *sparse* — 1000-2000 directions, but you need arbitrary (θ, φ). Two options:

1. **Nearest-neighbour** — pick the closest measured direction. Fast, audible zippering when a source moves.
2. **Bilinear (triangular) interpolation** on the measurement grid. Interpolate between three neighbours weighted by barycentric coordinates. Much smoother, still cheap. Steam Audio does this.

A subtlety: interpolate **in frequency domain** (FFT of HRIR, interpolate magnitudes + phases), not time domain — naive time-domain interpolation can create comb filtering. Algazi & Duda 2011, "Headphone-Based Spatial Sound" IEEE Signal Processing Magazine, covers this.

### 4.4 Personalization

Generic HRTFs work OK-ish but the pinna-specific notches (the cues the brain uses for front-back and elevation) are highly individual. A wrong HRTF causes "in-head localization" — the sound feels stuck inside your skull rather than externalized. Personalization options:

- **Ear scan** (photo → ML model → predicted HRTF) — shipping in Apple, prototyped in PS5 (not yet deployed).
- **User-selectable presets** — Sony's five PS5 presets, OpenAL Soft's multiple SOFA files.
- **Behavioural tuning** — "which of these four samples sounds in front of you?" A/B test. Used by some VR apps.

For a single-dev engine, the right answer is **ship one decent SOFA** (KEMAR or MIT-average) and expose a preset dropdown if you ever need more. Don't try to build personalization.

---

## 5. Ray-traced audio

Ray-traced audio uses geometric ray tracing against the same (or a lower-LOD) scene to compute **occlusion** (is there geometry between source and listener?), **obstruction** (partial path blockage), **early reflections** (rays that bounce once or twice before reaching listener, simulating discrete echoes), and **late reverb** (statistical ray accumulation giving the reverb tail's envelope and spectrum).

### 5.1 Valve Steam Audio

**The most prominent ray-traced audio SDK in 2026.** Open source since 2017 (original Steam Audio 1.x), rewritten 2020 (Steam Audio 2.x, permissive licence, https://github.com/ValveSoftware/steam-audio — Valve Corporation n.d., Apache 2.0). After a dormant period 2021-2022, Valve resumed active development in 2023 and added Unreal/Unity plugins plus a baking toolchain.

**Features:**
- HRTF convolution (Valve licenses the SADIE HRTF set and embeds it).
- **Scene geometry import** — your engine submits triangles (or a lower-poly audio geometry proxy), Steam Audio builds a BVH.
- **Real-time path tracing for reflections + diffraction.** Configurable ray budget per frame (typically 512-4096 rays per listener). Rays propagate through geometry with specular bounces + Lambertian scattering.
- **Baked probes** for fixed geometry — probes placed in the scene at author time, each storing a precomputed impulse response for "source at probe location, listener at probe location". At run time interpolate between nearby probes.
- **Ambisonics decoding** to whatever output layout.
- Plugins for Unity, Unreal, FMOD, Wwise.

**Shipping titles:** 
- **Amazon's New World (2021)** — Steam Audio baked probes for reverb across Aeternum (~36 km² map). Per their GDC 2022 audio postmortem, baking 10,000 probes took ~8 hours on a server farm (https://www.gdcvault.com/play/1027956/ , Amazon Game Studios 2022).
- **Half-Life: Alyx (2020)** — Steam Audio for real-time occlusion + reverb in VR. Valve's own showcase title.
- **The Lab, Aperture Hand Lab** — Valve VR demos.
- **Dozens of VRChat worlds** — Steam Audio is the default audio backend for VRChat's built-in world authoring.

**CPU cost:** a Steam Audio "Reflections" effect with 2048 rays, 4 bounces, 16 ms IR length on a 2024 CPU (Ryzen 7 7800X3D) costs ~2-3 ms/frame. On GPU (via Radeon Rays integration) it's ~0.5-1 ms. That's per-listener — single-player games only pay this once.

### 5.2 Microsoft Project Acoustics

**Baked-probe-only system**, not real-time path traced. Microsoft Research publication + SDK, first shipped in *Gears of War 5* and later *Microsoft Flight Simulator 2020*.

Approach: the author places probes throughout the level (or lets the tooling auto-place on a grid). Offline, on Azure-hosted compute, MS Acoustics simulates wave-equation-based acoustic propagation (not just rays — actual FEM/BEM solvers for low frequencies, plus rays for high) from each probe. The result: a compressed per-probe encoding of how sound propagates through the space. Runtime is cheap: interpolate between probes, apply the encoded filter.

**Strengths:** handles diffraction correctly (rays can't), handles low-frequency wave effects (rays can't), offloads heavy compute to bake time.

**Weaknesses:** static geometry only. If a door opens/closes, the acoustics don't update. Baking is expensive ($$ on Azure).

**Shipping titles:** Gears 5 (2019, launch title), Flight Simulator 2020, Watch Dogs: Legion (experimental), Sea of Thieves (limited use).

Docs: https://learn.microsoft.com/en-us/gaming/acoustics/what-is-acoustics (Microsoft Research n.d., "Project Acoustics"). GDC 2018 talk: Raghuvanshi & Snyder, "Wave-based Sound Propagation for VR Applications" (https://www.microsoft.com/en-us/research/publication/parametric-wave-field-coding-for-precomputed-sound-propagation/ — Raghuvanshi et al. 2014 TOG paper).

### 5.3 NVIDIA VRWorks Audio — sunset 2022

NVIDIA's ray-traced audio SDK using the GPU's RT cores for audio ray tracing. Real time, dynamic scene support. **Officially sunset** in 2022 (NVIDIA redirected VRWorks resources to Omniverse and DLSS). Original docs now only available via web.archive.org — e.g. https://web.archive.org/web/2022*/https://developer.nvidia.com/vrworks/vrworks-audio (NVIDIA n.d., archived).

Its IP lives on informally in parts of NVIDIA Audio2Face and in Project Anari (the scene-description framework).

### 5.4 The ray-traced audio state of the art in 2026

- **Baked probes** (Project Acoustics, Steam Audio's baked mode) — shipping at scale, proven, low runtime cost.
- **Hybrid baked + real-time** (Steam Audio's combined mode) — used by Alyx, Amazon New World for occlusion on dynamic elements.
- **Full real-time path tracing for audio** — still experimental. Research projects like **GSound** (Schissler, Mehra & Manocha, UNC, https://gamma.cs.unc.edu/GSOUND/ ), **Nvidia Remix's audio experiments**, **Resonance Audio's legacy successor work at Meta**. Not shipping at AAA scale yet; the CPU/GPU budgets go to graphics.

### 5.5 Ray-traced audio comparison

| System | Status | Approach | HW | Shipping titles |
|--------|--------|----------|-----|-----------------|
| Steam Audio 4.x | active | RT + baked probes + HRTF | CPU, GPU optional | Alyx, New World, VRChat |
| Project Acoustics | active | baked wave-sim probes | CPU runtime, Azure bake | Gears 5, MSFS 2020 |
| VRWorks Audio | sunset 2022 | RT on RT cores | NVIDIA RTX | none mainstream |
| GSound | research | RT + diffraction | CPU | research demos |
| Wwise Reflect | active | ray-traced early reflections | CPU | various Wwise titles |
| FMOD Resonance Audio | active | ambisonics + simple reverb | CPU | various FMOD titles |

---

## 6. Procedural audio + DSP

### 6.1 Physically-based foley

Traditional foley (footsteps, cloth rustle, weapon handling) is sample-library-based — a few hundred recorded clips per surface × character × intensity, randomized at playback. **Procedural foley** generates these from physical parameters.

- **Footsteps:** impact model (mass, velocity, material damping) + surface model (grass: stochastic high-frequency noise burst; stone: short impulsive + longer decay). Perry Cook's **PhySynth / PhISEM** framework (Cook 2002, *Real Sound Synthesis for Interactive Applications*, A K Peters) is the canonical reference. Adopted in simplified form in Wwise (the "Impacts" plugin) and FMOD (the "Impact" DSP).
- **Cloth / chain:** modal synthesis — a sum of decaying sinusoids tuned to physical resonant modes. Microsoft Research's "Modal synthesis for complex sound models" (van den Doel et al. 2001, http://www.cs.ubc.ca/~kvdoel/publications/ ).
- **Fire / water:** granular + filtered noise, parametrized by flow rate / flame size.

**In shipping games:** used sparingly. Naughty Dog (TLOU2) uses procedural cloth foley blended with samples. Red Dead Redemption 2's horse harness sounds are partially procedural. Most titles stick with samples because designers trust what they can hear in the editor, not what the physics model produces at runtime.

### 6.2 Granular synthesis

Slicing a source sample into small **grains** (5-100 ms) and replaying them with randomized pitch/position/density/envelope. Produces textures: wind, rain, distant crowd, engine drone, alien ambience.

**Shipping uses:** *No Man's Sky*'s audio team built a granular synth engine in Wwise for procedural planetary ambiences (Paul Weir, GDC 2016, https://www.gdcvault.com/play/1023215/ — Weir 2016, "The Sound of No Man's Sky"). Each planet biome has a set of grains; the cloud of active grains is seeded by biome + time-of-day + weather. Also used in *Spore*'s creature vocalizations and *Cyberpunk 2077*'s crowd chatter.

**Implementation cost:** 300-800 LOC for a basic granular synth. Real-time cost: cheap (a few granular streams at 100 grains/sec total).

### 6.3 Generative / adaptive music

Music that *reacts* to gameplay in richer ways than picking between stems:

- **Monolith Soft's *Xenoblade Chronicles* series** — leitmotif-keyed adaptive playback, where character proximity + combat state select which instruments layer in.
- **Sam Lake & Petri Alanko's Alan Wake 2** — licensed tracks and original score interlock with gameplay via vertical stems + cinematic stingers.
- **Richard Vreeland's *Mini Metro* / *Hyper Light Drifter*** — procedurally generated ambient music keyed to game state.
- **Brian Eno's *Spore* generative main theme** — famously one of the earliest AAA uses of truly generative (not just adaptive) game music.

For ALZE: unless you have a composer partner, stick to vertical stems (§ 8). Generative composition engines (Melodrive, AIVA, Ecrett) exist but are still awkward and the results sound generative.

---

## 7. Voice and dialogue systems

### 7.1 The AAA dialogue pipeline

- **Script → casting → studio recording** in 1-40+ languages. AAA titles often record 20-40k lines per language for the main character alone (BG3 recorded ~170k lines total per language across all characters for Larian's 2023 release; Cyberpunk 2077 ~100k per language per gender for V).
- **Wwise Speech** / **FMOD dialogue banks** — line lookup by ID, localized bank selection at runtime based on language setting.
- **Lip sync:** phoneme extraction from audio (offline), phoneme-to-viseme mapping (runtime, driven by facial rig blendshapes). Tools:
  - **Audiokinetic Wwise ShotGrid integration** — for Wwise Speech pipelines.
  - **Rhubarb Lip Sync** (open source, MIT, https://github.com/DanielSWolf/rhubarb-lip-sync — Wolf n.d.) — phoneme extraction, viseme output. Used in countless indie and some AAA projects.
  - **OVR Lipsync / Oculus LipSync** — Meta's real-time viseme extraction from audio, was OSS for a while, currently Meta XR Audio SDK component.
  - **JALI** — commercial procedural lipsync used in Witcher 3, Cyberpunk 2077, Mass Effect Andromeda. Paper: Edwards et al. 2016 SIGGRAPH, "JALI: An Animator-Centric Viseme Model for Expressive Lip Synchronization" (https://www.dgp.toronto.edu/~elf/jali.html ).
  - **NVIDIA Audio2Face** — neural network audio→face blendshapes, real-time on RTX. Shipping in Star Wars Outlaws and others.

### 7.2 Bark systems

"Barks" are short context-sensitive spoken lines triggered by AI state (guards noticing the player, squadmates reacting to events). Canonical bark systems:
- **Spider-Man (Insomniac)** — see r4/insomniac.md; citizens and enemies have massive bark banks triggered by perception events.
- **Red Dead Redemption 2** — Rockstar recorded ~500,000 lines of ambient NPC dialogue for RDR2 per the Digital Foundry / Rockstar postmortem interviews.
- **Middle-earth: Shadow of Mordor / Shadow of War** — Nemesis system barks, uniquely branching per-orc.

Implementation-wise, barks are just events with sophisticated filtering: cooldowns, priority, per-NPC de-duplication, contextual branching on game state.

### 7.3 Localization

Major AAA ships in **8-13 full voiceover languages** typically: English, French, Italian, German, Spanish (ES + LatAm), Brazilian Portuguese, Russian, Japanese, Simplified Chinese, Traditional Chinese, Korean, Polish. Plus text-only for 20-30 more. Wwise's language-banks model and FMOD's localized event model are both designed around this: load the user's language bank at startup, events resolve to that language's take automatically.

For ALZE: probably ship English + one or two other languages in v1, with subtitles in more. Text-only localization is ~10% of the cost of voiced.

---

## 8. Interactive music systems

### 8.1 Vertical layering (additive stems)

Compose a piece with **N parallel tracks** (rhythm, strings, brass, percussion, choir, FX). All play in sync, but each layer's volume is driven by a game state parameter. "Combat intensity 0.3 → percussion in, choir at half, brass out." Smooth crossfades because everything is in the same tempo/key.

Used in: Breath of the Wild, Elden Ring, Horizon series, Overwatch.

### 8.2 Horizontal switching (event transitions)

A piece has **multiple musical segments** (A, B, C, bridge, ending). On a game event, transition from current segment to target segment, respecting musical rules (wait until bar boundary, beat 1, next downbeat, or "immediate" for emergencies). Crossfade, cut, or play a transition piece.

Used in: every Wwise title with interactive music, notably Doom Eternal (Mick Gordon's combat music transitions), Cyberpunk 2077 (Keanu briefcase scene dynamic shifts).

### 8.3 Stingers

One-shot overlays triggered by game events (boss reveal, objective completion, death). Musically composed to fit on top of any current state — harmonically neutral or explicitly matched per state.

### 8.4 Wwise Interactive Music Hierarchy

Five levels of authored data:
1. **Music Playlist Container** — stateful sequence of segments.
2. **Music Switch Container** — state-driven selection between playlists (one per "music state": explore, combat, stealth).
3. **Music Segment** — a fixed-length musical unit, beat/bar grid, entry/exit markers, stingers can target it.
4. **Music Track** — vertical stems within a segment.
5. **Music Clip** — individual audio files within a track.

Transitions between Music Switch Container states are rules: source state × target state × sync mode × fade time × optional transition segment. Wwise's authoring UI lets you define this as a matrix. This is where Wwise really earns its keep.

### 8.5 FMOD transition timelines

An FMOD event has a single timeline. Place **transition markers** on it (named positions + conditions). When a named parameter changes, the playhead jumps to the matching marker at the next sync boundary. Less structured than Wwise's IM hierarchy, but simpler to understand — you see the whole thing as one horizontal timeline.

---

## 9. Format support

### 9.1 The codec landscape

| Format | Compression | CPU cost (decode) | Licence | Use |
|--------|-------------|-------------------|---------|-----|
| PCM WAV | none | ~0 | free | short sfx, everything <5 s |
| IMA ADPCM / DVI | 4:1 | low | free | short sfx on constrained platforms |
| Vorbis | ~10:1 | medium | BSD-like (Xiph) | music, long sfx |
| Opus | ~10:1, better <10kbps | medium | BSD-like, RFC 6716 | voice, music, streaming |
| MP3 | ~10:1 | low | **patent-free since 2017-04-16** (Fraunhofer patents expired) | legacy content |
| FLAC | ~2:1 lossless | low | Xiph/BSD | archival, high-budget music |
| AAC | ~10:1 | low | patented (licence req) | mobile, Apple ecosystem |
| Platform-specific (XMA, Vorbis-WEM, ATRAC9) | varies | varies | per-platform | Wwise/FMOD authored output |

**For a single-dev engine in 2026:** use **WAV for short sfx** (decompressed fully on load, trivial to mix), **Vorbis or Opus for music and streaming speech** (good ratio, free licence, mature decoders). MP3 is no longer contentious (patents expired) but there's no reason to prefer it over Vorbis for new content.

### 9.2 Decoder cost

At 48 kHz stereo:
- **WAV:** 0 CPU, just read from buffer.
- **Vorbis:** ~1-3% of one CPU core per simultaneous stream (stb_vorbis).
- **Opus:** ~1-2% per stream (libopus).
- **MP3:** ~0.5-1% per stream (dr_mp3).
- **FLAC:** ~1-2% per stream (dr_flac).

For a modern engine streaming 20 simultaneous sources, decoder cost is ~30% of one core at worst — trivial on any modern CPU, noticeable on mobile. Amortize by decoding in a worker thread and feeding the audio mixer a ring buffer of PCM.

### 9.3 Streaming from disk

Long audio (music, ambience, dialogue lines >10 s) should stream, not be decoded fully into RAM. Pattern:
1. Open file, seek to start.
2. Background thread reads 64-256 KB chunks, decodes, pushes PCM frames into a ring buffer (2-4 seconds of PCM).
3. Audio callback consumes from ring buffer.
4. When ring buffer < N% full, background thread reads more.
5. Latency to start: ~100-300 ms (first chunk read + decode). Perceptible for on-demand dialogue; acceptable for music. Hide with a pre-loaded short "attack" sample if needed.

All three (miniaudio, FMOD, Wwise) handle this internally; for a DIY DSP graph budget 300-500 LOC for the streaming subsystem.

---

## 10. ALZE applicability — v1 / v2 / v3 roadmap

| Capability | v1 (now, single dev, no audio yet) | v2 (+spatial, +more sfx) | v3 (aspirational, ray-traced) |
|------------|------------------------------------|--------------------------|-------------------------------|
| Device I/O | miniaudio | miniaudio | miniaudio (or SDL_mixer?) |
| Decoding | WAV + Vorbis (stb_vorbis) | +Opus (libopus) | +streaming FLAC for music |
| Mixer / DSP graph | custom ~500 LOC | expand to ~1500 LOC with buses + sends | add convolution reverb node |
| 3D positional | distance atten + pan only | **OpenAL Soft HRTF** (or miniaudio spatializer) | Steam Audio plugin |
| Reverb | one parametric reverb (Freeverb-style, ~300 LOC) | zone-based reverb per map area | Steam Audio baked probes |
| Occlusion | none | simple ray cast → lowpass cutoff | Steam Audio occlusion rays |
| Music | vertical stems, 2-4 layers | +horizontal transitions between segments | full interactive music via Wwise? |
| Dialogue | short WAV clips, one at a time | streamed Vorbis per line, localization banks | JALI / Audio2Face lipsync |
| Platforms | Win + Linux | +macOS +Web (Emscripten) | +consoles if funded |
| Total LOC | **~2500-3500** | **~5000-7000** | **~8000-12000 + middleware** |

### 10.1 v1 concrete stack (recommendation)

**Stack:**
- **miniaudio 0.11+** — device I/O, format decoding, streaming, resampling. Single header, zero build-system drama.
- **Custom C++17 DSP graph** (~500 LOC) with nodes: `MixerNode`, `LowpassNode` (biquad), `HighpassNode`, `ReverbNode` (Freeverb-derived, ~300 LOC), `DistanceAttenNode` (distance + pan), optional `SpatializerNode` (miniaudio's built-in for now).
- **Event/Trigger layer** (~500 LOC) — game code calls `audio.play("footstep_grass", pos, vel)`; the layer looks up asset, picks a variation, applies mixer bus, does the distance atten, feeds into the graph.
- **Resource layer** (~300 LOC) — hot-reload-friendly asset table mapping string IDs → decoded buffers or streaming file handles.
- **Music state machine** (~200 LOC) — two-to-four vertical stems + volume crossfades driven by game state floats. Not a full IM hierarchy — just "combat intensity 0→1 → percussion crossfades in over 2 s".

**Total:** ~2500-3500 LOC. Ship in weeks, not months. No royalties. No splash screen. No vendor lock-in.

**Trade-off with Wwise (the honest comparison):**
- **You lose:** world-class authoring tool for sound designers, battle-tested IM hierarchy, mature profiler, the ability to say "we use Wwise" in marketing.
- **You gain:** code you fully own, zero external tool dependency, deterministic behaviour across platforms, no licensing friction for Asian/Chinese markets (Wwise's SIE acquisition introduced some uncertainty about Chinese government posture toward SIE-owned tools, though as of 2026 there's no concrete restriction — worth monitoring).
- **Upgrade path if needed:** miniaudio can remain your device I/O layer while Wwise sits on top for authored content. Or switch entirely — a v1 ALZE audio subsystem with clean interfaces (Event/Trigger layer abstracting the backend) can be replaced without touching game code.

### 10.2 v2 adds spatial audio

When the game grows to "3D positional audio affects gameplay" (stealth mechanics, combat awareness by ear, VR), add:
- **OpenAL Soft** as the HRTF backend — replace miniaudio's built-in spatializer with AL sources that use `AL_SOFT_source_spatialize = AL_TRUE` and let OpenAL Soft do real HRIR convolution from a loaded SOFA file (MIT KEMAR default).
- **Ambisonic beds** for environment ambience — author in Reaper with Facebook 360 Spatial Workstation plugins (free), decode at runtime with 3rd-order ambisonics → binaural.
- **Zone-based reverb** — each map area has a reverb preset (concert hall, small room, forest, tunnel); cross-fade between zones as the listener moves.
- **Platform hand-off hooks** — detect Windows Sonic / PS5 Tempest / Apple Spatial at startup and route through the platform API when available instead of the internal HRTF. Game code doesn't change; only the output stage does.

### 10.3 v3 aspirational ray-traced audio

If ALZE ever reaches "the game genuinely needs ray-traced audio" (big reverb-rich spaces, precise occlusion as a gameplay cue, VR-first), **integrate Steam Audio as a plugin**. Steam Audio's Unity/Unreal plugins are not re-usable in a bespoke engine, but the C API (https://valvesoftware.github.io/steam-audio/doc/capi/ ) is well-documented and usable standalone. ~2000-5000 LOC to integrate scene submission + baking toolchain + runtime probe interpolation.

Alternative: Project Acoustics (also has a standalone C++ SDK), better for large static environments, worse for dynamic geometry.

---

## 11. Primary references

Complete list, author/year/venue/URL format requested by the user:

- Audiokinetic. n.d. "Wwise SDK 2024.1 Documentation." Audiokinetic Inc. https://www.audiokinetic.com/library/edge/?source=SDK&id=welcome.html
- Audiokinetic. 2025. "Wwise Pricing." Audiokinetic Inc. https://www.audiokinetic.com/en/pricing/
- Audiokinetic. 2022. "Audiokinetic Joins Sony Interactive Entertainment." Press release, Feb 2022. https://www.audiokinetic.com/en/news/audiokinetic-joins-sony/
- Firelight Technologies. n.d. "FMOD Engine 2.03 API Reference." https://www.fmod.com/docs/2.03/api/welcome.html
- Firelight Technologies. 2025. "FMOD Licensing." https://www.fmod.com/licensing
- Schütze, Stephan. 2014. "FMOD Studio: A New Sound Engine." GDC 2014. https://www.gdcvault.com/play/1020388/
- Reid, David. n.d. "miniaudio." GitHub repository. https://github.com/mackron/miniaudio
- Komppa, Jari. n.d. "SoLoud." GitHub repository. https://github.com/jarikomppa/soloud
- Robinson, Chris (kcat). n.d. "OpenAL Soft." GitHub repository. https://github.com/kcat/openal-soft
- Kelley, Andrew. n.d. "libsoundio." GitHub repository. https://github.com/andrewrk/libsoundio
- Gaul, Randy. n.d. "cute_headers / cute_sound." GitHub repository. https://github.com/RandyGaul/cute_headers
- Microsoft. n.d. "Spatial sound." Microsoft Learn. https://learn.microsoft.com/en-us/windows/win32/coreaudio/spatial-sound
- Microsoft Research. n.d. "Project Acoustics." https://learn.microsoft.com/en-us/gaming/acoustics/what-is-acoustics
- Raghuvanshi, Nikunj, and John Snyder. 2014. "Parametric Wave Field Coding for Precomputed Sound Propagation." ACM TOG 33(4) (SIGGRAPH 2014). https://www.microsoft.com/en-us/research/publication/parametric-wave-field-coding-for-precomputed-sound-propagation/
- Raghuvanshi, Nikunj. 2018. "Project Triton: Pre-computed Wave Simulation for Immersive Audio." GDC 2018. https://www.gdcvault.com/play/1025010/
- Cerny, Mark. 2020. "The Road to PS5." PlayStation technical presentation, March 2020. https://www.youtube.com/watch?v=ph8LyNIT9sg
- Apple. n.d. "PHASE." Apple Developer. https://developer.apple.com/documentation/phase
- Apple. 2021. "Discover spatial audio." WWDC 2021 session 10265. https://developer.apple.com/videos/play/wwdc2021/10265/
- Dolby Laboratories. n.d. "Dolby Atmos for Games." https://professional.dolby.com/gaming/
- Meta. n.d. "Meta XR Audio SDK Overview." Meta Horizon developer docs. https://developers.meta.com/horizon/documentation/unity/meta-xr-audio-sdk-overview
- Valve Corporation. n.d. "Steam Audio." GitHub repository. https://github.com/ValveSoftware/steam-audio
- Valve Corporation. n.d. "Steam Audio C API Reference." https://valvesoftware.github.io/steam-audio/doc/capi/
- Amazon Game Studios. 2022. "The Sound of New World." GDC 2022. https://www.gdcvault.com/play/1027956/
- NVIDIA. n.d. "VRWorks Audio (archived)." https://web.archive.org/web/2022/https://developer.nvidia.com/vrworks/vrworks-audio
- Algazi, V. Ralph, Richard O. Duda, Dennis M. Thompson, and Carlos Avendano. 2001. "The CIPIC HRTF Database." IEEE WASPAA 2001. UC Davis CIPIC. https://www.ece.ucdavis.edu/cipic/spatial-sound/hrtf-data/
- Algazi, V. Ralph, and Richard O. Duda. 2011. "Headphone-Based Spatial Sound." IEEE Signal Processing Magazine 28(1): 33-42.
- MIT Media Lab. n.d. "KEMAR HRTF Measurements." https://sound.media.mit.edu/resources/KEMAR.html
- Austrian Acoustics Research Institute (ARI). n.d. "ARI HRTF Database." https://www.oeaw.ac.at/isf/das-institut/software/hrtf-database
- University of York. n.d. "SADIE II Database." https://www.york.ac.uk/sadie-project/database.html
- AES. 2015. "AES69-2015: AES standard for file exchange — Spatial acoustic data file format (SOFA)." Audio Engineering Society.
- Cook, Perry R. 2002. *Real Sound Synthesis for Interactive Applications*. A K Peters.
- van den Doel, Kees, Paul G. Kry, and Dinesh K. Pai. 2001. "FoleyAutomatic: Physically-Based Sound Effects for Interactive Simulation and Animation." SIGGRAPH 2001. http://www.cs.ubc.ca/~kvdoel/publications/
- Weir, Paul. 2016. "The Sound of No Man's Sky." GDC 2016. https://www.gdcvault.com/play/1023215/
- Whitmore, Guy. 2012. "Interactive Music Techniques for Peggle 2 and Other Titles." GDC 2012. https://www.gdcvault.com/play/1015562/
- Allen, Becky. 2019. "The Audio Art of Control." GDC 2019. https://www.gdcvault.com/play/1025812/
- Edwards, Pif, Chris Landreth, Eugene Fiume, and Karan Singh. 2016. "JALI: An Animator-Centric Viseme Model for Expressive Lip Synchronization." ACM TOG 35(4) (SIGGRAPH 2016). https://www.dgp.toronto.edu/~elf/jali.html
- Wolf, Daniel S. n.d. "Rhubarb Lip Sync." GitHub repository. https://github.com/DanielSWolf/rhubarb-lip-sync
- Schissler, Carl, Ravish Mehra, and Dinesh Manocha. n.d. "GSound — Geometric Acoustics." UNC Gamma. https://gamma.cs.unc.edu/GSOUND/
- Fraunhofer IIS. 2017. "MP3 Licensing Program Terminated." Press release, April 2017.
- Xiph.Org Foundation. n.d. "Vorbis I specification." https://xiph.org/vorbis/doc/Vorbis_I_spec.html
- Valin, Jean-Marc, Koen Vos, and Tim Terriberry. 2012. "RFC 6716: Definition of the Opus Audio Codec." IETF. https://datatracker.ietf.org/doc/html/rfc6716

---

## 12. One-paragraph final recommendation (my vote)

Build ALZE v1 audio on **miniaudio** (MIT-0, single-header, platform coverage already sorted) with a **hand-rolled C++17 DSP graph of ~500 LOC** (buses, biquad filters, a Freeverb-style reverb, distance attenuation, simple HRTF-less 3D panning) wired to an **event layer of ~500 LOC** that game code talks to via string IDs. Total **~2500-3500 LOC** and a working, shippable, royalty-free audio subsystem in weeks. When and only when 3D positional audio becomes gameplay-critical, drop in **OpenAL Soft** for HRTF convolution loaded from a **MIT KEMAR SOFA file** — keep miniaudio as the device I/O and decoder layer underneath, use OpenAL Soft only as the spatializer. When and only when ray-traced audio becomes a differentiator (probably never for a single-dev scope, but honestly possible for a VR spin-off), integrate **Steam Audio** via its standalone C API. Do not reach for Wwise or FMOD at v1 — both middlewares are over-powered for a solo-dev codebase with no dedicated sound designer, both introduce build-system / licensing / vendor-update friction, and neither gives you anything you can't replicate in the authored content scope of an indie-sized game. Keep the interfaces clean so that if ALZE ever grows into Wwise territory the swap is a week's work, not a month's.
