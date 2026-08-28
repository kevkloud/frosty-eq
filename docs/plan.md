# Building a 1073/1084-style EQ Plugin (VST3 + AU) for Ableton

## Context

You want a VST3 plugin for Ableton Live that replicates the Neve 1073 and 1084 EQs — not just the frequency curves, but the *color*: the transformer saturation, the Class-A harmonic signature, and the band interaction that makes these units sound the way they do. You have 10 years of software engineering experience but have never built an audio plugin, so this plan doubles as an onboarding document for the domain.

**Decisions locked in from our conversation:**

| Decision | Choice |
|---|---|
| Packaging | One plugin, `1073 / 1084` mode switch |
| Control feel | Frequency selectors stepped; gain knobs continuous |
| Features | Core hardware control set + Mix (dry/wet) + auto-gain |
| Metering | EQ curve display + input/output level meters |
| GUI stack | JUCE native C++ |
| Distribution | Free / open source |
| Fidelity strategy | Staged — accurate linear EQ first, then color |

**Two constraints that follow from "open source":**

1. We model from **schematics, published specifications, and circuit theory** — not by curve-fitting against your UAD plugins' output. Using UAD as an *ear reference* ("am I in the ballpark?") is completely normal and fine. Capturing its output and numerically optimizing our coefficients to match it is not, and would poison an open-source release.
2. The product cannot be named "Neve," "1073," or "1084." Those are live AMS Neve trademarks. The *circuits* are from 1970–71 and long out of patent — replicating them is legal. Only the branding is off limits. Working title in this doc: **`ClassicEQ`** (rename later).

---

## Part 1 — How VST Plugin Development Actually Works

Skip to Part 2 if this is familiar. This section defines the terminology used throughout.

### The basic model

A plugin is **not** an application. It is a **dynamically-loaded library** that a host application (Ableton Live) loads into its own process and calls. A `.vst3` on macOS is not a file — it is a *bundle*, a directory with a specific structure, that macOS displays as a single item. Same for `.component` (Audio Units).

The host owns the audio device, the clock, and the threads. Your plugin is a guest. The host calls you; you never call the host except through narrowly-defined callbacks.

### The core loop

The host hands you audio in **blocks** (also called buffers). At 48 kHz with a 128-sample block size, the host calls your `processBlock()` function 375 times per second, each time handing you 128 samples per channel to modify **in place**. You have roughly 2.6 ms of wall-clock time to return. If you don't, the audio device underruns and the user hears a click. This is called a **dropout** or **xrun**.

```
Host audio thread (real-time, high priority):
  loop forever:
    fill input buffer from audio interface
    → plugin.processBlock(buffer, midiMessages)   ← YOUR CODE, ~2.6ms budget
    write output buffer to audio interface
```

### The real-time thread contract — the #1 conceptual jump

This is where experienced software engineers get burned. Inside `processBlock()` you **must not**:

- Allocate or free memory (`new`, `delete`, `malloc`, `std::vector::push_back`, `std::string`, any STL container that grows)
- Take a lock that another thread might hold (`std::mutex`) — priority inversion causes dropouts
- Do file or network I/O, or log
- Throw exceptions
- Call anything with unbounded or unpredictable runtime
- Call into the OS in ways that might block (most system calls)

The reason: the audio thread has a hard deadline. A `malloc` that usually takes 100 ns can take 10 ms when the allocator decides to hit the kernel. You will not see this in testing; the user will hear it in a mix session.

**The pattern:** allocate everything up front in `prepareToPlay()` (called on the message thread before audio starts, when you learn the sample rate and max block size), and communicate between threads with `std::atomic` for scalars and lock-free FIFOs for anything larger.

`prepareToPlay(sampleRate, maxBlockSize)` is your constructor-for-audio. It gets called again whenever the user changes sample rate or buffer size in Live's preferences. Everything sample-rate-dependent — filter coefficients, delay line sizes, oversampler state — is built or resized here.

### Parameters and automation

