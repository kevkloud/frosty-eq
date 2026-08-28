#include "PluginEditor.h"

FrostyEqAudioProcessorEditor::FrostyEqAudioProcessorEditor (FrostyEqAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), generic (p)
{
    addAndMakeVisible (generic);
    setResizable (true, true);
    setResizeLimits (360, 200, 1400, 900);
    setSize (520, 260);
}

void FrostyEqAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void FrostyEqAudioProcessorEditor::resized()
{
    generic.setBounds (getLocalBounds());
}
