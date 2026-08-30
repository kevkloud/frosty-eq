# FrostyEQ

A VST3 / AU equaliser modelled on the classic 1970–71 British console EQ modules
(the 1073 and 1084 topologies), for use in Ableton Live, Logic, and other hosts.

Not affiliated with, endorsed by, or derived from any product of AMS Neve Ltd.
The circuits modelled here are over fifty years old and long out of patent; the
names and trademarks belong to their owners and are not used in this product.

## Status

Phase 4 complete. Builds as VST3, AU, and Standalone; passes `auval`,
`pluginval --strictness-level 10`, and its own test suite.

The equaliser works. All three bands, the high-pass and the 1084 low-pass are
implemented as a shared feedback network rather than a chain of biquads, so the
bands interact and the mid bell shows proportional Q the way the hardware does.
There is a real panel, with a live response curve and input/output metering.

The colour stage is in: two transformers and two single-ended class-A gain
stages around the equaliser, anti-aliased and oversampled. Drive it with the
Input control and pull the Output down, exactly as you would wind up the mic
gain and drop the fader on the hardware.

## Measured

At full scale with the Input at unity, 2x oversampling:

| | THD | character |
|---|---|---|
| 40 Hz | 4.3 % | third harmonic leads by 8.7 dB -- the core saturates symmetrically |
| 1 kHz | 0.64 % | second harmonic leads by 12.1 dB -- the class-A stages |
| 10 kHz | 0.52 % | second harmonic |

Low frequencies distort roughly seven times harder than the midrange at the
same level, because core flux is the integral of applied voltage and so falls
as 1/f. Marinair measured 10:1 between 40 Hz and 1 kHz on the line transformer
used in these units. Distortion falls cleanly with level -- a quarter for every
12 dB down, which is what a second-harmonic-dominant stage should do -- and
folded images sit at -141 dB at the default 2x.

```bash
./build/measure profile
```

## Curve calibration

Every branch constant is either fitted to a measurement or chosen for a stated
reason. They began as estimates and the difference mattered -- the mid band was
up to 15.8 % off its marked frequency, and the high-pass carried a 0.87 dB
resonance the hardware does not have.

