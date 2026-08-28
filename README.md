# FrostyEQ

A VST3 / AU equaliser modelled on the classic 1970–71 British console EQ modules
(the 1073 and 1084 topologies), for use in Ableton Live, Logic, and other hosts.

Not affiliated with, endorsed by, or derived from any product of AMS Neve Ltd.
The circuits modelled here are over fifty years old and long out of patent; the
names and trademarks belong to their owners and are not used in this product.

## Status

Phase 1 complete. Builds as VST3, AU, and Standalone; passes `auval`,
`pluginval --strictness-level 10`, and its own test suite.

The full 17-parameter schema is in place and exposed to hosts. The level and
routing path (input gain, phase, output, dry/wet mix) is live. The EQ bands are
declared but do not filter yet -- that is Phase 2, the admittance-summed LC
network.

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

Built plugins are copied to `~/Library/Audio/Plug-Ins/` automatically.

For a release universal binary, configure with
`-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`.

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
