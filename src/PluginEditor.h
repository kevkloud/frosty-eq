#pragma once

#include "PluginProcessor.h"
#include "gui/Controls.h"
#include "gui/EqCurveDisplay.h"
#include <memory>

/** The panel.

    Control layout follows the hardware, so anyone who has used a 1073 knows
    where things are: the filters are grouped together, the three EQ bands sit
    in their own section running low to high, and each band is a gain control
    with its stepped frequency selector directly beneath -- the flat equivalent
    of the hardware's concentric pairs, with the gain knob dominant. Left to
    right is low to high, matching the curve above and the horizontal
    emulations of these units.

    The finish is Ableton's, not Neve's: flat controls drawn as value arcs,
    grouped sections, a dark response plot in the manner of EQ Eight.
    Reproducing the original's panel livery would be copying trade dress, which
    raises the same problem as using the name.
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
    void drawSection (juce::Graphics&, juce::Rectangle<int>, const juce::String& caption) const;

    FrostyEqAudioProcessor& processorRef;
    frostyeq::gui::FrostyLookAndFeel lookAndFeel;

    juce::ComboBox modelChooser;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modelAttachment;

    frostyeq::gui::EqCurveDisplay curve;
    frostyeq::gui::LevelMeter inputMeter, outputMeter;

    frostyeq::gui::LabelledKnob hpf, lpf;
    frostyeq::gui::LabelledKnob lfGain, lfFreq;
    frostyeq::gui::LabelledKnob midGain, midFreq;
    frostyeq::gui::LabelledKnob hfGain, hfFreq;
    frostyeq::gui::LabelledKnob inputGain, outputLevel, mix;

    frostyeq::gui::SwitchButton eqIn, phase, midHiQ, autoGain;

    juce::Rectangle<int> filterSection, eqSection, levelSection;

    // Tri-state on purpose. A plain bool initialised to false matches the
    // default model, so the first call would decide nothing had changed and
    // skip the update -- leaving the 1084-only controls looking live on a
    // fresh 1073, which is the state the plugin opens in.
    int appliedModel = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrostyEqAudioProcessorEditor)
};