Anything the user can turn, and anything Ableton can automate, is a **parameter**. Parameters have rules:

- They are exposed to the host as a **normalized float in [0, 1]**. Your "±18 dB mid gain" is stored as 0.5 = 0 dB. JUCE's parameter classes handle this conversion for you.
- Each has a **stable string ID**. This is the one thing in the whole project that is genuinely hard to change later: once a user saves a Live set with your plugin, the saved automation and state are keyed by these IDs. Rename `mid_gain` to `midGain` and every existing project silently loses that setting. **Design the parameter layout carefully in Phase 1 and then treat the IDs as an append-only schema**, exactly like a database migration or a wire protocol.
- Changes arrive from *both* threads: the GUI (message thread) when the user drags a knob, and the host (audio thread) when automation plays back. Reading them safely is the job of JUCE's `AudioProcessorValueTreeState` (APVTS), which is the standard solution — it gives you an atomic-backed value per parameter, plus automatic GUI attachment and state serialization.

### State

Ableton saves your plugin's full state inside the `.als` project file by calling `getStateInformation()` (you serialize to a byte blob) and restores it with `setStateInformation()`. APVTS serializes to XML for you. **Always write a version number into your state blob** so future versions can migrate old projects.

### The GUI

The editor is a **separate object with a separate lifetime** from the processor. The user can close the plugin window — your editor is destroyed — while audio keeps running. Never store DSP state in the editor, and never let the editor hold a pointer the processor assumes is alive.

The editor runs on the **message thread** (the UI thread). To display meters, the audio thread writes a level into a `std::atomic<float>` and the editor polls it from a `juce::Timer` at ~30 Hz. You do not push from audio to UI; you publish and let the UI sample.

### Latency and PDC

If your plugin delays the signal — which oversampling with linear-phase filters does — you must report it via `setLatencySamples()`. Ableton reads this and applies **plugin delay compensation (PDC)**, shifting other tracks so everything stays in sync. Report it wrong and your track drifts against the rest of the mix. Report it *late* (after audio starts) and Live may not pick it up cleanly.

### Formats

| Format | What it is | Relevant here? |
|---|---|---|
| **VST3** | Steinberg's cross-platform standard | **Yes** — primary target |
| **AU** (Audio Unit) | Apple's macOS/iOS format | **Yes** — Live and Logic both load it; nearly free from JUCE |
| **AAX** | Avid, Pro Tools only | No — requires an NDA'd SDK and paid signing |
| **CLAP** | Modern permissive open format | Not yet — Live doesn't support it natively |
| **Standalone** | Your plugin as its own .app | **Yes** — this is your primary dev/debug target |

JUCE builds all of these from one codebase. You write the DSP once.

### The development loop (and why Ableton is bad at it)

Live scans and **caches** plugins at startup. It will hold the `.vst3` bundle open, so rebuilding while Live is running often fails to take effect or fails to link. Iterating through Live is miserable.

**The actual dev loop:**
1. Build the **Standalone** target. It's a normal macOS app — launch it, attach lldb/Xcode, set breakpoints, hot-iterate.
2. Use **JUCE's AudioPluginHost** (a sample app that ships with JUCE) when you need to test plugin-specific behavior — parameter automation, state save/restore — without Live's caching.
3. Load into **Ableton periodically** — at phase boundaries, not every build — to verify the real integration.

### Validation tooling

- **pluginval** — Tracktion's open-source plugin validator. Runs your plugin through hostile conditions (random buffer sizes, sample rate changes, parameter fuzzing, editor open/close cycles, out-of-order calls) and catches the crashes real hosts eventually find. Run it at strictness level 10 in CI.
- **auval** — Apple's Audio Unit validator, built into macOS. Your AU must pass `auval` or Logic and Live will refuse to load it.

---

## Part 2 — Stack and Dependencies

### Homebrew

Everything you need from brew (all verified present in the taps on your machine):

```bash
brew install cmake ninja pkgconf
```

```bash
brew install --cask pluginval
```

