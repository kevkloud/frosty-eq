# FrostyEQ

A VST3 / AU equaliser modelled on the classic 1970–71 British console EQ modules
(the 1073 and 1084 topologies), for use in Ableton Live, Logic, and other hosts.

Not affiliated with, endorsed by, or derived from any product of AMS Neve Ltd.
The circuits modelled here are over fifty years old and long out of patent; the
names and trademarks belong to their owners and are not used in this product.

## Status

Phase 2 complete. Builds as VST3, AU, and Standalone; passes `auval`,
`pluginval --strictness-level 10`, and its own test suite.

The equaliser works. All three bands, the high-pass and the 1084 low-pass are
implemented as a shared feedback network rather than a chain of biquads, so the
bands interact and the mid bell shows proportional Q the way the hardware does.
Saturation and the transformer model are Phase 4; the custom GUI is Phase 3.

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
