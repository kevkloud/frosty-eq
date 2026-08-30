#include "Controls.h"
#include "params/ParameterLayout.h"

namespace frostyeq::gui
{

namespace
{
    /** "1.6 kHz" -> "1k6", "360 Hz" -> "360", "Off" -> "Off". The legend has to
        fit around a knob, and this is how the hardware prints it. */
    juce::String compactFrequency (const juce::String& text)
    {
        if (text.containsIgnoreCase ("kHz"))
        {
            const auto number = text.upToFirstOccurrenceOf (" ", false, true).trim();

            if (number.contains ("."))
                return number.upToFirstOccurrenceOf (".", false, false) + "k"
                     + number.fromFirstOccurrenceOf (".", false, false);

            return number + "k";
        }

        if (text.containsIgnoreCase ("Hz"))
            return text.upToFirstOccurrenceOf (" ", false, true).trim();

        return text;
    }
}

//==============================================================================
PlainKnob::PlainKnob (juce::AudioProcessorValueTreeState& state,
                      const juce::String& parameterId,
                      const juce::String& captionText,
                      bool polarityMarks)
    : caption (captionText)
{
    knob.setPolarityMarks (polarityMarks);
    knob.setFaceScale (0.62f);
    addAndMakeVisible (knob);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, parameterId, knob);
}

void PlainKnob::paint (juce::Graphics& g)
{
    g.setColour (knob.isEnabled() ? theme::palette().text : theme::palette().textDim.withAlpha (0.5f));
    g.setFont (theme::labelFont (10.5f));
    g.drawText (caption, getLocalBounds().removeFromTop (14), juce::Justification::centred, false);
}

void PlainKnob::resized()
{
    knob.setBounds (getLocalBounds().withTrimmedTop (15));
}

void PlainKnob::setKnobEnabled (bool shouldBeEnabled)
{
    knob.setEnabled (shouldBeEnabled);
    repaint();
}

//==============================================================================
ConcentricBand::ConcentricBand (juce::AudioProcessorValueTreeState& s,
                                const juce::String& frequencyParameterId,
                                const juce::String& gainParameterId,
                                const juce::String& captionText)
    : caption (captionText), state (s)
{
    frequency = state.getParameter (frequencyParameterId);
    jassert (frequency != nullptr);

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (frequency))
        ring.setDetents (choice->choices.size());

    ring.setSliderSnapsToMousePosition (false);
    ring.setFaceScale (0.60f);
    addAndMakeVisible (ring);

    ringAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, frequencyParameterId, ring);

    if (gainParameterId.isNotEmpty())
    {
        // Added second, so it sits above the ring and takes the mouse first.
        centre.setPolarityMarks (true);
        centre.setFaceScale (1.0f);
        centre.setCircularHitTest (true);
        addAndMakeVisible (centre);

        centreAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, gainParameterId, centre);
    }
    else
    {
        // A plain ring: make its own face the visible knob.
        ring.setFaceScale (0.44f);
    }

    startTimerHz (8);
    timerCallback();
}

void ConcentricBand::timerCallback()
{
    // The high-pass legend depends on which module is selected, so rebuild it
    // when that changes rather than caching it once.
    const auto* model = state.getRawParameterValue (params::kModel);
    const auto current = model != nullptr ? (int) model->load (std::memory_order_relaxed) : 0;

    if (current == lastLegendModel && ! legend.isEmpty())
        return;

    lastLegendModel = current;
    legend.clear();

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (frequency))
        for (int i = 0; i < choice->choices.size(); ++i)
            legend.add (compactFrequency (
                frequency->getText (frequency->convertTo0to1 ((float) i), 0)));

    repaint();
}

void ConcentricBand::setRingEnabled (bool shouldBeEnabled)
{
    ringEnabled = shouldBeEnabled;
    ring.setEnabled (shouldBeEnabled);
    repaint();
}

void ConcentricBand::setCentreEnabled (bool shouldBeEnabled)
{
    centre.setEnabled (shouldBeEnabled);
    repaint();
}

juce::Rectangle<int> ConcentricBand::getCaptionArea() const
{
    return getLocalBounds().removeFromTop (15);
}

