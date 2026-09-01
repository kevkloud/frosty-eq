#include "PluginProcessor.h"
#include "presets/PresetManager.h"
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

    void checkClose (double actual, double expected, double tolerance, const juce::String& what)
    {
        if (! (std::abs (actual - expected) <= tolerance))
        {
            std::cerr << "FAIL: " << what << " -- expected " << expected
                      << " +/- " << tolerance << ", got " << actual << '\n';
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

        // The high-shelf selector reads the same in both models -- only the
        // 1084 can actually move the shelf, and the clamp lives in the DSP so
        // the menu does not show three identical entries.
        auto& hf = param (proc, P::kHfFreq);
        setValue (proc, P::kHfFreq, 2.0f);
        check (hf.getCurrentValueAsText() == "16 kHz",
               "HF detent 2 should read '16 kHz', got '" + hf.getCurrentValueAsText() + "'");

        for (int hfPos = 0; hfPos < 3; ++hfPos)
            check (juce::approximatelyEqual (frostyeq::highShelfFreq (frostyeq::Model::m1073, hfPos), 12000.0f),
                   "the 1073's shelf is fixed at 12 kHz whatever the selector says");

        check (juce::approximatelyEqual (frostyeq::highShelfFreq (frostyeq::Model::m1084, 2), 16000.0f),
               "the 1084's shelf should follow the selector");
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


    //== Presets ==============================================================
    {
        // Somewhere disposable, so a test run never touches real presets.
        const auto sandbox = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("FrostyEQPresetTests");
        sandbox.deleteRecursively();
        sandbox.createDirectory();
        frostyeq::PresetManager::setDirectoryForTesting (sandbox);

        FrostyEqAudioProcessor proc;
        auto& presets = proc.getPresets();

        check (! presets.getFactory().empty(), "there should be factory presets");

        // Every factory preset must load and land on what it asks for.
        for (int i = 0; i < (int) presets.getFactory().size(); ++i)
        {
            const auto& preset = presets.getFactory()[(size_t) i];
            presets.loadFactory (i);

            check (presets.getCurrentName() == preset.name,
                   juce::String ("loading should name the preset, got ") + presets.getCurrentName());

            check (! presets.isEdited(), "a freshly loaded preset is not edited");

            for (const auto& setting : preset.settings)
                check (std::abs (getValue (proc, setting.id) - setting.value) < 0.01f,
                       juce::String ("preset \"") + preset.name + "\" should set " + setting.id
                           + " to " + juce::String (setting.value) + ", got "
                           + juce::String (getValue (proc, setting.id)));
        }

        // The guarantee that keeps presets honest: anything a preset does not
        // mention goes back to its default, rather than inheriting whatever the
        // previous preset left behind.
        {
            setValue (proc, P::kPhase, 1.0f);
            setValue (proc, P::kMix,  40.0f);
            setValue (proc, P::kLpfFreq, 4.0f);

            presets.loadFactory (0);   // Init, which sets nothing

            for (const auto& id : P::allIds())
            {
                auto* param = proc.getApvts().getParameter (id);
                checkClose (param->getValue(), param->getDefaultValue(), 1.0e-5,
                            juce::String ("loading a preset should default ") + id);
            }
        }

        // Moving a control marks it edited; loading clears that again.
        {
            presets.loadFactory (1);
            check (! presets.isEdited(), "just loaded, so not edited");

            setValue (proc, P::kMidGain, 7.0f);
            check (presets.isEdited(), "moving a control should mark the preset edited");

            presets.loadFactory (1);
            check (! presets.isEdited(), "reloading should clear the edited mark");
        }

        // Save, change everything, load it back.
        {
            presets.loadFactory (0);
            setValue (proc, P::kMidGain,  -6.5f);
            setValue (proc, P::kHpfFreq,   3.0f);
            setValue (proc, P::kModel,     1.0f);

            check (presets.saveUser ("Round Trip"), "saving a user preset should succeed");
            check (presets.getUserNames().contains ("Round Trip"), "it should then be listed");
            check (! presets.isEdited(), "saving should clear the edited mark");

            presets.loadFactory (0);
            checkClose (getValue (proc, P::kMidGain), 0.0f, 0.01, "cleared before reloading");

            presets.loadUser ("Round Trip");
            checkClose (getValue (proc, P::kMidGain), -6.5f, 0.01, "user preset restores mid gain");
            checkClose (getValue (proc, P::kHpfFreq),  3.0f, 0.01, "user preset restores the filter");
            checkClose (getValue (proc, P::kModel),    1.0f, 0.01, "user preset restores the model");
        }

        // Export somewhere else, import it back.
        {
            const auto away = sandbox.getChildFile ("elsewhere/Shared.frostyeq");

            presets.loadUser ("Round Trip");
            check (presets.exportTo (away), "exporting should write a file");
            check (away.existsAsFile(), "and the file should be there");

            presets.loadFactory (0);
            check (presets.importFrom (away), "importing should succeed");

            check (presets.getUserNames().contains ("Shared"),
                   "an imported preset should join the list, not just load once");

            checkClose (getValue (proc, P::kMidGain), -6.5f, 0.01, "import restores the settings");
        }

        // Stepping walks factory presets then the user's, and wraps.
        {
            presets.loadFactory (0);
            presets.step (-1);
            check (presets.getCurrentName() != "Init", "stepping back from the first should wrap");

            presets.loadFactory (0);
            presets.step (1);
            check (presets.getCurrentName() == presets.getFactory()[1].name,
                   "stepping forward should reach the next factory preset");
        }

        check (presets.deleteUser ("Round Trip"), "deleting a user preset should succeed");
        check (! presets.getUserNames().contains ("Round Trip"), "and remove it from the list");

        sandbox.deleteRecursively();
        frostyeq::PresetManager::setDirectoryForTesting ({});
    }

    if (failures == 0)
        std::cout << "All parameter tests passed.\n";

    return failures == 0 ? 0 : 1;
}
