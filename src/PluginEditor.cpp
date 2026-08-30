#include "PluginEditor.h"

using namespace frostyeq;
using namespace frostyeq::gui;
namespace P = frostyeq::params;

namespace
{
    constexpr int kWidth  = 360;
    constexpr int kHeight = 792;

    constexpr int kHeader = 34;
    constexpr int kPad    = 12;
    constexpr int kGainRow = 88;
    constexpr int kBandRow = 132;
    constexpr int kSwitchRow = 26;

    const juce::String phaseGlyph = juce::String (juce::CharPointer_UTF8 ("\xc3\x98"));
}

//==============================================================================
FrostyEqAudioProcessorEditor::FrostyEqAudioProcessorEditor (FrostyEqAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processorRef (p),
      inputGain   (p.getApvts(), P::kInputGain,   "INPUT",  true),
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
    const auto* model = p.getApvts().getRawParameterValue (P::kModel);
    theme::setModel (model != nullptr && model->load() > 0.5f ? Model::m1084 : Model::m1073);
    lookAndFeel.refreshColours();
    setLookAndFeel (&lookAndFeel);

    modelChooser.addItemList ({ "1073", "1084" }, 1);
    modelChooser.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (modelChooser);

    modelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        p.getApvts(), P::kModel, modelChooser);

    for (auto* c : std::initializer_list<juce::Component*> {
             &inputGain, &high, &mid, &low, &highPass, &lowPass,
             &eqIn, &phase, &midHiQ, &outputLevel, &meter })
        addAndMakeVisible (c);

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) kWidth / (double) kHeight);
    setResizeLimits (kWidth * 3 / 4, kHeight * 3 / 4, kWidth * 2, kHeight * 2);
    setSize (kWidth, kHeight);

    startTimerHz (10);
    timerCallback();
}

FrostyEqAudioProcessorEditor::~FrostyEqAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void FrostyEqAudioProcessorEditor::applyModel (bool is1084)
{
    theme::setModel (is1084 ? Model::m1084 : Model::m1073);
    lookAndFeel.refreshColours();

    // Controls the 1073 does not have stay on the panel but go dead, so
    // automation survives a model switch.
    high    .setRingEnabled (is1084);      // the 1073's shelf is fixed at 12 kHz
    lowPass .setRingEnabled (is1084);      // the low-pass is a 1084 addition
    midHiQ  .setSwitchEnabled (is1084);

    // ComboBox copies its text colour into an internal label, and only does so
    // when the look-and-feel object changes -- swapping the palette on the same
    // object leaves it painting in the previous scheme's colour. This forces
    // the whole tree to re-read.
    sendLookAndFeelChange();
    repaint();
}

void FrostyEqAudioProcessorEditor::timerCallback()
{
    const auto* model = processorRef.getApvts().getRawParameterValue (P::kModel);
    const auto is1084 = model != nullptr && model->load (std::memory_order_relaxed) > 0.5f;

    // Tri-state: a bool initialised to false would match the default model and
    // skip the first update, leaving the 1084-only controls looking live.
    if (appliedModel == (int) is1084)
        return;

    appliedModel = (int) is1084;
    applyModel (is1084);
}

//==============================================================================
void FrostyEqAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto& p = theme::palette();
    g.fillAll (p.background);

    auto header = getLocalBounds().removeFromTop (kHeader);
    g.setColour (p.panel);
    g.fillRect (header);
    g.setColour (p.outline);
    g.drawHorizontalLine (header.getBottom() - 1, 0.0f, (float) getWidth());

    g.setColour (p.text);
    g.setFont (theme::labelFont (13.0f));
    g.drawText ("FrostyEQ", header.reduced (kPad, 0), juce::Justification::centredLeft, false);

    // Hairlines between sections, in place of boxes.
    g.setColour (p.outline.withAlpha (0.5f));
    for (auto y : dividers)
        g.drawHorizontalLine (y, (float) kPad, (float) (getWidth() - kPad));
}

void FrostyEqAudioProcessorEditor::resized()
{
    dividers.clear();

    auto area = getLocalBounds();

    modelChooser.setBounds (area.removeFromTop (kHeader)
                                .removeFromRight (110).reduced (kPad, 8));

    area.reduce (kPad, kPad / 2);

    inputGain.setBounds (area.removeFromTop (kGainRow));
    dividers.push_back (area.getY());

    // Each band: frequency legended around the ring, cut and boost inside.
    high.setBounds (area.removeFromTop (kBandRow));
    mid .setBounds (area.removeFromTop (kBandRow));

    // Hi-Q belongs to the mid band, so it sits on that row rather than in a
    // strip of unrelated switches.
    midHiQ.setBounds (mid.getBounds().removeFromTop (22).removeFromRight (58).translated (-6, 2));

    low.setBounds (area.removeFromTop (kBandRow));
    dividers.push_back (area.getY());

    {
        auto filters = area.removeFromTop (kBandRow);
        const auto half = filters.getWidth() / 2;
        highPass.setBounds (filters.removeFromLeft (half));
        lowPass .setBounds (filters);
    }

    dividers.push_back (area.getY());

    {
        auto switches = area.removeFromTop (kSwitchRow + 6).withTrimmedTop (6);
        const auto w = 62;
        eqIn .setBounds (switches.removeFromLeft (w));
        switches.removeFromLeft (8);
        phase.setBounds (switches.removeFromLeft (40));
    }

    {
        auto bottom = area.removeFromTop (kGainRow + 6).withTrimmedTop (6);

        // Clear of the resize corner, and no taller than the knob beside it.
        auto meterArea = bottom.removeFromRight (52).withTrimmedRight (18);
        meter.setBounds (meterArea.withSizeKeepingCentre (18, 66).translated (0, 4));

        outputLevel.setBounds (bottom);
    }
}
