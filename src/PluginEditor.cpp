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
    constexpr int kBandRow   = 140;   // knob, gap, dotted track, gap, legend
    constexpr int kHiQRow    = 22;
    constexpr int kFilterRow = 92;
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
      eqIn   (p.getApvts(), P::kEqIn,   "EQL"),
      phase  (p.getApvts(), P::kPhase,  phaseGlyph),
      midHiQ (p.getApvts(), P::kMidHiQ, "HI-Q", true),
      meter  ([&p] { return juce::jmax (p.getOutputPeak (0), p.getOutputPeak (1)); },
              [&p] { return juce::jmax (p.getOutputRms  (0), p.getOutputRms  (1)); })
{
    for (auto* c : std::initializer_list<juce::Component*> {
             &presetBar, &inputGain, &high, &mid, &low, &highPass,
             &eqIn, &phase, &midHiQ, &outputLevel, &meter })
        addAndMakeVisible (c);
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
        const auto font = theme::labelFont (14.0f, true);

        const auto width = juce::jmax (44, juce::roundToInt (
            juce::GlyphArrangement::getStringWidth (font, rule.text)) + 16);
        const auto box = juce::Rectangle<int> (0, rule.y, getWidth(), kRuleRow)
                             .withSizeKeepingCentre (width, kRuleRow);

        theme::drawOutlinedText (g, rule.text, box.toFloat(), juce::Justification::centred,
                                 font, p.labelPink);

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

    area.removeFromTop (kHeader);

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
        // Every knob on the panel shares one centre line, output included. The
        // meter is not a knob and does not join it: it goes out to the right
        // margin, where it reads as an indicator beside the strip rather than
        // as something that shoves the output knob off the axis.
        auto bottom = area.removeFromTop (kGainRow);

        meter.setBounds (bottom.withTrimmedTop (4).withTrimmedBottom (22).removeFromRight (44));
        outputLevel.setBounds (bottom);
    }
}

//==============================================================================
FrostyEqAudioProcessorEditor::FrostyEqAudioProcessorEditor (FrostyEqAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), panel (p)
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
}

FrostyEqAudioProcessorEditor::~FrostyEqAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void FrostyEqAudioProcessorEditor::resized()
{
    // One uniform scale, so everything keeps its proportions.
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) kDesignWidth));
}
