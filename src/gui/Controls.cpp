#include "Controls.h"
#include "params/ParameterLayout.h"

namespace frostyeq::gui
{

namespace
{
    /** "1.6 kHz" -> "1k6", "360 Hz" -> "360", "Off" -> "OFF". The legend has to
        fit around a knob, and this is how the hardware prints it. */
    juce::String compactFrequency (const juce::String& text)
    {
        if (text.equalsIgnoreCase ("off"))
            return "OFF";

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
                      const juce::String& captionText)
    : caption (captionText)
{
    knob.setStyle (Knob::Style::utility);
    knob.setFaceScale (0.62f);
    addAndMakeVisible (knob);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, parameterId, knob);
}

void PlainKnob::paint (juce::Graphics& g)
{
    const auto& p = theme::palette();

    // The name sits under the knob, as in the design.
    g.setColour (knob.isEnabled() ? p.blue : p.blue.withAlpha (0.4f));
    g.setFont (theme::labelFont (14.0f, true));
    g.drawText (caption, getLocalBounds().removeFromBottom (18),
                juce::Justification::centred, false);
}

void PlainKnob::resized()
{
    knob.setBounds (getLocalBounds().withTrimmedBottom (18));
}

void PlainKnob::setKnobEnabled (bool shouldBeEnabled)
{
    knob.setEnabled (shouldBeEnabled);
    repaint();
}

//==============================================================================
ConcentricBand::ConcentricBand (juce::AudioProcessorValueTreeState& s,
                                const juce::String& frequencyParameterId,
                                const juce::String& gainParameterId)
    : state (s), hasCentre (gainParameterId.isNotEmpty())
{
    frequency = state.getParameter (frequencyParameterId);
    jassert (frequency != nullptr);

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (frequency))
        ring.setDetents (choice->choices.size());

    ring.setSliderSnapsToMousePosition (false);
    ring.setStyle (hasCentre ? Knob::Style::bandRing : Knob::Style::filter);
    ring.setFaceScale (hasCentre ? 0.62f : 0.40f);

    // The legend is drawn here, not by the slider, so a change of frequency has
    // to repaint the parent or the marked position goes stale.
    ring.onValueChange = [this] { repaint(); };

    addAndMakeVisible (ring);

    ringAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, frequencyParameterId, ring);

    if (hasCentre)
    {
        // Added second, so it sits above the ring and takes the mouse first.
        centre.setStyle (Knob::Style::bandGain);

        // The face is small, but the component spans the whole cell: its gain
        // track and its plus and minus are drawn well outside the face, and a
        // component only tight around the face would clip them away entirely.
        centre.setFaceScale (0.34f);
        centre.setCircularHitTest (true);
        addAndMakeVisible (centre);

        centreAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, gainParameterId, centre);
    }

    buildLegend();
}

void ConcentricBand::buildLegend()
{
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

void ConcentricBand::paint (juce::Graphics& g)
{
    if (legend.isEmpty())
        return;

    const auto& p = theme::palette();
    const auto area = getLocalBounds().toFloat();
    const auto centrePoint = area.getCentre();

    const auto ringRadius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f * ring.getFaceScale();

    // Far enough out to clear the ring, but never so far that a label runs off
    // the top of the cell and gets clipped by whatever is above it.
    const auto textRadius = juce::jmin (ringRadius * (hasCentre ? 1.62f : 1.72f),
                                        area.getHeight() * 0.5f - 11.0f);

    const auto startAngle = ring.getRotaryParameters().startAngleRadians;
    const auto endAngle   = ring.getRotaryParameters().endAngleRadians;
    const auto selected   = juce::roundToInt (ring.getValue());

    for (int i = 0; i < legend.size(); ++i)
    {
        const auto t = legend.size() > 1 ? (float) i / (float) (legend.size() - 1) : 0.0f;
        const auto a = startAngle + t * (endAngle - startAngle);

        const juce::Point<float> at { centrePoint.x + textRadius * std::sin (a),
                                      centrePoint.y - textRadius * std::cos (a) };

        const auto isSelected = (i == selected);

        g.setColour (! ringEnabled ? p.blue.withAlpha (0.28f)
                                   : (isSelected ? p.blue : p.blue.withAlpha (0.72f)));
        g.setFont (theme::labelFont (isSelected ? 13.0f : 12.0f, true));
        g.drawText (legend[i], juce::Rectangle<float> (38.0f, 14.0f).withCentre (at),
                    juce::Justification::centred, false);
    }
}

void ConcentricBand::resized()
{
    ring.setBounds (getLocalBounds());

    if (hasCentre)
        centre.setBounds (getLocalBounds());
}

//==============================================================================
SwitchButton::SwitchButton (juce::AudioProcessorValueTreeState& state,
                            const juce::String& parameterId,
                            const juce::String& text, bool blue)
{
    button.setButtonText (text);
    button.setName (blue ? "blue" : "pink");
    addAndMakeVisible (button);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, parameterId, button);
}

void SwitchButton::resized()                 { button.setBounds (getLocalBounds()); }
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
    const auto rate = vuMode ? 0.28f : (level > displayed ? 1.0f : 0.16f);

    displayed += rate * (level - displayed);
    repaint();
}

void OutputMeter::paint (juce::Graphics& g)
{
    const auto& p = theme::palette();

    auto bounds = getLocalBounds();
    const auto labelArea = bounds.removeFromBottom (12);
    const auto well = bounds.withSizeKeepingCentre (kBarWidth, bounds.getHeight()).toFloat();

    g.setColour (p.meterWell);
    g.fillRoundedRectangle (well, 2.0f);

    const auto db = juce::Decibels::gainToDecibels (displayed, -70.0f);
    const auto lo = vuMode ? -20.0f : -60.0f;
    const auto hi = vuMode ? 3.0f : 0.0f;
    const auto reading = vuMode ? db - kVuReference : db;
    const auto norm = juce::jlimit (0.0f, 1.0f, (reading - lo) / (hi - lo));

    if (norm > 0.002f)
    {
        auto bar = well.reduced (1.5f);
        bar = bar.removeFromBottom (bar.getHeight() * norm);

        const auto hot  = vuMode ? reading > 0.0f  : reading > -1.0f;
        const auto warm = vuMode ? reading > -3.0f : reading > -9.0f;

        g.setColour (hot ? p.meterClip : warm ? p.meterHigh : p.meterLow);
        g.fillRoundedRectangle (bar, 1.5f);
    }

    g.setColour (p.outline.withAlpha (0.6f));
    g.drawRoundedRectangle (well.reduced (0.5f), 2.0f, 1.0f);

    g.setColour (p.textDim);
    g.setFont (theme::labelFont (9.0f));
    g.drawText (vuMode ? "VU" : "dBFS", labelArea.toFloat(), juce::Justification::centred, false);
}

} // namespace frostyeq::gui
