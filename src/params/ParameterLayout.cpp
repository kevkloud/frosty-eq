#include "ParameterLayout.h"

namespace frostyeq::params
{

namespace
{
    juce::AudioParameterFloatAttributes dbAttr()
    {
        return juce::AudioParameterFloatAttributes().withLabel ("dB");
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
PositionalChoice::PositionalChoice (juce::ParameterID pid,
                                    const juce::String& paramName,
                                    juce::StringArray labels1073,
                                    juce::StringArray labels1084,
                                    int defaultIndex)
    // The 1073 labels are the declared choice list, so a host that caches
    // getAllValueStrings() still shows something sensible.
    : juce::AudioParameterChoice (std::move (pid), paramName, labels1073, defaultIndex),
      labelsA (std::move (labels1073)),
      labelsB (std::move (labels1084))
{
    jassert (labelsA.size() == labelsB.size());
}

juce::String PositionalChoice::getText (float normalisedValue, int maximumStringLength) const
{
    const auto& labels = (model.load (std::memory_order_relaxed) == (int) Model::m1084) ? labelsB : labelsA;
    const auto index   = juce::jlimit (0, labels.size() - 1,
                                       juce::roundToInt (getNormalisableRange().convertFrom0to1 (normalisedValue)));

    return maximumStringLength > 0 ? labels[index].substring (0, maximumStringLength)
                                   : labels[index];
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout create()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // -- Model ---------------------------------------------------------------
    layout.add (makeChoice (kModel, "Model", { "1073", "1084" }, 0));

    // -- High shelf ----------------------------------------------------------
    // Only the 1084 can select a shelf frequency; the 1073's is fixed at 12 kHz.
    // The control still exists in 1073 mode so automation survives a model
    // switch, but the DSP clamps it to 12 kHz and the editor greys it out.
    // (An earlier version relabelled all three positions "12 kHz" in 1073 mode,
    // which just looked like a broken menu.)
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

    // -- Filters -------------------------------------------------------------
    // Positional, because the two units put different frequencies on the same
    // detents. See PositionalChoice.
    layout.add (std::make_unique<PositionalChoice> (
        juce::ParameterID { kHpfFreq, kVersionHint }, "HPF",
        juce::StringArray { "Off", "50 Hz", "80 Hz", "160 Hz", "300 Hz" },
        juce::StringArray { "Off", "45 Hz", "70 Hz", "160 Hz", "360 Hz" }, 0));

    layout.add (makeChoice (kLpfFreq, "LPF",
        { "Off", "6 kHz", "8 kHz", "10 kHz", "14 kHz", "18 kHz" }, 0));  // 1084 only

    // -- Levels and routing --------------------------------------------------
    layout.add (makeFloat (kInputGain,   "Input",  -24.0f, 24.0f, 0.01f, 0.0f, dbAttr()));
    layout.add (makeFloat (kOutputLevel, "Output", -24.0f, 24.0f, 0.01f, 0.0f, dbAttr()));

    layout.add (makeBool (kEqIn,  "EQ In",  true));
    layout.add (makeBool (kPhase, "Phase",  false));

    layout.add (makeFloat (kMix, "Mix", 0.0f, 100.0f, 0.1f, 100.0f,
                           juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (makeBool (kAutoGain, "Auto Gain", false));

    layout.add (makeChoice (kOversampling, "Oversampling",
        { "Off", "2x", "4x", "HQ (8x)" }, 1));

    return layout;
}

} // namespace frostyeq::params
