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
class FrostyEqAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit FrostyEqAudioProcessorEditor (FrostyEqAudioProcessor&);
    ~FrostyEqAudioProcessorEditor() override;

    void resized() override;

    /** The size everything is laid out at. Any other size is this, scaled. */
    static constexpr int kDesignWidth  = 280;
    static constexpr int kDesignHeight = 912;

private:
    //==========================================================================
    /** Everything on the front of the plugin, at design size. */
    class Panel final : public juce::Component
    {
    public:
        explicit Panel (FrostyEqAudioProcessor&);

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        frostyeq::gui::PresetBar presetBar;

        frostyeq::gui::PlainKnob inputGain, outputLevel;
        frostyeq::gui::ConcentricBand high, mid, low, highPass;
        frostyeq::gui::SwitchButton eqIn, phase, midHiQ;
        frostyeq::gui::OutputMeter meter;

        /** A section rule: a legend, optionally with a hairline either side. */
        struct Rule { int y; juce::String text; bool lines; };
        std::vector<Rule> rules;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Panel)
    };

    frostyeq::gui::FrostyLookAndFeel lookAndFeel;
    Panel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrostyEqAudioProcessorEditor)
};
