#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace frostyeq;
namespace P = frostyeq::params;

//==============================================================================
FrostyEqAudioProcessor::FrostyEqAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", P::create())
{
    modelParam       = apvts.getRawParameterValue (P::kModel);
    inputGainParam   = apvts.getRawParameterValue (P::kInputGain);
    outputLevelParam = apvts.getRawParameterValue (P::kOutputLevel);
    mixParam         = apvts.getRawParameterValue (P::kMix);
    phaseParam       = apvts.getRawParameterValue (P::kPhase);
    eqInParam        = apvts.getRawParameterValue (P::kEqIn);
    autoGainParam    = apvts.getRawParameterValue (P::kAutoGain);

    jassert (modelParam != nullptr && inputGainParam != nullptr
             && outputLevelParam != nullptr && mixParam != nullptr);

    hfFreqParam  = dynamic_cast<P::PositionalChoice*> (apvts.getParameter (P::kHfFreq));
    hpfFreqParam = dynamic_cast<P::PositionalChoice*> (apvts.getParameter (P::kHpfFreq));
    jassert (hfFreqParam != nullptr && hpfFreqParam != nullptr);

    apvts.addParameterListener (P::kModel, this);
    parameterChanged (P::kModel, modelParam->load());
}

FrostyEqAudioProcessor::~FrostyEqAudioProcessor()
{
    apvts.removeParameterListener (P::kModel, this);
    cancelPendingUpdate();
}

//==============================================================================
void FrostyEqAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID != P::kModel)
        return;

    // Fired from whichever thread moved the parameter, so this must stay
    // allocation- and lock-free: two atomic stores plus an async trigger.
    const auto m = (Model) (int) newValue;

    if (hfFreqParam  != nullptr) hfFreqParam->setModel (m);
    if (hpfFreqParam != nullptr) hpfFreqParam->setModel (m);

    triggerAsyncUpdate();
}

void FrostyEqAudioProcessor::handleAsyncUpdate()
{
    // Switching models changes what the frequency selectors are called, so the
    // host needs to re-read the parameter text. Message thread only.
    updateHostDisplay();
}

//==============================================================================
void FrostyEqAudioProcessor::prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock)
{
    constexpr double rampSeconds = 0.02;

    inputGainSmoothed.reset   (sampleRate, rampSeconds);
    outputLevelSmoothed.reset (sampleRate, rampSeconds);
    mixSmoothed.reset         (sampleRate, rampSeconds);

    inputGainSmoothed.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (inputGainParam->load (std::memory_order_relaxed)));
    outputLevelSmoothed.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (outputLevelParam->load (std::memory_order_relaxed)));
    mixSmoothed.setCurrentAndTargetValue (mixParam->load (std::memory_order_relaxed) * 0.01f);

    // All allocation happens here so that processBlock never has to.
    dryBuffer.setSize (getTotalNumOutputChannels(), maximumExpectedSamplesPerBlock,
                       false, false, true);
    dryBuffer.clear();

    for (auto* a : { &inputPeak, &outputPeak })
        for (auto& p : *a)
            p.store (0.0f, std::memory_order_relaxed);

    // Phase 4 reports oversampling latency here.
    setLatencySamples (0);
}

void FrostyEqAudioProcessor::releaseResources()
{
    dryBuffer.setSize (0, 0);
}

bool FrostyEqAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    // The originals are mono modules; we run mono or stereo, but do not convert
    // between them.
    return layouts.getMainInputChannelSet() == out;
}

//==============================================================================
void FrostyEqAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // Denormals in IIR filter tails cost roughly 100x CPU and are a classic
    // source of mystery dropouts. Must be first.
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    const auto numIn      = getTotalNumInputChannels();
    const auto numOut     = getTotalNumOutputChannels();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    const auto numCh = juce::jmin (numOut, dryBuffer.getNumChannels());

    for (int ch = 0; ch < juce::jmin (2, numCh); ++ch)
        inputPeak[(size_t) ch].store (buffer.getMagnitude (ch, 0, numSamples),
                                      std::memory_order_relaxed);

    // Keep the unprocessed signal for the Mix blend.
    for (int ch = 0; ch < numCh; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    inputGainSmoothed.setTargetValue (
        juce::Decibels::decibelsToGain (inputGainParam->load (std::memory_order_relaxed)));
    outputLevelSmoothed.setTargetValue (
        juce::Decibels::decibelsToGain (outputLevelParam->load (std::memory_order_relaxed)));
    mixSmoothed.setTargetValue (mixParam->load (std::memory_order_relaxed) * 0.01f);

    const auto polarity = phaseParam->load (std::memory_order_relaxed) > 0.5f ? -1.0f : 1.0f;

    // Declared, not yet honoured. eqIn gates the EQ network (Phase 2); autoGain
    // compensates the network's insertion gain (Phase 4). Both are read here so
    // the wiring is exercised and the parameters are not dead.
    juce::ignoreUnused (eqInParam, autoGainParam);

    for (int n = 0; n < numSamples; ++n)
    {
        const auto preGain  = inputGainSmoothed.getNextValue() * polarity;
        const auto postGain = outputLevelSmoothed.getNextValue();
        const auto wet      = mixSmoothed.getNextValue();
        const auto dry      = 1.0f - wet;

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);

            // Phase 2 inserts the EQ network between these two gain stages.
            const auto processed = d[n] * preGain * postGain;

            d[n] = processed * wet + dryBuffer.getReadPointer (ch)[n] * dry;
        }
    }

    for (int ch = 0; ch < juce::jmin (2, numCh); ++ch)
        outputPeak[(size_t) ch].store (buffer.getMagnitude (ch, 0, numSamples),
                                       std::memory_order_relaxed);
}

//==============================================================================
juce::AudioProcessorEditor* FrostyEqAudioProcessor::createEditor()
{
    return new FrostyEqAudioProcessorEditor (*this);
}

void FrostyEqAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", P::kStateVersion, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void FrostyEqAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    // Future versions migrate here, keyed off the stored stateVersion property.
    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    // Restoring state can change the model without going through the listener.
    parameterChanged (P::kModel, modelParam->load (std::memory_order_relaxed));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FrostyEqAudioProcessor();
}
