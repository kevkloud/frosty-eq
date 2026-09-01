#include "PluginEditor.h"

using namespace frostyeq;
using namespace frostyeq::gui;
namespace P = frostyeq::params;

namespace
{
    constexpr int kHeader    = 28;
    constexpr int kPresetRow = 24;
    constexpr int kPad       = 10;

    constexpr int kGainRow   = 100;   // knob plus the name under it
    constexpr int kRuleRow   = 18;
    constexpr int kBandRow   = 118;
    constexpr int kHiQRow    = 22;
    constexpr int kFilterRow = 80;
    constexpr int kSwitchRow = 28;

    const juce::String phaseGlyph = juce::String (juce::CharPointer_UTF8 ("\xc3\x98"));
}

//==============================================================================
FrostyEqAudioProcessorEditor::Panel::Panel (FrostyEqAudioProcessor& p)
    : presetBar   (p.getPresets()),
      inputGain   (p.getApvts(), P::kInputGain,   "INPUT"),
      outputLevel (p.getApvts(), P::kOutputLevel, "OUTPUT"),
      high     (p.getApvts(), P::kHfFreq,  P::kHfGain),
      mid      (p.getApvts(), P::kMidFreq, P::kMidGain),
      low      (p.getApvts(), P::kLfFreq,  P::kLfGain),
      highPass (p.getApvts(), P::kHpfFreq, {}),
      lowPass  (p.getApvts(), P::kLpfFreq, {}),
      eqIn   (p.getApvts(), P::kEqIn,   "EQL"),
      phase  (p.getApvts(), P::kPhase,  phaseGlyph),
      midHiQ (p.getApvts(), P::kMidHiQ, "HI-Q", true),
      meter  ([&p] { return juce::jmax (p.getOutputPeak (0), p.getOutputPeak (1)); },
              [&p] { return juce::jmax (p.getOutputRms  (0), p.getOutputRms  (1)); })
{
    modelChooser.addItemList ({ "1073", "1084" }, 1);
    modelChooser.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (modelChooser);

    modelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        p.getApvts(), P::kModel, modelChooser);

    for (auto* c : std::initializer_list<juce::Component*> {
             &presetBar, &inputGain, &high, &mid, &low, &highPass, &lowPass,
             &eqIn, &phase, &midHiQ, &outputLevel, &meter })
        addAndMakeVisible (c);
}

void FrostyEqAudioProcessorEditor::Panel::applyModel (bool is1084)
{
    // Controls the 1073 does not have stay on the panel but go dead, so
    // automation survives a model switch.
    high   .setRingEnabled (is1084);   // the 1073's shelf is fixed at 12 kHz
    lowPass.setRingEnabled (is1084);   // the cut filter above is a 1084 addition
    midHiQ .setSwitchEnabled (is1084);

    sendLookAndFeelChange();
    repaint();
}

//==============================================================================
void FrostyEqAudioProcessorEditor::Panel::paint (juce::Graphics& g)
{
    const auto& p = theme::palette();
    g.fillAll (p.background);

    auto header = getLocalBounds().removeFromTop (kHeader);
    g.setColour (p.panel);
    g.fillRect (header);

    g.setColour (p.text);
    g.setFont (theme::labelFont (12.5f, true));
    g.drawText ("FrostyEQ", header.reduced (kPad, 0), juce::Justification::centredLeft, false);

    // Section legends, with a hairline running out to either side.
    for (const auto& rule : rules)
    {
        g.setColour (p.pink);
        g.setFont (theme::labelFont (14.0f, true));

        const auto width = juce::jmax (44, juce::roundToInt (
            juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), rule.text)) + 16);
        const auto box = juce::Rectangle<int> (0, rule.y, getWidth(), kRuleRow)
                             .withSizeKeepingCentre (width, kRuleRow);

        g.drawText (rule.text, box, juce::Justification::centred, false);

        if (! rule.lines)
            continue;

        const auto y = (float) (rule.y + kRuleRow / 2);
        const auto dashes = std::array<float, 2> { 2.0f, 3.0f };

        g.setColour (p.hairline);
        g.drawDashedLine ({ (float) kPad, y, (float) box.getX() - 6.0f, y },
                          dashes.data(), 2, 1.0f);
        g.drawDashedLine ({ (float) box.getRight() + 6.0f, y, (float) (getWidth() - kPad), y },
                          dashes.data(), 2, 1.0f);
    }
}

