#pragma once

#include "PluginProcessor.h"
#include "gui/Controls.h"
#include "gui/PresetBar.h"
#include <memory>
#include <vector>

/** A channel strip, not an analyser.

    Laid out the way the module is: one narrow column, gain at the top, the
    bands below it as concentric pairs with their frequencies legended around
    the ring, the filters under those, and the output at the bottom.

    There is no response curve, no analyser, and no numeric readout on any cut
    or boost -- a gain control is marked with a plus and a minus and nothing
    else. That came from an engineer who has spent years on the hardware: the
    numbers make people mix with their eyes, hunting a tidy figure and
    flinching from a large move. Frequency legends stay, because a switch
    position is not an amount.

    The panel is drawn once at a fixed size and scaled as a whole. Laying it out
    again at each new size, which is what it used to do, keeps the controls the
    same size while the gaps between them stretch, so the design comes apart as
    soon as it is not at its default size. A single uniform transform scales
    knobs, legends, fonts and spacing together.
*/
class FrostyEqAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit FrostyEqAudioProcessorEditor (FrostyEqAudioProcessor&);
    ~FrostyEqAudioProcessorEditor() override;

    void resized() override;

    /** The size everything is laid out at. Any other size is this, scaled. */
    static constexpr int kDesignWidth  = 260;
    static constexpr int kDesignHeight = 752;

private:
    //==========================================================================
    /** Everything on the front of the plugin, at design size. */
    class Panel final : public juce::Component
    {
    public:
        explicit Panel (FrostyEqAudioProcessor&);

        void paint (juce::Graphics&) override;
        void resized() override;

        void applyModel (bool is1084);

    private:
        frostyeq::gui::PresetBar presetBar;
        juce::ComboBox modelChooser;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modelAttachment;

        frostyeq::gui::PlainKnob inputGain, outputLevel;
        frostyeq::gui::ConcentricBand high, mid, low, highPass, lowPass;
        frostyeq::gui::SwitchButton eqIn, phase, midHiQ;
        frostyeq::gui::OutputMeter meter;

        std::vector<int> dividers;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Panel)
    };

    void timerCallback() override;

    FrostyEqAudioProcessor& processorRef;
    frostyeq::gui::FrostyLookAndFeel lookAndFeel;
    Panel panel;

    int appliedModel = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrostyEqAudioProcessorEditor)
};