void ConcentricBand::paint (juce::Graphics& g)
{
    const auto& p = theme::palette();

    g.setColour (p.text);
    g.setFont (theme::labelFont (10.5f));
    g.drawText (caption, getCaptionArea(), juce::Justification::centred, false);

    if (legend.isEmpty())
        return;

    // Frequency legend, printed around the ring at each detent.
    const auto area = getLocalBounds().withTrimmedTop (15).toFloat();
    const auto centrePoint = area.getCentre();
    const auto ringRadius = (float) juce::jmin (area.getWidth(), area.getHeight()) * 0.5f * ring.getFaceScale();
    const auto textRadius = ringRadius + 15.0f;

    const auto startAngle = ring.getRotaryParameters().startAngleRadians;
    const auto endAngle   = ring.getRotaryParameters().endAngleRadians;

    const auto selected = juce::roundToInt (ring.getValue());

    for (int i = 0; i < legend.size(); ++i)
    {
        const auto t = legend.size() > 1 ? (float) i / (float) (legend.size() - 1) : 0.0f;
        const auto a = startAngle + t * (endAngle - startAngle);

        const juce::Point<float> at { centrePoint.x + textRadius * std::sin (a),
                                      centrePoint.y - textRadius * std::cos (a) };

        const auto isSelected = (i == selected);

        g.setColour (! ringEnabled ? p.textDim.withAlpha (0.35f)
                                   : (isSelected ? p.text : p.textDim));
        g.setFont (theme::labelFont (isSelected ? 10.0f : 9.0f));
        g.drawText (legend[i], juce::Rectangle<float> (30.0f, 12.0f).withCentre (at),
                    juce::Justification::centred, false);
    }
}

void ConcentricBand::resized()
{
    auto area = getLocalBounds().withTrimmedTop (15);

    // The ring fills the cell; the centre control is the inner disc.
    ring.setBounds (area);

    if (centreAttachment != nullptr)
    {
        const auto ringRadius = (float) juce::jmin (area.getWidth(), area.getHeight()) * 0.5f * ring.getFaceScale();
        const auto inner = juce::roundToInt (ringRadius * 1.12f);
        centre.setBounds (area.withSizeKeepingCentre (inner, inner));
    }
}

//==============================================================================
SwitchButton::SwitchButton (juce::AudioProcessorValueTreeState& state,
                            const juce::String& parameterId,
                            const juce::String& text)
{
    button.setButtonText (text);
    addAndMakeVisible (button);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, parameterId, button);
}

void SwitchButton::resized()             { button.setBounds (getLocalBounds()); }
void SwitchButton::setSwitchEnabled (bool e) { button.setEnabled (e); }

//==============================================================================
OutputMeter::OutputMeter (std::function<float()> peakSource, std::function<float()> rmsSource)
    : peak (std::move (peakSource)), rms (std::move (rmsSource))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    startTimerHz (30);
}

void OutputMeter::mouseUp (const juce::MouseEvent&)
{
    vuMode = ! vuMode;
    displayed = 0.0f;
    repaint();
}

void OutputMeter::timerCallback()
{
    const auto level = vuMode ? (rms ? rms() : 0.0f) : (peak ? peak() : 0.0f);

    // A VU meter integrates; a peak meter jumps and falls back slowly.
    const auto rise = vuMode ? 0.28f : 1.0f;
    const auto fall = vuMode ? 0.28f : 0.16f;

    displayed += (level > displayed ? rise : fall) * (level - displayed);
    repaint();
}

void OutputMeter::paint (juce::Graphics& g)
{
    const auto& p = theme::palette();

    auto bounds = getLocalBounds();

    // The label needs more room than the bar does, so the component is wider
    // than the well and the well is centred inside it.
    const auto labelArea = bounds.removeFromBottom (12);
    const auto well = bounds.withSizeKeepingCentre (kBarWidth, bounds.getHeight()).toFloat();

    g.setColour (p.meterWell);
    g.fillRoundedRectangle (well, 2.0f);

    const auto db = juce::Decibels::gainToDecibels (displayed, -70.0f);

    // dBFS runs -60 to 0 and is about headroom; VU runs -20 to +3 about a
    // -18 dBFS reference and is about weight.
    const auto lo = vuMode ? -20.0f : -60.0f;
    const auto hi = vuMode ? 3.0f : 0.0f;
    const auto reading = vuMode ? db - kVuReference : db;
    const auto norm = juce::jlimit (0.0f, 1.0f, (reading - lo) / (hi - lo));

    if (norm > 0.002f)
    {
        auto bar = well.reduced (1.5f);
        bar = bar.removeFromBottom (bar.getHeight() * norm);

        const auto hot = vuMode ? reading > 0.0f : reading > -1.0f;
        const auto warm = vuMode ? reading > -3.0f : reading > -9.0f;

        g.setColour (hot ? p.meterClip : warm ? p.meterHigh : p.meterLow);
        g.fillRoundedRectangle (bar, 1.5f);
    }

    g.setColour (p.outline);
    g.drawRoundedRectangle (well.reduced (0.5f), 2.0f, 1.0f);

    g.setColour (p.textDim);
    g.setFont (theme::labelFont (9.0f));
    g.drawText (vuMode ? "VU" : "dBFS", labelArea.toFloat(),
                juce::Justification::centred, false);
}

} // namespace frostyeq::gui