For the Python analysis/measurement side — your system Python is 3.9.6 with no scientific stack, and you already have `uv` installed, which is the cleanest way to handle this:

```bash
uv venv --python 3.12 && uv pip install numpy scipy matplotlib soundfile
```

### Not from Homebrew (and that's correct)

**JUCE** is not a brew formula — the `juce` hits in brew search are unrelated packages. This is fine and actually preferable: JUCE is consumed as **source, vendored as a git submodule**, and pulled into your build with CMake's `add_subdirectory()`. This pins an exact version, makes CI reproducible, and lets you patch it if you ever need to.

```bash
git submodule add https://github.com/juce-framework/JUCE.git libs/JUCE
```

Pin to a release tag rather than tracking `master`. Check what the current stable major is when you set up — JUCE 8 is current as of this writing but JUCE 9 references have appeared in their legal docs, so verify at setup time.

**The VST3 SDK ships inside JUCE.** You do not download or manage it separately.

### What you already have

- Xcode 26.5 with the full toolchain — this is what CMake will drive
- Apple clang 21, arm64 native
- git, gh
- Ableton Live 11 and 12 Suite, Logic Pro — three hosts to test against
- **UAD Neve 1073 and 1084** — a professional reference to A/B by ear

### Licensing — an important recent change

**Steinberg relicensed the VST3 SDK to MIT in late 2025.** It used to be GPLv3-or-commercial, which historically forced open-source plugin projects into GPL. That constraint is gone. The SDK now imposes no meaningful restriction on you.

**JUCE is dual-licensed: AGPLv3, or a commercial license.** The commercial "Starter" tier is free below $20k annual revenue, but its EULA (§2.3) explicitly prohibits combining JUCE with strong-copyleft open-source code — so Starter is not a path to a GPL'd GitHub repo.

Your options, ranked:

1. **Release under AGPLv3.** Free, unambiguous, no paperwork. The AGPL's network-service clause is irrelevant to an audio plugin, so in practice this behaves like GPLv3. **Recommended** — this is what most open-source JUCE plugins do.
2. **JUCE Starter (free) + closed-source free binary.** You give the plugin away but don't publish source. Legal and free, but it isn't what you said you wanted.
3. **Switch to a permissive framework** — iPlug2 or DPF — if you specifically need MIT/BSD. This costs you JUCE's GUI toolkit, its DSP module, and roughly 90% of the tutorial material you'll want as a beginner. Only worth it if permissive licensing is a hard requirement.

Distributing to *other people's* Macs additionally requires **code signing with a Developer ID and notarization** (Apple Developer Program, $99/yr), or your users hit Gatekeeper. For your own machine, an ad-hoc signature is enough and costs nothing.

---

## Part 3 — What We Are Actually Modeling

Understanding the circuit is what separates this from "three biquads and a tanh."

### Signal chain of a real 1073

```
Mic/Line in
  → Input transformer (Marinair LO1166)      ← color: LF hysteresis, HF rolloff, phase
  → Class-A discrete preamp (BA283 card)     ← color: asymmetric clipping, 2nd harmonic
  → EQ section: passive LC network in the    ← the EQ, and the band interaction
    feedback loop of another Class-A stage
  → Output fader
  → Class-A output amp (BA283)               ← color: more asymmetric saturation
  → Output transformer (LO1166)              ← color: the dominant LF saturation source
```

Two things matter enormously and are what cheap emulations get wrong.

### 1. The bands are not independent — they share a feedback network

The Neve EQ is **not** three biquads in series. It is a single **passive LC network sitting in the negative feedback path of a gain stage**. All three bands live in that one network.

The mathematical consequence, and the single most important insight in this project: **the band branches sum in *admittance*, not in dB.**

```
Y_total(s) = Y_low(s) + Y_mid(s) + Y_high(s) + Y_fixed
H(s)       = f(Y_total(s))
```

Boost the mid *and* the high shelf together and you do **not** get the sum of the two curves — you get the response of the combined network, which is measurably less than the sum where they overlap. Cascaded independent biquads cannot reproduce this. It is a large part of why a 1073 "never sounds harsh" when you boost several bands at once, and why it survives moves that would wreck a clean digital EQ.

