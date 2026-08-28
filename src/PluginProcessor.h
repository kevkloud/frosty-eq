#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "params/ParameterLayout.h"
#include <array>
#include <atomic>

//==============================================================================
/** Phase 1: the full parameter schema is in place and the level/routing path is
    live. The EQ bands are declared but do not yet filter anything — that is
    Phase 2 (the admittance-summed LC network in dsp/EqNetwork).
*/
class FrostyEqAudioProcessor final : public juce::AudioProcessor,
                                     private juce::AudioProcessorValueTreeState::Listener,
                                     private juce::AsyncUpdater
{
public:
    FrostyEqAudioProcessor();
    ~FrostyEqAudioProcessor() override;

    //== AudioProcessor ========================================================
    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                          { return true; }

    const juce::String getName() const override              { return JucePlugin_Name; }
    bool acceptsMidi() const override                        { return false; }
    bool producesMidi() const override                       { return false; }
    bool isMidiEffect() const override                       { return false; }
    double getTailLengthSeconds() const override             { return 0.0; }

    int getNumPrograms() override                            { return 1; }
    int getCurrentProgram() override                         { return 0; }
    void setCurrentProgram (int) override                    {}
    const juce::String getProgramName (int) override         { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //== Ours ==================================================================
    juce::AudioProcessorValueTreeState& getApvts() noexcept  { return apvts; }

    frostyeq::Model getCurrentModel() const noexcept
    {
        return (frostyeq::Model) (int) modelParam->load (std::memory_order_relaxed);
    }

    /** Metering, written by the audio thread and polled by the editor on a
        timer. Publish-and-sample; never push from audio to UI. */
    float getInputPeak  (int ch) const noexcept { return read (inputPeak,  ch); }
    float getOutputPeak (int ch) const noexcept { return read (outputPeak, ch); }

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;

    static float read (const std::array<std::atomic<float>, 2>& a, int ch) noexcept
    {
        return a[(size_t) juce::jlimit (0, 1, ch)].load (std::memory_order_relaxed);
    }

    juce::AudioProcessorValueTreeState apvts;

    // Resolved once in the constructor. Looking parameters up by string ID on
    // the audio thread would be a hash lookup per block.
    std::atomic<float>* modelParam       = nullptr;
    std::atomic<float>* inputGainParam   = nullptr;
    std::atomic<float>* outputLevelParam = nullptr;
    std::atomic<float>* mixParam         = nullptr;
    std::atomic<float>* phaseParam       = nullptr;
    std::atomic<float>* eqInParam        = nullptr;
    std::atomic<float>* autoGainParam    = nullptr;

    // Selectors whose labels depend on the active model.
    frostyeq::params::PositionalChoice* hfFreqParam  = nullptr;
    frostyeq::params::PositionalChoice* hpfFreqParam = nullptr;

    using Smoothed = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;
    Smoothed inputGainSmoothed, outputLevelSmoothed, mixSmoothed;

    // Preallocated in prepareToPlay; the wet/dry split must not allocate.
    juce::AudioBuffer<float> dryBuffer;

    std::array<std::atomic<float>, 2> inputPeak  { };
    std::array<std::atomic<float>, 2> outputPeak { };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrostyEqAudioProcessor)
};
