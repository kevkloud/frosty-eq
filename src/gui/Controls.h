#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "LookAndFeel.h"
#include <functional>

namespace frostyeq::gui
{

/** Caption and a knob. No value readout of any kind. */
class PlainKnob final : public juce::Component
{
public:
    PlainKnob (juce::AudioProcessorValueTreeState&, const juce::String& parameterId,
               const juce::String& caption, bool polarityMarks);

    void paint (juce::Graphics&) override;
    void resized() override;

    void setKnobEnabled (bool);

private:
    juce::String caption;
    Knob knob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlainKnob)
};

//==============================================================================
/** A band: the frequency selector as a ring of detents, and the cut/boost
    control inside it.

    This is how the module puts them -- one concentric pair per band, the
    frequencies legended around the outside and the gain marked only with a
    plus and a minus. Grab the ring to change frequency, the middle to change
    gain. The frequency legend comes from the parameter itself, so the
    high-pass relabels when the model changes.
*/
class ConcentricBand final : public juce::Component,
                             private juce::Timer
{
public:
    /** An empty gainParameterId gives a plain legended ring with nothing in
        the middle, which is what the filters want. */
    ConcentricBand (juce::AudioProcessorValueTreeState&,
                    const juce::String& frequencyParameterId,
                    const juce::String& gainParameterId,
                    const juce::String& caption);

    void paint (juce::Graphics&) override;
    void resized() override;

    void setRingEnabled (bool);
    void setCentreEnabled (bool);

    /** Where the caption sits, so a switch can be tucked beside it. */
    juce::Rectangle<int> getCaptionArea() const;

private:
    void timerCallback() override;

    juce::String caption;
    juce::RangedAudioParameter* frequency = nullptr;

    Knob ring, centre;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ringAttachment, centreAttachment;

    juce::StringArray legend;
    int lastLegendModel = -1;
    juce::AudioProcessorValueTreeState& state;

    bool ringEnabled = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConcentricBand)
};

//==============================================================================
class SwitchButton final : public juce::Component
{
public:
    SwitchButton (juce::AudioProcessorValueTreeState&, const juce::String& parameterId,
                  const juce::String& text);

    void resized() override;
    void setSwitchEnabled (bool);

private:
    juce::ToggleButton button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwitchButton)
};

//==============================================================================
/** Output meter, switchable between peak dBFS and VU.

    VU is not a different scale on the same number: it is an RMS reading with
    slow ballistics, which is why it tells you about weight where a peak meter
    tells you about headroom. 0 VU sits at -18 dBFS, the usual alignment.
*/
class OutputMeter final : public juce::Component,
                          private juce::Timer
{
public:
    OutputMeter (std::function<float()> peakSource, std::function<float()> rmsSource);

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

    bool isVuMode() const noexcept { return vuMode; }

private:
    void timerCallback() override;

    std::function<float()> peak, rms;
    float displayed = 0.0f;
    bool  vuMode = false;

    static constexpr float kVuReference = -18.0f;   // dBFS at 0 VU

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutputMeter)
};

} // namespace frostyeq::gui