Two more consequences of the topology:

- **Proportional Q on the mid band.** The gain pot changes the loading on the LC tank, so Q varies with boost/cut amount — broad near flat, narrower toward the extremes. This is a *free* result of modeling the network correctly, and an ugly special case if you don't.
- **Inductor-shaped shelves.** The low shelf has a small characteristic dip just above the shelf frequency when boosting. The high shelf isn't a textbook shelf either — it rises and then gently rolls off at the very top rather than staying flat to Nyquist. That top-end rolloff is exactly why 12 kHz boosts sound like "air" and not "hiss."

### 2. The color is level-dependent and frequency-dependent

The distinctive part of transformer saturation is that **it is much stronger at low frequencies.** Core flux is the integral of voltage over time, so for the same voltage a 50 Hz signal drives far more flux than a 5 kHz one. A real 1073's THD is meaningfully higher at 30 Hz than at 3 kHz at the same level. A plain memoryless waveshaper distorts all frequencies equally and will never sound right — this is the tell of an amateur emulation.

The harmonic signature is dominantly **2nd harmonic** (from asymmetric Class-A stages) with **3rd** appearing from the transformers, especially at LF and at higher levels.

### Control specifications

| Band | 1073 | 1084 |
|---|---|---|
| **High shelf** | 12 kHz fixed, ±16 dB | 10k / 12k / 16k, ±16 dB |
| **Mid bell** | 360 / 700 / 1.6k / 3.2k / 4.8k / 7.2k Hz, ±18 dB | same 6 freqs, ±18 dB, **+ Hi-Q switch** |
| **Low shelf** | 35 / 60 / 110 / 220 Hz, ±16 dB | same |
| **High-pass** | 50 / 80 / 160 / 300 Hz, 18 dB/oct | 45 / 70 / 160 / 360 Hz, 18 dB/oct |
| **Low-pass** | — | 6k / 8k / 10k / 14k / 18k Hz |

Treat these published values as the *nominal* targets. The actual curve shapes — Q, shelf slope, the HPF's resonant bump — come from the network model, not from a spec sheet.

---

## Part 4 — Architecture

```
classic-eq/
├── CMakeLists.txt
├── libs/JUCE/                        # git submodule, pinned to a release tag
├── src/
│   ├── PluginProcessor.{h,cpp}       # juce::AudioProcessor — thin host adapter
│   ├── PluginEditor.{h,cpp}          # juce::AudioProcessorEditor
│   ├── params/ParameterLayout.{h,cpp}# APVTS layout — the permanent ID schema
│   ├── dsp/
│   │   ├── DspCore.{h,cpp}           # ★ host-agnostic. No JUCE plugin types.
│   │   ├── EqNetwork.{h,cpp}         # admittance-summed LC network → state-space
│   │   ├── Branches.h                # low shelf / mid tank / high shelf branches
│   │   ├── FilterSections.h          # 3rd-order HPF, LPF
│   │   ├── Saturator.h               # ADAA asymmetric waveshaper
│   │   ├── TransformerStage.h        # Wiener–Hammerstein LF-weighted saturation
│   │   └── ModelTables.h             # 1073 vs 1084 component values + freq tables
│   └── gui/
│       ├── KnobLookAndFeel.{h,cpp}
│       ├── SteppedSelector.{h,cpp}
│       ├── EqCurveDisplay.{h,cpp}
│       └── LevelMeter.{h,cpp}
├── tools/
│   ├── measure/                      # C++ CLI: sweep, THD, golden-file rendering
│   └── python/                       # scipy: derive network values, generate targets
├── tests/
│   ├── targets/*.csv                 # expected magnitude/phase curves
│   └── golden/*.wav                  # regression renders
└── .github/workflows/build.yml
```

**The single most important architectural decision: `DspCore` must not depend on JUCE's plugin layer.** It takes a plain struct of parameters and a raw float buffer. This means:

