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
folded images sit at -111 dB at the default 2x.

```bash
./build/measure profile
```

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
