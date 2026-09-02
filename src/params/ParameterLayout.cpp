#include "ParameterLayout.h"

namespace frostyeq::params
{

namespace
{
    juce::AudioParameterFloatAttributes dbAttr()
    {
        return juce::AudioParameterFloatAttributes()
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int)
            {
                // Signed, one decimal, with the unit -- what the readout under
                // each knob shows.
                return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB";
            });
    }

    std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id,
                                                          const juce::String& paramName,
                                                          float min, float max,
                                                          float step, float def,
                                                          juce::AudioParameterFloatAttributes attr = {})
    {
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, kVersionHint }, paramName,
            juce::NormalisableRange<float> { min, max, step }, def, std::move (attr));
    }

    std::unique_ptr<juce::AudioParameterChoice> makeChoice (const char* id,
                                                            const juce::String& paramName,
                                                            juce::StringArray choices,
                                                            int def)
    {
        return std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { id, kVersionHint }, paramName, std::move (choices), def);
    }

    std::unique_ptr<juce::AudioParameterBool> makeBool (const char* id,
                                                        const juce::String& paramName,
                                                        bool def)
    {
        return std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { id, kVersionHint }, paramName, def);
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout create()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // -- High shelf ----------------------------------------------------------
    layout.add (makeChoice (kHfFreq, "HF Freq", { "10 kHz", "12 kHz", "16 kHz" }, 1));

    layout.add (makeFloat (kHfGain, "HF Gain", -16.0f, 16.0f, 0.01f, 0.0f, dbAttr()));

    // -- Mid bell ------------------------------------------------------------
    layout.add (makeChoice (kMidFreq, "Mid Freq",
        { "360 Hz", "700 Hz", "1.6 kHz", "3.2 kHz", "4.8 kHz", "7.2 kHz" }, 2));

    layout.add (makeFloat (kMidGain, "Mid Gain", -18.0f, 18.0f, 0.01f, 0.0f, dbAttr()));
    layout.add (makeBool  (kMidHiQ,  "Mid Hi-Q", false));   // 1084 only

    // -- Low shelf -----------------------------------------------------------
    layout.add (makeChoice (kLfFreq, "LF Freq",
        { "35 Hz", "60 Hz", "110 Hz", "220 Hz" }, 1));

    layout.add (makeFloat (kLfGain, "LF Gain", -16.0f, 16.0f, 0.01f, 0.0f, dbAttr()));

    // -- Cut filters ---------------------------------------------------------
    // Frequencies per the user manual; the tests hold these strings and the
    // tables the filters are tuned from to the same figures.
    layout.add (makeChoice (kHpfFreq, "Low Cut",
        { "Off", "45 Hz", "70 Hz", "160 Hz", "360 Hz" }, 0));

    layout.add (makeChoice (kLpfFreq, "High Cut",
        { "Off", "6 kHz", "8 kHz", "10 kHz", "14 kHz", "18 kHz" }, 0));

    // -- Levels and routing --------------------------------------------------
    layout.add (makeFloat (kInputGain,   "Input",  -24.0f, 24.0f, 0.01f, 0.0f, dbAttr()));
    layout.add (makeFloat (kOutputLevel, "Output", -24.0f, 24.0f, 0.01f, 0.0f, dbAttr()));

    layout.add (makeBool (kEqIn,  "EQ In",  true));
    layout.add (makeBool (kPhase, "Phase",  false));

    layout.add (makeFloat (kMix, "Mix", 0.0f, 100.0f, 0.1f, 100.0f,
                           juce::AudioParameterFloatAttributes()
                               .withLabel ("%")
                               .withStringFromValueFunction ([] (float v, int)
                               {
                                   return juce::String (juce::roundToInt (v)) + " %";
                               })));

    layout.add (makeBool (kAutoGain, "Auto Gain", false));

    layout.add (makeChoice (kOversampling, "Oversampling",
        { "Off", "2x", "4x", "HQ (8x)" }, 1));

    return layout;
}

} // namespace frostyeq::params
