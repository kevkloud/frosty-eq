#include "PluginProcessor.h"
#include "presets/PresetManager.h"
#include "dsp/ModelTables.h"
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
        check (proc.getParameters().size() == 16,
               "expected 16 parameters, got " + juce::String (proc.getParameters().size()));
    }

    //== Selector labels ======================================================
    {
        FrostyEqAudioProcessor proc;

        auto& hpf = param (proc, P::kHpfFreq);
        setValue (proc, P::kHpfFreq, 1.0f);           // detent 1
        check (hpf.getCurrentValueAsText() == "45 Hz",
               "low cut detent 1 should read '45 Hz', got '" + hpf.getCurrentValueAsText() + "'");

        auto& hf = param (proc, P::kHfFreq);
        setValue (proc, P::kHfFreq, 2.0f);
        check (hf.getCurrentValueAsText() == "16 kHz",
               "HF detent 2 should read '16 kHz', got '" + hf.getCurrentValueAsText() + "'");

        check (juce::approximatelyEqual (frostyeq::highShelfFreq (2), 16000.0f),
               "the high shelf should follow the selector");
    }

    //== State round-trip ======================================================
    // This is what Ableton does when it saves and reopens a set.
    {
        juce::MemoryBlock state;

        struct Setting { const char* id; float value; };
        const Setting settings[] {
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

        check (param (restored, P::kHpfFreq).getCurrentValueAsText() == "70 Hz",
               "restored low cut detent 2 should read '70 Hz', got '"
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


    //== Panel legends must agree with what the DSP actually does =============
    // The frequencies live in two places: the display strings the selector
    // shows, and the tables EqNetwork tunes its filters from. Nothing in the
    // type system ties them together, so a panel that reads 700 Hz while the
    // filter sits at 100 Hz would compile and run and look entirely fine. Every
    // figure below is from the Neve 1073 & 1084 user manual, issue 5.
    {
        FrostyEqAudioProcessor proc;

        // "360 Hz" -> 360, "1.6 kHz" -> 1600, "Off" -> 0.
        const auto parse = [] (const juce::String& label)
        {
            if (label.equalsIgnoreCase ("off"))
                return 0.0f;

            const auto number = label.upToFirstOccurrenceOf (" ", false, true).getFloatValue();
            return label.containsIgnoreCase ("kHz") ? number * 1000.0f : number;
        };

        const auto compare = [&] (const char* id, int position, float expected,
                                  const juce::String& what)
        {
            auto& rp = param (proc, id);
            const auto shown = parse (rp.getText (rp.convertTo0to1 ((float) position), 0));

            checkClose (shown, expected, 0.5,
                        what + ": the panel shows " + juce::String (shown)
                             + " Hz where the manual says " + juce::String (expected));
        };

        // Bands, identical on both modules.
        const float mid[6] { 360.0f, 700.0f, 1600.0f, 3200.0f, 4800.0f, 7200.0f };
        const float low[4] { 35.0f, 60.0f, 110.0f, 220.0f };
        const float high[3] { 10000.0f, 12000.0f, 16000.0f };

        for (int i = 0; i < 6; ++i)
        {
            compare (P::kMidFreq, i, mid[i], "mid detent " + juce::String (i));
            checkClose (frostyeq::midFreq (i), mid[i], 0.5,
                        "the mid filter should tune to " + juce::String (mid[i]) + " Hz");
        }

        for (int i = 0; i < 4; ++i)
        {
            compare (P::kLfFreq, i, low[i], "low shelf detent " + juce::String (i));
            checkClose (frostyeq::lowShelfFreq (i), low[i], 0.5,
                        "the low shelf should tune to " + juce::String (low[i]) + " Hz");
        }

        for (int i = 0; i < 3; ++i)
        {
            compare (P::kHfFreq, i, high[i], "high shelf detent " + juce::String (i));
            checkClose (frostyeq::highShelfFreq (i), high[i], 0.5,
                        "the high shelf should tune to " + juce::String (high[i]) + " Hz");
        }

        // Cut filters.
        const float lowCut[4] { 45.0f, 70.0f, 160.0f, 360.0f };
        const float highCut[5] { 6000.0f, 8000.0f, 10000.0f, 14000.0f, 18000.0f };

        for (int i = 0; i < 4; ++i)
        {
            compare (P::kHpfFreq, i + 1, lowCut[i], "low cut detent " + juce::String (i + 1));
            checkClose (frostyeq::hpfFreq (i + 1), lowCut[i], 0.5,
                        "the low cut should tune to " + juce::String (lowCut[i]) + " Hz");
        }

        for (int i = 0; i < 5; ++i)
        {
            compare (P::kLpfFreq, i + 1, highCut[i], "high cut detent " + juce::String (i + 1));
            checkClose (frostyeq::lpfFreq (i + 1), highCut[i], 0.5,
                        "the high cut should tune to " + juce::String (highCut[i]) + " Hz");
        }

        check (parse (param (proc, P::kHpfFreq).getText (0.0f, 0)) == 0.0f,
               "low cut detent 0 should read Off");
        check (parse (param (proc, P::kLpfFreq).getText (0.0f, 0)) == 0.0f,
               "high cut detent 0 should read Off");
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

            check (presets.saveUser ("Round Trip"), "saving a user preset should succeed");
            check (presets.getUserNames().contains ("Round Trip"), "it should then be listed");
            check (! presets.isEdited(), "saving should clear the edited mark");

            presets.loadFactory (0);
            checkClose (getValue (proc, P::kMidGain), 0.0f, 0.01, "cleared before reloading");

            presets.loadUser ("Round Trip");
            checkClose (getValue (proc, P::kMidGain), -6.5f, 0.01, "user preset restores mid gain");
            checkClose (getValue (proc, P::kHpfFreq),  3.0f, 0.01, "user preset restores the filter");
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

    //== A preset must not change how loud the track is ======================
    // Every factory preset drives the input and pulls the output back, because
    // that is how the colour is got. If the two do not cancel, the preset is
    // judged on being louder rather than on its tone, which is the oldest way
    // there is to make a bad move sound like a good one. The knobs being equal
    // and opposite is not enough on its own: the equaliser has its own
    // broadband gain and the saturation has more, so this measures what
    // actually comes out.
    {
        FrostyEqAudioProcessor proc;
        auto& presets = proc.getPresets();

        constexpr double rate = 48000.0;
        constexpr int block = 512;
        constexpr int blocks = 200;          // a little over two seconds

        proc.prepareToPlay (rate, block);

        // Pink noise at -18 dBFS RMS: broadband, and at the level a mix
        // actually sits at rather than at the top of the scale.
        juce::Random random (0x50f7);
        std::vector<float> source ((size_t) (block * blocks));
        {
            double b0 = 0.0, b1 = 0.0, b2 = 0.0;

            for (auto& v : source)
            {
                const auto white = (double) random.nextFloat() * 2.0 - 1.0;
                b0 = 0.99765 * b0 + white * 0.0990460;
                b1 = 0.96300 * b1 + white * 0.2965164;
                b2 = 0.57000 * b2 + white * 1.0526913;
                v = (float) ((b0 + b1 + b2 + white * 0.1848) * 0.11);
            }

            double sum = 0.0;
            for (auto v : source) sum += (double) v * v;

            const auto scale = (float) (juce::Decibels::decibelsToGain (-18.0)
                                          / std::sqrt (sum / (double) source.size()));
            for (auto& v : source) v *= scale;
        }

        double sourceSum = 0.0;
        for (auto v : source) sourceSum += (double) v * v;
        const auto sourceDb = juce::Decibels::gainToDecibels (
            std::sqrt (sourceSum / (double) source.size()));

        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;

        const auto& factory = presets.getFactory();

        for (int index = 0; index < (int) factory.size(); ++index)
        {
            presets.loadFactory (index);
            proc.reset();

            double sum = 0.0;
            int counted = 0;

            for (int b = 0; b < blocks; ++b)
            {
                for (int ch = 0; ch < 2; ++ch)
                    buffer.copyFrom (ch, 0, source.data() + (size_t) (b * block), block);

                proc.processBlock (buffer, midi);

                // The first few blocks are the smoothers arriving at the new
                // preset, which is not what the preset sounds like.
                if (b < 20)
                    continue;

                const auto* read = buffer.getReadPointer (0);

                for (int i = 0; i < block; ++i)
                    sum += (double) read[i] * read[i];

                counted += block;
            }

            const auto outDb = juce::Decibels::gainToDecibels (std::sqrt (sum / (double) counted));

            checkClose (outDb - sourceDb, 0.0, 0.5,
                        juce::String ("preset '") + factory[(size_t) index].name
                            + "' should come out at the level it went in");
        }
    }

    if (failures == 0)
        std::cout << "All parameter tests passed.\n";

    return failures == 0 ? 0 : 1;
}
