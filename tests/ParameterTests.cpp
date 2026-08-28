#include "PluginProcessor.h"
#include <juce_events/juce_events.h>
#include <iostream>

namespace P = frostyeq::params;

namespace
{
    int failures = 0;

    void check (bool condition, const juce::String& what)
    {
        if (! condition)
        {
            std::cerr << "FAIL: " << what << '\n';
            ++failures;
        }
    }

    juce::RangedAudioParameter& param (FrostyEqAudioProcessor& p, const char* id)
    {
        auto* rp = p.getApvts().getParameter (id);
        jassert (rp != nullptr);
        return *rp;
    }

    /** Set a choice/bool/float parameter from its real-world value. */
    void setValue (FrostyEqAudioProcessor& p, const char* id, float realValue)
    {
        auto& rp = param (p, id);
        rp.setValueNotifyingHost (rp.convertTo0to1 (realValue));
    }

    float getValue (FrostyEqAudioProcessor& p, const char* id)
    {
        auto& rp = param (p, id);
        return rp.convertFrom0to1 (rp.getValue());
    }
}

//==============================================================================
int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    //== Schema ================================================================
    {
        FrostyEqAudioProcessor proc;
        check (proc.getParameters().size() == 17,
               "expected 17 parameters, got " + juce::String (proc.getParameters().size()));
    }

    //== Model-dependent selector labels ======================================
    // The two units put different frequencies on the same high-pass detents.
    // The automation value is the switch position; only the label changes.
    {
        FrostyEqAudioProcessor proc;
        auto& hpf = param (proc, P::kHpfFreq);

        setValue (proc, P::kHpfFreq, 1.0f);           // detent 1
        const auto position = hpf.getValue();

        setValue (proc, P::kModel, (float) (int) frostyeq::Model::m1073);
        check (hpf.getCurrentValueAsText() == "50 Hz",
               "1073 HPF detent 1 should read '50 Hz', got '" + hpf.getCurrentValueAsText() + "'");

        setValue (proc, P::kModel, (float) (int) frostyeq::Model::m1084);
        check (hpf.getCurrentValueAsText() == "45 Hz",
               "1084 HPF detent 1 should read '45 Hz', got '" + hpf.getCurrentValueAsText() + "'");

        check (juce::approximatelyEqual (hpf.getValue(), position),
               "switching model must not move the switch position");

        // The 1073's high shelf is fixed at 12 kHz whatever the selector says.
        auto& hf = param (proc, P::kHfFreq);
        setValue (proc, P::kModel, (float) (int) frostyeq::Model::m1084);
        setValue (proc, P::kHfFreq, 2.0f);
        check (hf.getCurrentValueAsText() == "16 kHz",
               "1084 HF detent 2 should read '16 kHz', got '" + hf.getCurrentValueAsText() + "'");

        setValue (proc, P::kModel, (float) (int) frostyeq::Model::m1073);
        check (hf.getCurrentValueAsText() == "12 kHz",
               "1073 HF is fixed at 12 kHz, got '" + hf.getCurrentValueAsText() + "'");
    }

    //== State round-trip ======================================================
    // This is what Ableton does when it saves and reopens a set.
    {
        juce::MemoryBlock state;

        struct Setting { const char* id; float value; };
        const Setting settings[] {
            { P::kModel,        1.0f },   // 1084
            { P::kHfGain,       7.5f },
            { P::kMidFreq,      4.0f },   // 4.8 kHz
            { P::kMidGain,    -11.25f },
            { P::kMidHiQ,       1.0f },
            { P::kLfFreq,       3.0f },   // 220 Hz
            { P::kLfGain,       4.0f },
            { P::kHpfFreq,      2.0f },
            { P::kLpfFreq,      3.0f },
            { P::kInputGain,    6.0f },
            { P::kOutputLevel, -3.0f },
            { P::kPhase,        1.0f },
            { P::kMix,         62.5f },
            { P::kAutoGain,     1.0f },
            { P::kOversampling, 2.0f },
        };

        {
            FrostyEqAudioProcessor proc;

            for (const auto& s : settings)
                setValue (proc, s.id, s.value);

            proc.getStateInformation (state);
        }

        FrostyEqAudioProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());

        for (const auto& s : settings)
            check (std::abs (getValue (restored, s.id) - s.value) < 0.01f,
                   juce::String ("state round-trip lost '") + s.id + "': expected "
                       + juce::String (s.value) + ", got " + juce::String (getValue (restored, s.id)));

        // Labels must follow the restored model, not the default.
        check (param (restored, P::kHpfFreq).getCurrentValueAsText() == "70 Hz",
               "restored 1084 HPF detent 2 should read '70 Hz', got '"
                   + param (restored, P::kHpfFreq).getCurrentValueAsText() + "'");
    }

    //== Audio path is sane ====================================================
    {
        FrostyEqAudioProcessor proc;
        proc.prepareToPlay (48000.0, 512);

        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;

        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < 512; ++n)
                buffer.getWritePointer (ch)[n] = 0.5f * std::sin (juce::MathConstants<float>::twoPi * 440.0f * (float) n / 48000.0f);

        proc.processBlock (buffer, midi);

        check (buffer.getMagnitude (0, 0, 512) > 0.0f, "unity settings should pass audio");

        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < 512; ++n)
                check (std::isfinite (buffer.getReadPointer (ch)[n]), "output must be finite");
    }

    if (failures == 0)
        std::cout << "All parameter tests passed.\n";

    return failures == 0 ? 0 : 1;
}
