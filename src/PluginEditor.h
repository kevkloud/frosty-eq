#pragma once

#include "PluginProcessor.h"

/** Phase 0 editor: hosts JUCE's auto-generated control panel so we can verify
    parameter plumbing and host integration before any custom drawing exists.
    Phase 3 replaces the generic editor with the real panel + EQ curve display.
*/
class ClassicEqAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit ClassicEqAudioProcessorEditor (ClassicEqAudioProcessor&);
    ~ClassicEqAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ClassicEqAudioProcessor& processorRef;
    juce::GenericAudioProcessorEditor generic;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClassicEqAudioProcessorEditor)
};