void FrostyEqAudioProcessorEditor::Panel::resized()
{
    rules.clear();
    auto area = getLocalBounds();

    modelChooser.setBounds (area.removeFromTop (kHeader)
                                .removeFromRight (88).reduced (kPad, 5));

    presetBar.setBounds (area.removeFromTop (kPresetRow));
    area.reduce (kPad, 4);

    const auto rule = [&] (const juce::String& text, bool lines)
    {
        rules.push_back ({ area.removeFromTop (kRuleRow).getY(), text, lines });
    };

    inputGain.setBounds (area.removeFromTop (kGainRow));

    rule ("HIGH", true);
    high.setBounds (area.removeFromTop (kBandRow));

    // Hi-Q belongs to the mid band, so it sits with it rather than in a row of
    // unrelated switches.
    midHiQ.setBounds (area.removeFromTop (kHiQRow).withSizeKeepingCentre (58, 20));

    rule ("MID", false);
    mid.setBounds (area.removeFromTop (kBandRow));

    rule ("LOW", true);
    low.setBounds (area.removeFromTop (kBandRow));

    rule ("LO-CUT", true);
    highPass.setBounds (area.removeFromTop (kFilterRow));

    rule ("HI-CUT", true);
    lowPass.setBounds (area.removeFromTop (kFilterRow));

    rule ({}, true);

    {
        auto switches = area.removeFromTop (kSwitchRow).withTrimmedTop (2);
        constexpr int eqlWidth = 56, phaseWidth = 44, gap = 8;

        auto group = switches.withSizeKeepingCentre (eqlWidth + gap + phaseWidth,
                                                     switches.getHeight());
        eqIn .setBounds (group.removeFromLeft (eqlWidth));
        group.removeFromLeft (gap);
        phase.setBounds (group);
    }

    {
        // Knob and meter centred together, so the meter does not push the panel
        // wide from the edge.
        auto bottom = area.removeFromTop (kGainRow);

        constexpr int knobWidth = 130, meterWidth = 40, gap = 4;
        auto group = bottom.withSizeKeepingCentre (knobWidth + gap + meterWidth,
                                                   bottom.getHeight());

        outputLevel.setBounds (group.removeFromLeft (knobWidth));
        group.removeFromLeft (gap);
        meter.setBounds (group.withSizeKeepingCentre (meterWidth, 52));
    }
}

//==============================================================================
FrostyEqAudioProcessorEditor::FrostyEqAudioProcessorEditor (FrostyEqAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p), panel (p)
{
    lookAndFeel.refreshColours();
    setLookAndFeel (&lookAndFeel);

    // The panel is always this size; the editor scales it.
    panel.setBounds (0, 0, kDesignWidth, kDesignHeight);
    addAndMakeVisible (panel);

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) kDesignWidth / (double) kDesignHeight);
    setResizeLimits (kDesignWidth * 2 / 3, kDesignHeight * 2 / 3,
                     kDesignWidth * 2,     kDesignHeight * 2);
    setSize (kDesignWidth, kDesignHeight);

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
    const auto* model = processorRef.getApvts().getRawParameterValue (P::kModel);
    const auto is1084 = model != nullptr && model->load (std::memory_order_relaxed) > 0.5f;

    // Tri-state: a bool initialised to false would match the default model and
    // skip the first update, leaving the 1084-only controls looking live.
    if (appliedModel == (int) is1084)
        return;

    appliedModel = (int) is1084;
    panel.applyModel (is1084);
}

void FrostyEqAudioProcessorEditor::resized()
{
    // One uniform scale, so everything keeps its proportions.
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) kDesignWidth));
}
