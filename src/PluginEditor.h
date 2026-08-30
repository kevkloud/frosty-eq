#pragma once

#include "PluginProcessor.h"
#include "gui/Controls.h"
#include <memory>

/** A channel strip, not an analyser.

    Laid out the way the module is: one column, gain at the top, the bands
    below it as concentric pairs with their frequencies legended around the
    ring, the filters under those, and the output at the bottom. There is no
    response curve, no analyser, and no numeric readout on any cut or boost --
    a gain control is marked with a plus and a minus and nothing else.

    That is a deliberate choice and it came from an engineer who has used the
    hardware for years: the numbers make people mix with their eyes, hunting a
    tidy figure and flinching from a large move. Taking them away leaves the
    ear to decide. The frequency legends stay, because those are switch
    positions rather than amounts.

    The finish is Ableton's and the colour follows the module -- rose for one,
    black for the other -- rather than either unit's actual livery. Copying
    that would be trade dress, and it invites the thing to be judged as a
    failed clone instead of used on its own terms.
*/
class FrostyEqAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit FrostyEqAudioProcessorEditor (FrostyEqAudioProcessor&);
    ~FrostyEqAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void applyModel (bool is1084);

    FrostyEqAudioProcessor& processorRef;
    frostyeq::gui::FrostyLookAndFeel lookAndFeel;

    juce::ComboBox modelChooser;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modelAttachment;

    frostyeq::gui::PlainKnob inputGain, outputLevel;
    frostyeq::gui::ConcentricBand high, mid, low, highPass, lowPass;
    frostyeq::gui::SwitchButton eqIn, phase, midHiQ;
    frostyeq::gui::OutputMeter meter;

    int appliedModel = -1;
    std::vector<int> dividers;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrostyEqAudioProcessorEditor)
};
