#include "PluginEditor.h"

ClassicEqAudioProcessorEditor::ClassicEqAudioProcessorEditor (ClassicEqAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p), generic (p)
{
    addAndMakeVisible (generic);
    setResizable (true, true);
    setResizeLimits (360, 200, 1400, 900);
    setSize (520, 260);
}

void ClassicEqAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void ClassicEqAudioProcessorEditor::resized()
{
    generic.setBounds (getLocalBounds());
}
