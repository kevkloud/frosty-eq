#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "LookAndFeel.h"
#include <functional>

namespace frostyeq::gui
{

/** A gain control with its name underneath and nothing else: no number, only a
    plus one side and a minus the other. Input and output. */
class PlainKnob final : public juce::Component
{
public:
    PlainKnob (juce::AudioProcessorValueTreeState&, const juce::String& parameterId,
               const juce::String& caption);

    void paint (juce::Graphics&) override;
    void resized() override;

    void setKnobEnabled (bool);

private:
    /** Room under the knob for its name. */
    static constexpr int kCaptionRow = 22;

    juce::String caption;
    Knob knob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlainKnob)
};

//==============================================================================
/** A band, or a filter.

    With a gain parameter it is a band: the frequency selector is the white ring,
    legended with its switch positions, and the cut and boost sits inside it.
    Grab the ring for frequency, the middle for gain. That is how the module
    puts them, one concentric pair per band.

    Without one it is a filter: a single knob with the same legend around it.

    The legend comes from the parameter, so the cut filter relabels itself when
    the model changes.
*/
class ConcentricBand final : public juce::Component
{
public:
    ConcentricBand (juce::AudioProcessorValueTreeState&,
                    const juce::String& frequencyParameterId,
                    const juce::String& gainParameterId);

    void paint (juce::Graphics&) override;
    void resized() override;

    void setRingEnabled (bool);

private:
    /** How much narrower the frequency sweep is than the gain sweep, each
        side, in radians. */
    static constexpr float kLegendInset = 0.60f;

    void buildLegend();

    juce::RangedAudioParameter* frequency = nullptr;
    juce::AudioProcessorValueTreeState& state;

    Knob ring, centre;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ringAttachment, centreAttachment;

    juce::StringArray legend;
    bool hasCentre = false, ringEnabled = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConcentricBand)
};

//==============================================================================
class SwitchButton final : public juce::Component
{
public:
    /** `blue` picks the Hi-Q tint; everything else is pink. */
    SwitchButton (juce::AudioProcessorValueTreeState&, const juce::String& parameterId,
                  const juce::String& text, bool blue = false);

    void resized() override;
    void setSwitchEnabled (bool);

private:
    juce::ToggleButton button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwitchButton)
};

//==============================================================================
/** Output meter, switchable between peak dBFS and VU by clicking it.

    VU is not a different scale on the same number: it is an RMS reading with
    slow ballistics, 0 VU at -18 dBFS, which is why it reads weight where a peak
    meter reads headroom.
*/
class OutputMeter final : public juce::Component,
                          private juce::Timer
{
public:
    OutputMeter (std::function<float()> peakSource, std::function<float()> rmsSource);

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    std::function<float()> peak, rms;
    float displayed = 0.0f;
    bool  vuMode = false;

    static constexpr float kVuReference = -18.0f;
    static constexpr int   kBarWidth    = 14;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutputMeter)
};

} // namespace frostyeq::gui