Targets are traced from response plots of an assembled board published with the
[Nyan-1073-EQ](https://github.com/ravettel/Nyan-1073-EQ) hardware project
(CC BY-SA 4.0), read by calibrating against the plot axes and following each
curve by colour rather than by eye.

**Mid band.** Q is set per switch position. The circuit does not switch the
band uniformly: the lower three positions switch inductance as well as
capacitance, holding Q roughly level, while the upper three share one winding
and switch capacitance alone, so Q climbs with frequency. Realised -3 dB widths
at +18 dB land on the measured figures exactly -- 1.13, 1.00, 1.06, 1.15, 0.74
and 0.52 octaves -- so 360 Hz is broad and musical and 7.2 kHz is a focused
presence peak. Every detent peaks within 0.6 % of its marked frequency, tighter
than the board that was measured, whose own peaks sit up to 6.6 % off.

**Shelves.** Half first order, half second. Purely first order is too shallow
at 4.9 dB/octave against a measured 7.0; purely second order carves a 6.5 dB
hole an octave from a 12 dB boost. The blend gives 6.4 to 6.9 and brings the
-3 dB corners inside 6 % of their marked frequencies.

**High-pass.** Third-order Butterworth: one real pole and a pair at Q = 1.0.
Maximally flat, which is what the hardware measures -- no peak whatever.
Asymptotic slope 18.00 dB/octave against the quoted 18.

That board is one reference, not ground truth: its shelves measure +18 to
+21 dB where the unit is specified at +/-16. Where its figures conflict with
the published specification the specification wins, and each such point is
noted in `ModelTables.h`.

The tracing is in the repository too, so the targets can be checked rather than
taken on trust:

```bash
uv venv && uv pip install pillow numpy
.venv/bin/python tools/python/trace_measurements.py mid nyan1073_mid.png
```

```bash
./build/measure bell     # realised centre and width, per mid detent
./build/measure fitq     # solve branch Q from a measured width
./build/measure shelf    # shelf slope, corner and dip
./build/measure hpf      # high-pass corner, slope and resonance
```

## The two modules

The 1084 is the 1073 with more options, and the differences come from the
Neve 1073 & 1084 user manual (issue 5) rather than from retailer copy:

| | 1073 | 1084 |
|---|---|---|
| High shelf | 12 kHz fixed, +/-16 dB | 10k / 12k / 16k, +/-16 dB |
| Mid | +/-18 dB, fixed Q | +/-18 dB, switchable Hi-Q |
| Low shelf | 35 / 60 / 110 / 220 Hz, +/-16 dB | same |
| High pass | 18 dB/oct, 50 / 80 / 160 / 300 Hz | 18 dB/oct, 45 / 70 / 160 / 360 Hz |
| Low pass | none | 18 dB/oct, 6 / 8 / 10 / 14 / 18 kHz |

Two things worth flagging. The high-pass frequencies above are the original
module's; AMS Neve's current page specifies the 1084 reissue with the 1073's
set, so both figures are correct for different units and these follow the
original. And the manual specifies the 1084's mid as "+/-12dB or +/-18dB
peaking with switchable 'High Q'" without saying which range goes with which Q
-- that is not modelled, and why is in `ModelTables.h`.

In 1073 mode the controls the module does not have stay on the panel but grey
out, and the DSP ignores them, so automation survives a model switch.

## Cost

Stereo, 48 kHz, measured as a fraction of one core on Apple silicon:

| oversampling | % of a core |
|---|---|
| Off | 0.94 |
| 2x (default) | 4.7 |
| 4x | 9.9 |
| 8x | 19.2 |

Roughly 57 % of that is the four saturating stages, which evaluate an
antiderivative in double precision per sample; the anti-imaging filters are
about a fifth, and the equaliser the rest. Off is there for when a session is
tight -- anti-aliasing still runs, it just runs at the host rate.

## Interface

A channel strip, not an analyser. One column: gain at the top, the three bands
below it as concentric pairs with their frequencies legended around the ring,
the filters under those, and the output at the bottom.

There is no response curve, no analyser, and **no numeric readout on any cut or
boost**. A gain control is marked with a plus and a minus and nothing else,
which is what the hardware does. That is deliberate, and it came from an
engineer who has spent years on the actual units: the numbers make people mix
with their eyes, hunting a tidy figure and flinching from a large move. Take
them away and the ear decides. Frequency legends stay, because those are switch
positions rather than amounts.

The colour follows the module -- rose for the 1073, black for the 1084 -- so the
panel says which one you are on without a label having to. It is not either
unit's actual livery; copying that would be trade dress, and it invites the
plugin to be judged as a failed clone rather than used on its own terms.

The output meter switches between peak dBFS and VU on click. VU is not the same
number on a different scale: it is an RMS reading with slow ballistics, 0 VU at
-18 dBFS, which is why it reads weight where a peak meter reads headroom.

Render the panel without launching a host:

```bash
./build/snapshot ui.png 360 792 model=1 mid_gain=9 mid_freq=5 mid_hiq=1
```

## Curve calibration

Every branch constant is either fitted to a measurement or chosen for a stated
reason. They began as estimates and the difference mattered -- the mid band was
up to 15.8 % off its marked frequency, and the high-pass carried a 0.87 dB
resonance the hardware does not have.

Targets are traced from response plots of an assembled board published with the
[Nyan-1073-EQ](https://github.com/ravettel/Nyan-1073-EQ) hardware project
(CC BY-SA 4.0), read by calibrating against the plot axes and following each
curve by colour rather than by eye.

**Mid band.** Q is set per switch position. The circuit does not switch the
band uniformly: the lower three positions switch inductance as well as
capacitance, holding Q roughly level, while the upper three share one winding
and switch capacitance alone, so Q climbs with frequency. Realised -3 dB widths
at +18 dB land on the measured figures exactly -- 1.13, 1.00, 1.06, 1.15, 0.74
and 0.52 octaves -- so 360 Hz is broad and musical and 7.2 kHz is a focused
presence peak. Every detent peaks within 0.6 % of its marked frequency, tighter
than the board that was measured, whose own peaks sit up to 6.6 % off.

**Shelves.** Half first order, half second. Purely first order is too shallow
at 4.9 dB/octave against a measured 7.0; purely second order carves a 6.5 dB
hole an octave from a 12 dB boost. The blend gives 6.4 to 6.9 and brings the
-3 dB corners inside 6 % of their marked frequencies.

**High-pass.** Third-order Butterworth: one real pole and a pair at Q = 1.0.
Maximally flat, which is what the hardware measures -- no peak whatever.
Asymptotic slope 18.00 dB/octave against the quoted 18.

That board is one reference, not ground truth: its shelves measure +18 to
+21 dB where the unit is specified at +/-16. Where its figures conflict with
the published specification the specification wins, and each such point is
noted in `ModelTables.h`.

The tracing is in the repository too, so the targets can be checked rather than
taken on trust:

```bash
uv venv && uv pip install pillow numpy
.venv/bin/python tools/python/trace_measurements.py mid nyan1073_mid.png
```

```bash
./build/measure bell     # realised centre and width, per mid detent
./build/measure fitq     # solve branch Q from a measured width
./build/measure shelf    # shelf slope, corner and dip
./build/measure hpf      # high-pass corner, slope and resonance
```

## The two modules

The 1084 is the 1073 with more options, and the differences come from the
Neve 1073 & 1084 user manual (issue 5) rather than from retailer copy:

| | 1073 | 1084 |
|---|---|---|
| High shelf | 12 kHz fixed, +/-16 dB | 10k / 12k / 16k, +/-16 dB |
| Mid | +/-18 dB, fixed Q | +/-18 dB, switchable Hi-Q |
| Low shelf | 35 / 60 / 110 / 220 Hz, +/-16 dB | same |
| High pass | 18 dB/oct, 50 / 80 / 160 / 300 Hz | 18 dB/oct, 45 / 70 / 160 / 360 Hz |
| Low pass | none | 18 dB/oct, 6 / 8 / 10 / 14 / 18 kHz |

Two things worth flagging. The high-pass frequencies above are the original
module's; AMS Neve's current page specifies the 1084 reissue with the 1073's
set, so both figures are correct for different units and these follow the
original. And the manual specifies the 1084's mid as "+/-12dB or +/-18dB
peaking with switchable 'High Q'" without saying which range goes with which Q
-- that is not modelled, and why is in `ModelTables.h`.

In 1073 mode the controls the module does not have stay on the panel but grey
out, and the DSP ignores them, so automation survives a model switch.

## Cost

Stereo, 48 kHz, measured as a fraction of one core on Apple silicon:

| oversampling | % of a core |
|---|---|
| Off | 0.94 |
| 2x (default) | 4.7 |
| 4x | 9.9 |
| 8x | 19.2 |

Roughly 57 % of that is the four saturating stages, which evaluate an
antiderivative in double precision per sample; the anti-imaging filters are
about a fifth, and the equaliser the rest. Off is there for when a session is
tight -- anti-aliasing still runs, it just runs at the host rate.

## Interface

The control layout follows the hardware, so anyone who has used a 1073 knows
where things are: filters grouped together, the three bands running low to
high, each a gain control with its stepped frequency selector directly beneath
-- the flat equivalent of the hardware's concentric pairs. Hi-Q sits with the
mid band because that is where it belongs on a 1084.

The finish follows Ableton's stock devices rather than the original's panel.
That livery is trade dress; copying it would raise the same problem as using
the name.

Render the editor to a PNG without launching a host:

```bash
./build/snapshot ui.png 760 486 model=1 mid_gain=11 mid_freq=4 hpf_freq=2
```

See `docs/plan.md` for the full roadmap.

## Building

Requires macOS with Xcode, plus:

```bash
brew install cmake ninja pkgconf
brew install --cask pluginval
```

```bash
git clone --recurse-submodules <this repo>
cd classic-eq
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
```

Built plugins are copied to `~/Library/Audio/Plug-Ins/` automatically. Use
`./scripts/build.sh` instead of raw `cmake --build` -- it verifies the copy
actually landed, which it silently will not do while Ableton Live is running.

### Iterating

Live hosts plugins in-process on macOS, so once it has instantiated the plugin
it keeps that binary mapped. Rescan (`Cmd+,` -> Plug-Ins -> Rescan) refreshes
Live's database but does not swap loaded code -- picking up a code change means
quitting Live and reopening it.

So do not iterate through Live. Use the standalone build, which needs no scan,
no restart, and can be attached to a debugger:

```bash
./scripts/build.sh --run
```

Load into Live at phase boundaries, to check real host integration.

For a release universal binary, configure with
`-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`.

## Measuring

```bash
./build/measure bands
```

`measure curve` prints magnitude and phase for a given setting, `measure bands`
demonstrates band interaction, `measure q` shows proportional Q. It links the
DSP core directly, with no plugin host involved.

## Testing

```bash
cmake --build build --parallel && ctest --test-dir build --output-on-failure
```

## Validating

```bash
auval -v aufx Fsty LT3a
```

```bash
/Applications/pluginval.app/Contents/MacOS/pluginval --strictness-level 10 --validate ~/Library/Audio/Plug-Ins/VST3/FrostyEQ.vst3
```

## Licence

AGPLv3 — see `LICENSE`. JUCE is used under the AGPLv3 branch of its dual
licence. The VST3 SDK (bundled inside JUCE) is MIT-licensed.
