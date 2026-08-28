#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Parameter IDs are a permanent, append-only schema: once a user saves an
    // Ableton set, automation and state are keyed by these strings. Treat any
    // change here the way you would treat a wire-protocol change.
    constexpr auto kGainId      = "output_level";
    constexpr int  kStateVersion = 1;
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
ClassicEqAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { kGainId, kStateVersion },
        "Output",
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.01f },
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return layout;
}

ClassicEqAudioProcessor::ClassicEqAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    gainDbParam = apvts.getRawParameterValue (kGainId);
    jassert (gainDbParam != nullptr);
}

//==============================================================================
void ClassicEqAudioProcessor::prepareToPlay (double sampleRate, int)
{
    // Everything allocation-shaped happens here, never in processBlock.
    gainSmoothed.reset (sampleRate, 0.02);
    gainSmoothed.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (gainDbParam->load (std::memory_order_relaxed)));

    for (auto& p : outputPeak)
        p.store (0.0f, std::memory_order_relaxed);

    setLatencySamples (0);
}

void ClassicEqAudioProcessor::releaseResources() {}

bool ClassicEqAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    // The 1073 is a mono unit; we support mono and stereo, but not format
    // conversion between them.
    return layouts.getMainInputChannelSet() == out;
}

void ClassicEqAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer&)
{
    // Denormals in IIR filter tails cost ~100x CPU on x86 and are a classic
    // source of mystery dropouts. This must be the first thing in the block.
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples  = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    gainSmoothed.setTargetValue (
        juce::Decibels::decibelsToGain (gainDbParam->load (std::memory_order_relaxed)));

    for (int n = 0; n < numSamples; ++n)
    {
        const auto g = gainSmoothed.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer (ch)[n] *= g;
    }

    // Publish metering for the editor to sample. Write-only from here.
    for (int ch = 0; ch < juce::jmin (2, numChannels); ++ch)
        outputPeak[(size_t) ch].store (buffer.getMagnitude (ch, 0, numSamples),
                                       std::memory_order_relaxed);
}

//==============================================================================
juce::AudioProcessorEditor* ClassicEqAudioProcessor::createEditor()
{
    return new ClassicEqAudioProcessorEditor (*this);
}

void ClassicEqAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", kStateVersion, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ClassicEqAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    // Future versions migrate here, keyed off the stored stateVersion.
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ClassicEqAudioProcessor();
}
