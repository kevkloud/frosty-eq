#include "PluginEditor.h"

using namespace frostyeq;
using namespace frostyeq::gui;
namespace P = frostyeq::params;

namespace
{
    // Row heights at design size. Sized so the whole strip sits in roughly the
    // footprint of a comparable module plugin rather than towering over it.
    constexpr int kHeader    = 30;
    constexpr int kPad       = 10;
    constexpr int kGainRow   = 68;
    constexpr int kBandRow   = 104;
    constexpr int kHiQRow    = 22;
    constexpr int kFilterRow = 96;
    constexpr int kSwitchRow = 32;
    constexpr int kOutputRow = 74;

    const juce::String phaseGlyph = juce::String (juce::CharPointer_UTF8 ("\xc3\x98"));
}

//==============================================================================
FrostyEqAudioProcessorEditor::Panel::Panel (FrostyEqAudioProcessor& p)
    : inputGain   (p.getApvts(), P::kInputGain,   "INPUT",  true),
      outputLevel (p.getApvts(), P::kOutputLevel, "OUTPUT", true),
      high     (p.getApvts(), P::kHfFreq,  P::kHfGain,  "HIGH"),
      mid      (p.getApvts(), P::kMidFreq, P::kMidGain, "MID"),
      low      (p.getApvts(), P::kLfFreq,  P::kLfGain,  "LOW"),
      highPass (p.getApvts(), P::kHpfFreq, {},          "HIGH PASS"),
      lowPass  (p.getApvts(), P::kLpfFreq, {},          "LOW PASS"),
      eqIn   (p.getApvts(), P::kEqIn,   "EQL"),
      phase  (p.getApvts(), P::kPhase,  phaseGlyph),
      midHiQ (p.getApvts(), P::kMidHiQ, "HI-Q"),
      meter  ([&p] { return juce::jmax (p.getOutputPeak (0), p.getOutputPeak (1)); },
              [&p] { return juce::jmax (p.getOutputRms  (0), p.getOutputRms  (1)); })
{
    modelChooser.addItemList ({ "1073", "1084" }, 1);
    modelChooser.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (modelChooser);

    modelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        p.getApvts(), P::kModel, modelChooser);

    for (auto* c : std::initializer_list<juce::Component*> {
             &inputGain, &high, &mid, &low, &highPass, &lowPass,
             &eqIn, &phase, &midHiQ, &outputLevel, &meter })
        addAndMakeVisible (c);
}

void FrostyEqAudioProcessorEditor::Panel::applyModel (bool is1084)
{
    // Controls the 1073 does not have stay on the panel but go dead, so
    // automation survives a model switch.
    high   .setRingEnabled (is1084);      // the 1073's shelf is fixed at 12 kHz
    lowPass.setRingEnabled (is1084);      // the low-pass is a 1084 addition
    midHiQ .setSwitchEnabled (is1084);

    // ComboBox copies its text colour into an internal label and only re-reads
    // it when the look-and-feel object changes, so swapping the palette on the
    // same object would leave the chooser painting in the previous scheme.
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
    g.setColour (p.outline);
    g.drawHorizontalLine (header.getBottom() - 1, 0.0f, (float) getWidth());

    g.setColour (p.text);
    g.setFont (theme::labelFont (12.5f));
    g.drawText ("FrostyEQ", header.reduced (kPad, 0), juce::Justification::centredLeft, false);

    // Hairlines between sections, in place of boxes.
    g.setColour (p.outline.withAlpha (0.5f));
    for (auto y : dividers)
        g.drawHorizontalLine (y, (float) kPad, (float) (getWidth() - kPad));
}

void FrostyEqAudioProcessorEditor::Panel::resized()
{
    dividers.clear();

    auto area = getLocalBounds();

    modelChooser.setBounds (area.removeFromTop (kHeader)
                                .removeFromRight (92).reduced (kPad, 6));

    area.reduce (kPad, kPad / 2);

    inputGain.setBounds (area.removeFromTop (kGainRow));
    dividers.push_back (area.getY());

    high.setBounds (area.removeFromTop (kBandRow));

    // Hi-Q belongs to the mid band. The module puts it between the high and mid
    // knobs, centred, so it goes there rather than floating to one side.
    midHiQ.setBounds (area.removeFromTop (kHiQRow).withSizeKeepingCentre (52, 19));

    mid.setBounds (area.removeFromTop (kBandRow));
    low.setBounds (area.removeFromTop (kBandRow));
    dividers.push_back (area.getY());

    highPass.setBounds (area.removeFromTop (kFilterRow));
    lowPass .setBounds (area.removeFromTop (kFilterRow));
    dividers.push_back (area.getY());

    {
        auto switches = area.removeFromTop (kSwitchRow).withTrimmedTop (5);
        constexpr int eqlWidth = 52, phaseWidth = 32, gap = 8;

        auto group = switches.withSizeKeepingCentre (eqlWidth + gap + phaseWidth,
                                                     switches.getHeight());
        eqIn .setBounds (group.removeFromLeft (eqlWidth));
        group.removeFromLeft (gap);
        phase.setBounds (group);
    }

    {
        // Knob and meter centred together, so the meter does not push the panel
        // wide from the edge.
        auto bottom = area.removeFromTop (kOutputRow).withTrimmedTop (4);

        constexpr int knobWidth = 84, meterWidth = 44, gap = 4;
        auto group = bottom.withSizeKeepingCentre (knobWidth + gap + meterWidth,
                                                   bottom.getHeight());

        outputLevel.setBounds (group.removeFromLeft (knobWidth));
        group.removeFromLeft (gap);
        meter.setBounds (group.withSizeKeepingCentre (meterWidth, 54).translated (0, 4));
    }
}

//==============================================================================
FrostyEqAudioProcessorEditor::FrostyEqAudioProcessorEditor (FrostyEqAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p), panel (p)
{
    const auto* model = p.getApvts().getRawParameterValue (P::kModel);
    theme::setModel (model != nullptr && model->load() > 0.5f ? Model::m1084 : Model::m1073);
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

    theme::setModel (is1084 ? Model::m1084 : Model::m1073);
    lookAndFeel.refreshColours();
    panel.applyModel (is1084);
    repaint();
}

void FrostyEqAudioProcessorEditor::resized()
{
    // One uniform scale, so everything keeps its proportions.
    const auto scale = (float) getWidth() / (float) kDesignWidth;
    panel.setTransform (juce::AffineTransform::scale (scale));
}
