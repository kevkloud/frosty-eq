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
    const auto bind = [this] (const char* id) { return apvts.getRawParameterValue (id); };

    modelParam       = bind (P::kModel);
    hfFreqParam      = bind (P::kHfFreq);
    hfGainParam      = bind (P::kHfGain);
    midFreqParam     = bind (P::kMidFreq);
    midGainParam     = bind (P::kMidGain);
    midHiQParam      = bind (P::kMidHiQ);
    lfFreqParam      = bind (P::kLfFreq);
    lfGainParam      = bind (P::kLfGain);
    hpfIndexParam    = bind (P::kHpfFreq);
    lpfIndexParam    = bind (P::kLpfFreq);
    inputGainParam   = bind (P::kInputGain);
    outputLevelParam = bind (P::kOutputLevel);
    mixParam         = bind (P::kMix);
    phaseParam       = bind (P::kPhase);
    eqInParam        = bind (P::kEqIn);
    autoGainParam    = bind (P::kAutoGain);

    jassert (modelParam != nullptr && midGainParam != nullptr && mixParam != nullptr);

    hpfFreqChoice = dynamic_cast<P::PositionalChoice*> (apvts.getParameter (P::kHpfFreq));
    jassert (hpfFreqChoice != nullptr);

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

    // Fired from whichever thread moved the parameter, so this stays
    // allocation- and lock-free: one atomic store plus an async trigger.
    if (hpfFreqChoice != nullptr)
        hpfFreqChoice->setModel ((Model) (int) newValue);

    triggerAsyncUpdate();
}

void FrostyEqAudioProcessor::handleAsyncUpdate()
{
    // Switching models changes what the high-pass detents are called, so the
    // host must re-read the parameter text. Message thread only.
    updateHostDisplay();
}

//==============================================================================
void FrostyEqAudioProcessor::prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock)
{
    dsp.prepare (sampleRate, maximumExpectedSamplesPerBlock, getTotalNumOutputChannels());

    for (auto* a : { &inputPeak, &outputPeak })
        for (auto& p : *a)
            p.store (0.0f, std::memory_order_relaxed);

    // Phase 4 reports oversampling latency here.
    setLatencySamples (0);
}

void FrostyEqAudioProcessor::releaseResources()
{
    dsp.reset();
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

    for (int ch = 0; ch < juce::jmin (2, numOut); ++ch)
        inputPeak[(size_t) ch].store (buffer.getMagnitude (ch, 0, numSamples),
                                      std::memory_order_relaxed);

    const auto load = [] (const std::atomic<float>* p) { return p->load (std::memory_order_relaxed); };

    DspCore::Params p;
    p.model         = (Model) (int) load (modelParam);
    p.hfFreqIndex   = (int) load (hfFreqParam);
    p.midFreqIndex  = (int) load (midFreqParam);
    p.lfFreqIndex   = (int) load (lfFreqParam);
    p.hpfIndex      = (int) load (hpfIndexParam);
    p.lpfIndex      = (int) load (lpfIndexParam);
    p.hfGainDb      = load (hfGainParam);
    p.midGainDb     = load (midGainParam);
    p.lfGainDb      = load (lfGainParam);
    p.midHiQ        = load (midHiQParam) > 0.5f;
    p.inputGainDb   = load (inputGainParam);
    p.outputLevelDb = load (outputLevelParam);
    p.mixPercent    = load (mixParam);
    p.eqIn          = load (eqInParam) > 0.5f;
    p.phaseInvert   = load (phaseParam) > 0.5f;
    p.autoGain      = load (autoGainParam) > 0.5f;

    dsp.setParams (p);
    dsp.process (buffer.getArrayOfWritePointers(), numOut, numSamples);

    for (int ch = 0; ch < juce::jmin (2, numOut); ++ch)
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
