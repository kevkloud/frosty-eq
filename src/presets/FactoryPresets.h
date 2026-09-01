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

    Most of these push INPUT and pull OUTPUT back by a similar amount. That is
    the whole trick with this kind of unit: the colour is something you drive
    it into, not something it does sitting still.
*/
inline std::vector<Factory> factory()
{
    using namespace frostyeq::params;

    return {
        { "Init", {} },

        { "Vocal Air", {
            { kHpfFreq, 2 },        // 80 Hz -- nothing useful lives below it
            { kHfGain, 4.0f },      // the fixed 12 kHz shelf, which is the point of a 1073
            { kMidFreq, 3 }, { kMidGain, 1.5f },
            { kInputGain, 5.0f }, { kOutputLevel, -5.0f } } },

        { "Vocal Presence", {
            { kHpfFreq, 2 },
            { kMidFreq, 3 }, { kMidGain, 4.5f },   // 3.2 kHz, where a voice cuts
            { kHfGain, 2.5f },
            { kInputGain, 4.0f }, { kOutputLevel, -4.0f } } },

        { "Bass Weight", {
            { kLfFreq, 1 }, { kLfGain, 5.0f },     // 60 Hz shelf
            { kMidFreq, 1 }, { kMidGain, -2.5f },  // 700 Hz out of the way
            { kInputGain, 6.0f }, { kOutputLevel, -6.0f } } },   // hardest-working iron

        { "Kick Thump", {
            { kHpfFreq, 1 },                       // 50 Hz, subsonic only
            { kLfFreq, 2 }, { kLfGain, 4.5f },     // 110 Hz
            { kMidFreq, 0 }, { kMidGain, -3.5f },  // 360 Hz box
            { kHfGain, 2.0f },
            { kInputGain, 6.0f }, { kOutputLevel, -6.0f } } },

        { "Snare Crack", {
            { kHpfFreq, 3 },                       // 160 Hz
            { kMidFreq, 4 }, { kMidGain, 5.0f },   // 4.8 kHz
            { kHfGain, 3.0f },
            { kInputGain, 7.0f }, { kOutputLevel, -7.0f } } },

        { "Drum Bus Iron", {
            { kMidFreq, 2 }, { kMidGain, 2.0f },
            { kHfGain, 2.0f },
            { kInputGain, 10.0f }, { kOutputLevel, -10.0f } } },  // driven, not equalised

        { "Guitar Body Cut", {
            { kHpfFreq, 2 },
            { kMidFreq, 0 }, { kMidGain, -5.0f },  // 360 Hz, where guitars muddy
            { kHfGain, 3.0f },
            { kInputGain, 5.0f }, { kOutputLevel, -5.0f } } },

        { "Acoustic Sparkle", {
            { kHpfFreq, 2 },
            { kMidFreq, 1 }, { kMidGain, -2.0f },
            { kHfGain, 5.5f },
            { kInputGain, 4.0f }, { kOutputLevel, -4.0f } } },

        { "Mix Bus Sheen", {
            { kLfFreq, 0 }, { kLfGain, 1.5f },     // 35 Hz, a touch of foundation
            { kHfGain, 1.5f },
            { kInputGain, 7.0f }, { kOutputLevel, -7.0f },
            { kAutoGain, 1.0f } } },               // level-matched, so it is judged on tone

        { "Presence Lift (1084)", {
            { kModel, 1 },
            { kHfFreq, 2 }, { kHfGain, 4.0f },     // 16 kHz, which the 1073 has not got
            { kMidFreq, 5 }, { kMidGain, 3.0f }, { kMidHiQ, 1.0f },
            { kHpfFreq, 2 },
            { kInputGain, 4.0f }, { kOutputLevel, -4.0f } } },

        { "Telephone (1084)", {
            { kModel, 1 },
            { kHpfFreq, 4 },                       // 360 Hz
            { kLpfFreq, 1 },                       // 6 kHz
            { kMidFreq, 2 }, { kMidGain, 8.0f }, { kMidHiQ, 1.0f },
            { kInputGain, 8.0f }, { kOutputLevel, -8.0f } } },
    };
}

} // namespace frostyeq::presets
