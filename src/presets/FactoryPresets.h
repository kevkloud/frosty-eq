#pragma once

#include "params/ParameterLayout.h"
#include <vector>

namespace frostyeq::presets
{

/** One parameter setting inside a preset. Frequencies are switch positions, so
    the value is the detent index, not a frequency in Hz. */
struct Setting
{
    const char* id;
    float value;
};

struct Factory
{
    const char* name;
    std::vector<Setting> settings;
};

/** The presets that ship with the plugin.

    These are starting points, not verdicts. They are built from what these
    controls are conventionally used for -- the frequency choices on the
    original were picked for particular jobs, and the panel legends have not
    changed in fifty years -- and they were checked against what the model
    actually measures. They have not been checked by ear on real material,
    which is the only test that finally matters, so treat them as somewhere to
    start rather than somewhere to stop.

    Anything a preset does not mention goes back to its default, so a preset
    cannot leave a stray setting behind from whatever was loaded before it.

    Most of these push INPUT and pull OUTPUT back. That is the whole trick with
    this kind of unit: the colour is something you drive it into, not something
    it does sitting still.

    The two are not simply equal and opposite. The equaliser has a broadband
    gain of its own and the saturation has more, so cancelling the knobs still
    leaves a preset louder or quieter than what went in -- and a preset that
    arrives louder gets credit for it, which is the oldest way there is to make
    a bad move sound like a good one. Every OUTPUT figure below is the one that
    measured flat through the whole chain on pink noise, which is why they are
    odd numbers. A test in tests/ParameterTests.cpp holds them there.
*/
inline std::vector<Factory> factory()
{
    using namespace frostyeq::params;

    return {
        { "Init", {} },

        { "Vocal Air", {
            { kHpfFreq, 2 },        // 70 Hz -- nothing useful lives below it
            { kHfGain, 4.0f },      // air off the 12 kHz shelf
            { kMidFreq, 3 }, { kMidGain, 1.5f },
            { kInputGain, 5.0f }, { kOutputLevel, -4.2f } } },

        { "Vocal Presence", {
            { kHpfFreq, 2 },
            { kMidFreq, 3 }, { kMidGain, 4.5f },   // 3.2 kHz, where a voice cuts
            { kHfGain, 2.5f },
            { kInputGain, 4.0f }, { kOutputLevel, -4.0f } } },

        { "Bass Weight", {
            { kLfFreq, 1 }, { kLfGain, 5.0f },     // 60 Hz shelf
            { kMidFreq, 1 }, { kMidGain, -2.5f },  // 700 Hz out of the way
            { kInputGain, 6.0f }, { kOutputLevel, -7.1f } } },   // hardest-working iron

        { "Kick Thump", {
            { kHpfFreq, 1 },                       // 45 Hz, subsonic only
            { kLfFreq, 2 }, { kLfGain, 4.5f },     // 110 Hz
            { kMidFreq, 0 }, { kMidGain, -3.5f },  // 360 Hz box
            { kHfGain, 2.0f },
            { kInputGain, 6.0f }, { kOutputLevel, -5.2f } } },

        { "Snare Crack", {
            { kHpfFreq, 3 },                       // 160 Hz
            { kMidFreq, 4 }, { kMidGain, 5.0f },   // 4.8 kHz
            { kHfGain, 3.0f },
            { kInputGain, 7.0f }, { kOutputLevel, -6.4f } } },

        { "Drum Bus Iron", {
            { kMidFreq, 2 }, { kMidGain, 2.0f },
            { kHfGain, 2.0f },
            { kInputGain, 10.0f }, { kOutputLevel, -10.0f } } },  // driven, not equalised

        { "Guitar Body Cut", {
            { kHpfFreq, 2 },
            { kMidFreq, 0 }, { kMidGain, -5.0f },  // 360 Hz, where guitars muddy
            { kHfGain, 3.0f },
            { kInputGain, 5.0f }, { kOutputLevel, -2.9f } } },

        { "Acoustic Sparkle", {
            { kHpfFreq, 2 },
            { kMidFreq, 1 }, { kMidGain, -2.0f },
            { kHfGain, 5.5f },
            { kInputGain, 4.0f }, { kOutputLevel, -3.1f } } },

        { "Mix Bus Sheen", {
            { kLfFreq, 0 }, { kLfGain, 1.5f },     // 35 Hz, a touch of foundation
            { kHfGain, 1.5f },
            { kInputGain, 7.0f }, { kOutputLevel, -7.0f },
            { kAutoGain, 1.0f } } },               // level-matched, so it is judged on tone

        { "Presence Lift", {
            { kHfFreq, 2 }, { kHfGain, 4.0f },     // 16 kHz
            { kMidFreq, 5 }, { kMidGain, 3.0f }, { kMidHiQ, 1.0f },
            { kHpfFreq, 2 },
            { kInputGain, 4.0f }, { kOutputLevel, -3.0f } } },

        { "Telephone", {
            { kHpfFreq, 4 },                       // 360 Hz
            { kLpfFreq, 1 },                       // 6 kHz
            { kMidFreq, 2 }, { kMidGain, 8.0f }, { kMidHiQ, 1.0f },
            { kInputGain, 8.0f }, { kOutputLevel, -6.3f } } },
    };
}

} // namespace frostyeq::presets
