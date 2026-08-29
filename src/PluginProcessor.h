#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "params/ParameterLayout.h"
#include "dsp/DspCore.h"
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

    /** Rate the equaliser runs at, for the curve display. */
    double getEqSampleRate() const noexcept { return dsp.getEqSampleRate(); }

    frostyeq::Model getCurrentModel() const noexcept
    {
        return (frostyeq::Model) (int) modelParam->load (std::memory_order_relaxed);
    }

    /** Metering, written by the audio thread and polled by the editor on a
        timer. Publish-and-sample; never push from audio to UI. */
    float getInputPeak  (int ch) const noexcept { return read (inputPeak,  ch); }
    float getOutputPeak (int ch) const noexcept { return read (outputPeak, ch); }

    // Deliberately no accessor for the audio path's EqNetwork. The editor runs
    // on the message thread and the audio thread mutates those coefficients
    // continuously, so reading them to draw a curve would be a data race. The
    // curve display owns its own EqNetwork instead and drives it from the
    // parameter values -- same code, same maths, no sharing.

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    /** Oversampling choice index to factor: Off, 2x, 4x, HQ. */
    static int oversamplingFactor (int index) noexcept
    {
        constexpr int factors[] { 1, 2, 4, 8 };
        return factors[juce::jlimit (0, 3, index)];
    }
    void handleAsyncUpdate() override;

    static float read (const std::array<std::atomic<float>, 2>& a, int ch) noexcept
    {
        return a[(size_t) juce::jlimit (0, 1, ch)].load (std::memory_order_relaxed);
    }

    juce::AudioProcessorValueTreeState apvts;

    // Resolved once in the constructor. Looking parameters up by string ID on
    // the audio thread would be a hash lookup per block.
    // Resolved once in the constructor.
    std::atomic<float>* modelParam        = nullptr;
    std::atomic<float>* hfFreqParam       = nullptr;
    std::atomic<float>* hfGainParam       = nullptr;
    std::atomic<float>* midFreqParam      = nullptr;
    std::atomic<float>* midGainParam      = nullptr;
    std::atomic<float>* midHiQParam       = nullptr;
    std::atomic<float>* lfFreqParam       = nullptr;
    std::atomic<float>* lfGainParam       = nullptr;
    std::atomic<float>* hpfIndexParam     = nullptr;
    std::atomic<float>* lpfIndexParam     = nullptr;
    std::atomic<float>* inputGainParam    = nullptr;
    std::atomic<float>* outputLevelParam  = nullptr;
    std::atomic<float>* mixParam          = nullptr;
    std::atomic<float>* phaseParam        = nullptr;
    std::atomic<float>* eqInParam         = nullptr;
    std::atomic<float>* autoGainParam     = nullptr;
    std::atomic<float>* oversamplingParam = nullptr;

    // The high-pass selector's labels depend on the active model.
    frostyeq::params::PositionalChoice* hpfFreqChoice = nullptr;

    frostyeq::DspCore dsp;

    std::array<std::atomic<float>, 2> inputPeak  { };
    std::array<std::atomic<float>, 2> outputPeak { };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrostyEqAudioProcessor)
};
