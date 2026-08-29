#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "dsp/EqNetwork.h"

namespace frostyeq::gui
{

/** Magnitude response plot, in the spirit of EQ Eight's display.

    Owns a private EqNetwork and drives it from the parameter values rather
    than reading the audio path's coefficients, which the audio thread is
    mutating continuously. Same class, same maths, so the drawn curve cannot
    disagree with what is heard -- but nothing is shared across threads.
*/
class EqCurveDisplay final : public juce::Component,
                             private juce::Timer
{
public:
    explicit EqCurveDisplay (juce::AudioProcessorValueTreeState&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    bool refreshSettings();          // returns true if anything changed
    void rebuildCurve();

    float frequencyToX (double hz) const noexcept;
    float decibelsToY  (double db) const noexcept;

    juce::AudioProcessorValueTreeState& state;

    EqNetwork   network;
    EqSettings  settings;
    bool        eqIn = true;

    juce::Path  curve;
    juce::Rectangle<float> plot;

    static constexpr double kMinHz = 20.0, kMaxHz = 20000.0;
    static constexpr double kRangeDb = 24.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqCurveDisplay)
};

} // namespace frostyeq::gui
