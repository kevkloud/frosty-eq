#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

//==============================================================================
/** Phase 0 skeleton: a single gain parameter, wired through APVTS.

    The structure here is deliberately the one we keep: parameters live in an
    AudioProcessorValueTreeState, the editor is decoupled from DSP state, and
    metering values cross the thread boundary through atomics only.
*/
class ClassicEqAudioProcessor final : public juce::AudioProcessor
{
public:
    ClassicEqAudioProcessor();
    ~ClassicEqAudioProcessor() override = default;

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
    juce::AudioProcessorValueTreeState& getApvts() noexcept   { return apvts; }

    /** Post-processing peak level, written by the audio thread and polled by
        the editor on a timer. Never read DSP state from the message thread. */
    float getOutputPeak (int channel) const noexcept
    {
        return outputPeak[(size_t) juce::jlimit (0, 1, channel)].load (std::memory_order_relaxed);
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    // Cached raw pointer into APVTS. Looking parameters up by string ID on the
    // audio thread would be a hash lookup per block; resolve it once instead.
    std::atomic<float>* gainDbParam = nullptr;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gainSmoothed;

    std::array<std::atomic<float>, 2> outputPeak { };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClassicEqAudioProcessor)
};
