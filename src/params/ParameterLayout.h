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

inline constexpr auto kModel        = "model";
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

//==============================================================================
/** A stepped selector whose displayed labels depend on which console model is
    active, while its automation value stays a plain switch position.

    The 1073 and 1084 high-pass filters have different frequencies at the same
    detents (50/80/160/300 vs 45/70/160/360). Encoding the *position* rather
    than the frequency means switching models lands you on the corresponding
    detent of the other unit's table, deterministically and round-trip-safely,
    and it sidesteps the fact that a VST3 discrete parameter cannot change its
    step count at runtime.
*/
class PositionalChoice final : public juce::AudioParameterChoice
{
public:
    PositionalChoice (juce::ParameterID pid,
                      const juce::String& paramName,
                      juce::StringArray labels1073,
                      juce::StringArray labels1084,
                      int defaultIndex);

    juce::String getText (float normalisedValue, int maximumStringLength) const override;

    /** Called by the processor whenever the model parameter changes. */
    void setModel (Model m) noexcept { model.store ((int) m, std::memory_order_relaxed); }

private:
    juce::StringArray labelsA, labelsB;
    std::atomic<int> model { (int) Model::m1073 };
};

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout create();

} // namespace frostyeq::params
