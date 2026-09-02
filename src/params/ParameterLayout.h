#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/ModelTables.h"
#include <atomic>

namespace frostyeq::params
{

//==============================================================================
// Parameter IDs.
//
// This is a permanent, append-only schema. Once a user saves an Ableton set,
// automation lanes and stored state are keyed by these exact strings. Renaming
// or reordering silently loses settings in every existing project. Treat a
// change here the way you would treat a wire-protocol change: don't, and if you
// must, add a new ID and migrate on load.
//==============================================================================

inline constexpr auto kHfFreq       = "hf_freq";
inline constexpr auto kHfGain       = "hf_gain";
inline constexpr auto kMidFreq      = "mid_freq";
inline constexpr auto kMidGain      = "mid_gain";
inline constexpr auto kMidHiQ       = "mid_hiq";
inline constexpr auto kLfFreq       = "lf_freq";
inline constexpr auto kLfGain       = "lf_gain";
inline constexpr auto kHpfFreq      = "hpf_freq";
inline constexpr auto kLpfFreq      = "lpf_freq";
inline constexpr auto kInputGain    = "input_gain";
inline constexpr auto kOutputLevel  = "output_level";
inline constexpr auto kEqIn         = "eq_in";
inline constexpr auto kPhase        = "phase";
inline constexpr auto kMix          = "mix";
inline constexpr auto kAutoGain     = "auto_gain";
inline constexpr auto kOversampling = "oversampling";

/** Bump only when adding parameters; existing entries keep their original hint. */
inline constexpr int kVersionHint  = 1;
inline constexpr int kStateVersion = 1;

/** Every parameter id, in panel order. Presets reset everything to its default
    before applying their own settings, so a preset cannot leave a stray value
    behind from whatever was loaded before it. */
inline juce::StringArray allIds()
{
    return { kHfFreq, kHfGain, kMidFreq, kMidGain, kMidHiQ,
             kLfFreq, kLfGain, kHpfFreq, kLpfFreq,
             kInputGain, kOutputLevel, kEqIn, kPhase, kMix, kAutoGain, kOversampling };
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout create();

} // namespace frostyeq::params
