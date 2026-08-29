#include "PluginEditor.h"

using namespace frostyeq;
using namespace frostyeq::gui;
namespace P = frostyeq::params;

namespace
{
    constexpr int kHeader      = 30;
    constexpr int kSections    = 206;
    constexpr int kFooter      = 26;
    constexpr int kPad         = 10;
    constexpr int kMeterBlock  = 40;
    constexpr int kSectionCap  = 15;

    constexpr int kGainKnob = 54;   // dominant, as on the hardware
    constexpr int kFreqKnob = 40;
    constexpr int kUtilKnob = 44;

    constexpr int kOneKnob  = 78;

    // The hardware's phase switch is marked with a slashed O.
    const juce::String phaseGlyph = juce::String (juce::CharPointer_UTF8 ("\xc3\x98"));
}

//==============================================================================
FrostyEqAudioProcessorEditor::FrostyEqAudioProcessorEditor (FrostyEqAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processorRef (p),
      curve       (p.getApvts()),
      inputMeter  ("IN",  [&p] { return juce::jmax (p.getInputPeak (0),  p.getInputPeak (1)); }),
      outputMeter ("OUT", [&p] { return juce::jmax (p.getOutputPeak (0), p.getOutputPeak (1)); }),
      hpf         (p.getApvts(), P::kHpfFreq,     "HPF",    false),
      lpf         (p.getApvts(), P::kLpfFreq,     "LPF",    false),
      lfGain      (p.getApvts(), P::kLfGain,      "LOW",    true),
      lfFreq      (p.getApvts(), P::kLfFreq,      "FREQ",   false),
      midGain     (p.getApvts(), P::kMidGain,     "MID",    true),
      midFreq     (p.getApvts(), P::kMidFreq,     "FREQ",   false),
      hfGain      (p.getApvts(), P::kHfGain,      "HIGH",   true),
      hfFreq      (p.getApvts(), P::kHfFreq,      "FREQ",   false),
      inputGain   (p.getApvts(), P::kInputGain,   "INPUT",  true),
      outputLevel (p.getApvts(), P::kOutputLevel, "OUTPUT", true),
      mix         (p.getApvts(), P::kMix,         "MIX",    false),
      eqIn        (p.getApvts(), P::kEqIn,        "EQ IN"),
      phase       (p.getApvts(), P::kPhase,       phaseGlyph),
      midHiQ      (p.getApvts(), P::kMidHiQ,      "HI-Q"),
      autoGain    (p.getApvts(), P::kAutoGain,    "AUTO GAIN")
{
    setLookAndFeel (&lookAndFeel);

    modelChooser.addItemList ({ "1073", "1084" }, 1);
    modelChooser.setColour (juce::ComboBox::backgroundColourId, theme::background);
    modelChooser.setColour (juce::ComboBox::textColourId,       theme::text);
    modelChooser.setColour (juce::ComboBox::outlineColourId,    theme::outline);
    modelChooser.setColour (juce::ComboBox::arrowColourId,      theme::textDim);
    modelChooser.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (modelChooser);

    modelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        p.getApvts(), P::kModel, modelChooser);

    for (auto* k : { &lfGain, &midGain, &hfGain })
        k->setKnobDiameter (kGainKnob);

    for (auto* k : { &lfFreq, &midFreq, &hfFreq })
        k->setKnobDiameter (kFreqKnob);

    for (auto* k : { &hpf, &lpf, &inputGain, &outputLevel, &mix })
        k->setKnobDiameter (kUtilKnob);

    for (auto* c : std::initializer_list<juce::Component*> {
             &curve, &inputMeter, &outputMeter,
             &hpf, &lpf, &lfGain, &lfFreq, &midGain, &midFreq, &hfGain, &hfFreq,
             &inputGain, &outputLevel, &mix,
             &eqIn, &phase, &midHiQ, &autoGain })
        addAndMakeVisible (c);

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio (760.0 / 486.0);
    setResizeLimits (646, 413, 1520, 972);
    setSize (760, 486);

    startTimerHz (10);
    timerCallback();
}

FrostyEqAudioProcessorEditor::~FrostyEqAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void FrostyEqAudioProcessorEditor::timerCallback()
{
    // Controls the 1073 does not have stay present but greyed, so automation
    // survives a model switch.
    curve.setEqSampleRate (processorRef.getEqSampleRate());

    const auto* model = processorRef.getApvts().getRawParameterValue (P::kModel);
    const auto is1084 = model != nullptr && model->load (std::memory_order_relaxed) > 0.5f;

    if (is1084 == lastModelWas1084)
        return;

    lastModelWas1084 = is1084;

    hfFreq.setKnobEnabled   (is1084);   // the 1073's shelf is fixed at 12 kHz
    lpf   .setKnobEnabled   (is1084);   // the low-pass is a 1084 addition
    midHiQ.setSwitchEnabled (is1084);
}