- The CLI measurement harness drives it directly — no host, no plugin wrapper, no GUI. Sweeps run in milliseconds.
- Unit tests are ordinary tests.
- The EQ curve display evaluates the *same* transfer function the audio path uses, so the drawn curve is guaranteed to match what you hear. (Emulations where the display is a separate approximation are a common and maddening bug class.)

This is the standard "hexagonal architecture" instinct applied to audio, and it's the thing that will make this project feel tractable to you rather than alien.

---

## Part 5 — Phased Implementation

### Phase 0 — Environment and hello-world (½ day)

1. `brew install cmake ninja pkgconf`; `brew install --cask pluginval`
2. `git init`, add JUCE as a submodule pinned to a release tag
3. CMake project using `juce_add_plugin()`; formats `VST3 AU Standalone`; set `COPY_PLUGIN_AFTER_BUILD TRUE` so builds land in `~/Library/Audio/Plug-Ins/` automatically
4. Build a trivial gain plugin. Target `arm64` only for dev (universal binary is a release-time concern).
5. Build and run JUCE's `AudioPluginHost` sample once — this is your dev harness.

**Exit criteria:** the plugin runs as a Standalone app, loads in AudioPluginHost, and appears in Ableton Live 12's browser and passes audio.

### Phase 1 — Parameter schema and skeleton (1–2 days)

Define the complete APVTS layout now, because the IDs are permanent:

| ID | Type | Range / choices |
|---|---|---|
| `model` | choice | 1073, 1084 |
| `hf_freq` | choice | model-dependent |
| `hf_gain` | float | ±16 dB |
| `mid_freq` | choice | 360/700/1.6k/3.2k/4.8k/7.2k |
| `mid_gain` | float | ±18 dB |
| `mid_hiq` | bool | 1084 only |
| `lf_freq` | choice | 35/60/110/220 |
| `lf_gain` | float | ±16 dB |
| `hpf_freq` | choice | Off + 4 model-dependent |
| `lpf_freq` | choice | Off + 5 (1084 only) |
| `input_gain` | float | dB, drives the saturation |
| `output_level` | float | dB |
| `eq_in` | bool | EQ bypass (color stays in) |
| `phase` | bool | polarity invert |
| `mix` | float | 0–100% |
| `auto_gain` | bool | |
| `oversampling` | choice | Off / 2× / 4× / HQ |

Ship a generic auto-generated editor for now.

**Exit criteria:** every parameter automates in Ableton, and a Live set that saves with non-default settings restores them correctly after quitting and reopening Live.

### Phase 2 — The linear EQ (1–2 weeks; this is the hard part)

**Python side first.** Build the admittance model of the EQ network from the 1073 schematic in scipy. Solve for the component values that reproduce the published curves. Export target magnitude and phase responses as CSV across a grid of settings — single bands, and critically, *combinations* of bands.

**C++ side.** Implement `EqNetwork` as a **topology-preserving state-space realization**: build each LC branch from TPT (topology-preserving transform) integrators and solve the resulting small linear system. With ~8–10 reactive elements this is a fixed-size matrix solve whose inverse is precomputed whenever coefficients change. This gives exact band interaction and proportional Q for free, stays stable under parameter modulation, and has bounded per-sample cost.