//==============================================================================
void FrostyEqAudioProcessorEditor::drawSection (juce::Graphics& g,
                                                juce::Rectangle<int> bounds,
                                                const juce::String& caption) const
{
    g.setColour (theme::panel);
    g.fillRoundedRectangle (bounds.toFloat(), 4.0f);

    g.setColour (theme::outline);
    g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 4.0f, 1.0f);

    g.setColour (theme::textDim);
    g.setFont (theme::labelFont (9.5f));
    g.drawText (caption, bounds.removeFromTop (kSectionCap).reduced (8, 2),
                juce::Justification::centredLeft, false);
}

void FrostyEqAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);

    auto header = getLocalBounds().removeFromTop (kHeader);

    g.setColour (theme::panel);
    g.fillRect (header);
    g.setColour (theme::outline);
    g.drawHorizontalLine (header.getBottom() - 1, 0.0f, (float) getWidth());

    g.setColour (theme::text);
    g.setFont (theme::labelFont (14.0f));
    g.drawText ("FrostyEQ", header.reduced (kPad, 0), juce::Justification::centredLeft, false);

    g.setColour (theme::textDim);
    g.setFont (theme::labelFont (10.0f));
    g.drawText ("LT3 AUDIO", header.reduced (kPad, 0), juce::Justification::centredRight, false);

    drawSection (g, filterSection, "FILTERS");
    drawSection (g, eqSection,     "EQUALISER");
    drawSection (g, levelSection,  "LEVEL");
}

//==============================================================================
void FrostyEqAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    {
        auto header = area.removeFromTop (kHeader);
        modelChooser.setBounds (header.withSizeKeepingCentre (92, 19));
    }

    area.reduce (kPad, kPad);

    // Curve takes whatever the fixed-height rows below do not need.
    {
        auto top = area.removeFromTop (area.getHeight() - kSections - kFooter - kPad - 6);
        auto meters = top.removeFromRight (kMeterBlock);
        top.removeFromRight (6);

        meters = meters.withTrimmedBottom (4);
        const auto barWidth = (meters.getWidth() - 6) / 2;
        inputMeter .setBounds (meters.removeFromLeft (barWidth));
        meters.removeFromLeft (6);
        outputMeter.setBounds (meters.removeFromLeft (barWidth));

        curve.setBounds (top);
    }

    area.removeFromTop (kPad);

    // Footer switches.
    {
        auto footer = area.removeFromBottom (kFooter);

        for (auto& s : std::initializer_list<std::pair<SwitchButton*, int>> {
                 { &eqIn, 56 }, { &phase, 34 } })
        {
            s.first->setBounds (footer.removeFromLeft (s.second).withTrimmedTop (4));
            footer.removeFromLeft (5);
        }
    }

    // Three sections: filters, the equaliser proper, then level. The order
    // reads left to right as signal flow, and low to high within the EQ.
    filterSection = area.removeFromLeft (kOneKnob + 24);
    area.removeFromLeft (kPad);

    levelSection = area.removeFromRight (3 * kOneKnob + 16);
    area.removeFromRight (kPad);

    eqSection = area;

    // Lay each control out at its natural height, top-aligned, rather than
    // letting it stretch down the section.
    const auto place = [] (LabelledKnob& knob, juce::Rectangle<int> cell)
    {
        knob.setBounds (cell.removeFromTop (knob.getPreferredHeight()));
    };

    const auto content = [] (juce::Rectangle<int> section)
    {
        section.removeFromTop (kSectionCap + 2);
        return section.reduced (8, 4);
    };

    // Filters stacked, as they sit in the hardware's vertical control chain.
    {
        auto r = content (filterSection);
        place (hpf, r.removeFromTop (hpf.getPreferredHeight()));
        r.removeFromTop (6);
        place (lpf, r);
    }

    {
        auto r = content (eqSection);
        const auto w = r.getWidth() / 3;

        for (auto& band : std::initializer_list<std::pair<LabelledKnob*, LabelledKnob*>> {
                 { &lfGain, &lfFreq }, { &midGain, &midFreq }, { &hfGain, &hfFreq } })
        {
            auto column = r.removeFromLeft (w);
            place (*band.first,  column.removeFromTop (band.first->getPreferredHeight()));
            place (*band.second, column.removeFromTop (band.second->getPreferredHeight()));

            // The Hi-Q switch is a mid-band control on the hardware, so it
            // lives with the mid band rather than in a row of global switches.
            if (band.first == &midGain)
                midHiQ.setBounds (column.removeFromTop (22).reduced (14, 2));
        }
    }

    {
        auto r = content (levelSection);
        const auto w = r.getWidth() / 3;
        place (inputGain,   r.removeFromLeft (w));
        place (outputLevel, r.removeFromLeft (w));
        place (mix,         r.removeFromLeft (w));

        // Auto-gain compensates the level stage, so it belongs here.
        autoGain.setBounds (levelSection.withTrimmedTop (kSectionCap + 2 + kOneKnob + 14)
                                        .withHeight (22)
                                        .withSizeKeepingCentre (96, 22));
    }
}