Implement HPF and LPF as separate 3rd-order sections (one real pole + one complex pair, Q tuned to reproduce the passive LC filter's characteristic resonant bump near cutoff).

Compute coefficients **off the audio thread** and hand them over via a lock-free swap; crossfade over ~10 ms when a stepped selector changes.

**Exit criteria:** the measurement harness verifies every band and every tested band *combination* against the Python-derived targets within ±0.25 dB from 20 Hz to 20 kHz. You now have an accurate, clean, fully usable EQ.

### Phase 3 — GUI v1 (3–5 days)

- Custom `LookAndFeel`: rotary knobs, stepped selectors that render visible detents
- **EQ curve display** — evaluate `EqNetwork`'s transfer function at ~256 log-spaced points on a 30 Hz timer and stroke the path. Reuses the audio-path math, so it cannot drift.
- **I/O level meters** — audio thread writes peak and RMS to `std::atomic<float>`; editor polls on the timer
- Panel layout roughly mirroring the hardware, so muscle memory transfers

### Phase 4 — Saturation and color (1–2 weeks)

- **`Saturator`**: asymmetric static waveshaper for the Class-A stages, dominantly 2nd harmonic. Use **ADAA** (antiderivative anti-aliasing) — for `tanh` the antiderivative is `log(cosh(x))` in closed form, so it's cheap, and it kills most aliasing without brute-force oversampling.
- **`TransformerStage`**: a **Wiener–Hammerstein sandwich** — LF-emphasis filter → memoryless saturator → complementary de-emphasis — in series with the transformer's linear response (LF high-pass ~10–20 Hz, HF low-pass ~30–50 kHz). This is what produces frequency-dependent saturation: heavy at 50 Hz, negligible at 5 kHz, which is the actual signature of the sound.
- Place these at the correct points in the chain (Part 3), not as a single lump at the end. Placement relative to the EQ changes the result audibly.
- **Oversampling**: `juce::dsp::Oversampling`. Use IIR polyphase (minimum phase, near-zero latency) for live use and FIR (linear phase) for HQ/render mode. Report latency via `setLatencySamples()`.
- **Auto-gain**: compute the network's gain at the current settings and compensate, so A/B comparisons aren't just "louder is better."

**Exit criteria:** the THD harness shows THD rising with level *and* falling with frequency, with a dominant 2nd harmonic — matching the published behavior of the hardware. Then A/B by ear against your UAD 1073.

### Phase 5 — 1084 mode (2–4 days)

Extra shelf frequencies, Hi-Q mid switch, low-pass filter, different HPF frequency table.

One design decision to make here: when the user switches models and the current frequency doesn't exist in the other model's table (e.g. 12 kHz shelf → 1084's 10k/12k/16k), map to the nearest available value and make sure that mapping is deterministic and round-trip-safe.

### Phase 6 — Hardening and release

- `pluginval --strictness-level 10` and `auval` in CI (GitHub Actions, macOS runner)
- `juce::ScopedNoDenormals` in `processBlock` (denormal floats cause 100× CPU spikes in IIR filter tails — a classic audio bug)
- Thread-safety audit; CPU profile with Instruments; target < 2% of one core per instance at 48 kHz
- Universal binary (`arm64;x86_64`), Developer ID signing, notarization
- AGPLv3 license file, README, trademark-safe product name

---

## Part 6 — Difficulties, With Solutions Ranked by Confidence

### 1. Band interaction (the defining problem)

Cascaded independent biquads produce visibly and audibly wrong curves whenever two bands are engaged at once.

1. **Admittance-summed network → TPT state-space realization.** Exact interaction, modulation-safe, bounded per-sample cost, gives proportional Q for free. *This is the plan.*
2. Derive `H(s)` analytically, bilinear-transform with prewarping, factor into cascaded biquads off the audio thread. Simpler to start; needs care with numerical root-finding.
3. Full wave-digital-filter model of the schematic. Most authentic, substantially more work, needs per-sample iterative solving.
4. Cascaded biquads plus an empirical correction table. Fast to build, fundamentally wrong, won't survive an A/B.

### 2. Frequency-dependent transformer saturation

A memoryless waveshaper distorts all frequencies equally. Real transformers distort LF far more than HF. This is the difference between "saturated" and "sounds like a 1073."

1. **Wiener–Hammerstein sandwich** (emphasis → saturator → de-emphasis). Cheap, stable, well-understood, and captures the essential behavior. *Recommended.*
2. **Jiles–Atherton hysteresis** (the model behind CHOW Tape). Physically real magnetic hysteresis with memory. Needs Newton–Raphson per sample, is numerically stiff, and can go unstable. Worth doing as a Phase 4.5 upgrade *only if* the sandwich doesn't convince you.
3. Neural / black-box modeling. Ruled out — it would require either real hardware to train on or fitting to UAD's output, which we've excluded.

### 3. Aliasing from any nonlinearity

Every waveshaper generates harmonics above Nyquist that fold back as inharmonic garbage — the "digital fizz" that makes bad saturation plugins sound bad.

1. **ADAA + 2× oversampling.** ADAA is elegant and cheap for memoryless shapers; 2× catches the rest. Best quality-per-CPU.
2. 4×–8× oversampling alone. Simple, brute force, more CPU and more latency.
3. Higher-order ADAA. Better still, more math, diminishing returns.

### 4. Clicks when changing stepped selectors

Switching frequency swaps filter coefficients discontinuously → audible click.

1. **Crossfade** between two filter instances over ~10 ms. Robust, always works, costs 2× filter CPU during the transition only. *Recommended.*
2. Interpolate coefficients. Cheaper, but interpolated IIR coefficients can transiently go unstable.
3. Recompute at buffer boundaries with a short output ramp. Simplest; still faintly audible.

### 5. Real-time thread violations

Allocation or locking in `processBlock` causes dropouts that appear only under load and are hard to reproduce.

1. Preallocate in `prepareToPlay`; `std::atomic` for scalars; lock-free FIFO for anything larger; `ScopedNoDenormals`. *Standard practice.*
2. Add a real-time-safety assertion layer in debug builds (a custom allocator that fires an assert if called from the audio thread). Very worth the hour it takes.
3. Run pluginval at strictness 10 in CI to catch what review misses.

### 6. Validating "does it sound like a 1073?"

The failure mode here is subjective drift — endless tweaking with no ground truth.

1. **Build the measurement harness in Phase 2, before you need it.** Sweep → deconvolve → magnitude/phase vs. CSV targets; sine grid → FFT → per-harmonic levels and THD vs. targets; golden-file renders for regression. Under `ctest`. This turns a taste problem into a test suite, which is the whole reason to approach this as an engineer.
2. Blind A/B against the UAD in Live with `mix` at 100% and auto-gain on. Ear reference only — never a fitting target.
3. Published measurements of real hardware from Sound On Sound and similar, as an independent cross-check.

### 7. Ableton's plugin caching makes iteration painful

1. **Iterate on the Standalone target and AudioPluginHost; open Live only at phase boundaries.** *This is the answer.*
2. Live → Preferences → Plug-Ins → Rescan when you do need it.
3. Quit Live entirely before rebuilding, or the bundle stays open and the copy step fails.

### 8. Parameter IDs are effectively a permanent schema

Renaming or reordering silently breaks users' saved Live sets.

1. **Design the full layout in Phase 1 and treat IDs as append-only**, with a version tag in the state blob and explicit migration code. Same discipline as a wire protocol.
2. Add a state-round-trip test to the harness that loads an old serialized blob and asserts the resulting parameter values.

### 9. Latency reporting and PDC

Oversampling adds latency; misreport it and the track drifts against the mix.

1. Report via `setLatencySamples()` in `prepareToPlay`, and call `updateHostDisplay()` if the oversampling mode changes at runtime.
2. Verify empirically: null-test a rendered track against a silent-plugin reference in Live. If PDC is right, it nulls.

### 10. Two vintage designs, ambiguous specs

Published 1073/1084 specs vary between sources, and real units drift with component tolerance and age.

1. Derive curves from the **schematic and circuit theory**, using published specs as sanity checks rather than as the source of truth.
2. Where sources genuinely conflict (the 1084's HPF frequencies are quoted differently in different places), pick the AMS Neve official value and note the choice in a comment.

### 11. Scope creep into a full channel strip

The 1073 is a preamp *and* an EQ. It's tempting to model the mic preamp, add noise, add a compressor.

1. Hold the line at the decided feature set. `input_gain` is your drive control; it gets you the preamp color without a separate preamp model.
2. Park everything else (noise floor, M/S, per-channel component drift, separate drive knob) in a `FUTURE.md`.

### 12. JUCE's default GUI looks like 2005

1. Custom `LookAndFeel` with vector drawing — resolution-independent, small binary, easy to tweak. *Recommended for v1.*
2. Filmstrip PNG knobs — the industry-standard look, but you need the artwork.
3. JUCE 8 WebView UI (React) — you'd be more productive in it given your background, but it adds a param-sync bridge to debug and thinner docs. Revisit at v2 if the native GUI frustrates you.

---

## Part 7 — Verification

**Per-phase gates** are listed as "exit criteria" in Part 5. The persistent verification infrastructure:

**`tools/measure` CLI** — links `DspCore` directly, no host:
- `measure sweep --settings <json>` → log sine sweep, deconvolve to impulse response, emit magnitude + phase CSV
- `measure thd --freq <f> --level <dBFS>` → FFT, per-harmonic levels, THD
- `measure render --in <wav> --out <wav>` → deterministic offline render for golden files

**`ctest` suite:**
- Curve tests: every band and a matrix of band *combinations* vs. `tests/targets/*.csv`, tolerance ±0.25 dB
- THD tests: THD must rise with level and fall with frequency, H2 > H3
- Golden-file regression: fixed input renders bit-comparable within tolerance
- State round-trip: serialize → deserialize → assert parameter equality
- Real-time safety: debug allocator asserts nothing allocates on the audio thread

**Host-level:**
- `pluginval --strictness-level 10` on both VST3 and AU
- `auval -v aufx <subtype> <manufacturer>` must pass
- Manual Live checks: parameter automation records and plays back; project save/reload preserves state; PDC nulls; no clicks when sweeping stepped selectors during playback
- Blind A/B against UAD 1073 and 1084, level-matched via auto-gain

---

## Part 8 — Legal Summary

| Item | Status |
|---|---|
| 1073/1084 circuit topology | Patents long expired — replicating is legal |
| "Neve", "1073", "1084" names | **Active AMS Neve trademarks — cannot use in product name or marketing** |
| VST3 SDK | MIT since late 2025 — no restriction |
| JUCE | AGPLv3 (recommended) or free Starter commercial license below $20k revenue |
| Measuring your UAD plugins | Fine as an ear reference; **do not curve-fit coefficients to its output** |
| Distributing to other Macs | Needs Apple Developer Program ($99/yr) for signing + notarization |

---

## First Concrete Steps

1. `brew install cmake ninja pkgconf` and `brew install --cask pluginval`
2. Create the repo, add JUCE as a pinned submodule, write the root `CMakeLists.txt`
3. Get a trivial gain plugin building as Standalone + VST3 + AU
4. Confirm it loads in Ableton Live 12 and passes audio
5. Then Phase 1: nail down the parameter schema

The riskiest and most interesting work is Phase 2 (the admittance network). Everything before it is scaffolding, and everything after it is layering. If Phase 2 is right, this plugin will be genuinely good.

## Sources

- [JUCE 8 End User Licence Agreement](https://juce.com/legal/juce-8-licence/)
- [JUCE LICENSE.md (dual AGPLv3 / commercial)](https://github.com/juce-framework/JUCE/blob/master/LICENSE.md)
- [Steinberg moves VST3 SDK to MIT license](https://www.kvraudio.com/news/steinberg-moves-vst-3-sdk-to-mit-open-source-license-asio-now-gplv3-65179)
- [VST 3 Developer Portal — Licensing](https://steinbergmedia.github.io/vst3_dev_portal/pages/FAQ/Licensing.html)
- [AMS Neve 1073 Mic Preamp & Equaliser](https://www.ams-neve.com/outboard/1073-range/1073-mic-preamp-equaliser/)
- [AMS Neve 1084](https://www.ams-neve.com/outboard/classic-range/1084-2/)
- [UA Neve 1073 Preamp & EQ Manual](https://help.uaudio.com/hc/en-us/articles/4419497223188-Neve-1073-Preamp-EQ-Manual)
