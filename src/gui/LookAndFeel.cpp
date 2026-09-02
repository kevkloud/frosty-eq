#include "LookAndFeel.h"

namespace frostyeq::gui
{

void FrostyLookAndFeel::refreshColours()
{
    const auto& p = theme::palette();

    setColour (juce::ResizableWindow::backgroundColourId, p.background);
    setColour (juce::Label::textColourId,                 p.text);

    setColour (juce::ComboBox::backgroundColourId,        p.background);
    setColour (juce::ComboBox::textColourId,              p.text);
    setColour (juce::ComboBox::outlineColourId,           p.outline);
    setColour (juce::ComboBox::arrowColourId,             p.blue);

    setColour (juce::PopupMenu::backgroundColourId,       p.panel);
    setColour (juce::PopupMenu::textColourId,             p.text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, p.blueFill);
    setColour (juce::PopupMenu::highlightedTextColourId,  p.white);

    // The preset strip is built from TextButtons, which otherwise come out in
    // JUCE's default blue and fight the scheme.
    setColour (juce::TextButton::buttonColourId,   p.background);
    setColour (juce::TextButton::buttonOnColourId, p.blueFill);
    setColour (juce::TextButton::textColourOffId,  p.text);
    setColour (juce::TextButton::textColourOnId,   p.white);

    setColour (juce::AlertWindow::backgroundColourId, p.panel);
    setColour (juce::AlertWindow::textColourId,       p.text);
    setColour (juce::AlertWindow::outlineColourId,    p.outline);
    setColour (juce::TextEditor::backgroundColourId,  p.background);
    setColour (juce::TextEditor::textColourId,        p.text);
    setColour (juce::TextEditor::outlineColourId,     p.outline);
    setColour (juce::TextEditor::highlightColourId,   p.blueFill);
}

juce::Font FrostyLookAndFeel::getLabelFont (juce::Label& label)
{
    return theme::labelFont (label.getHeight() > 0 ? juce::jmin (12.0f, (float) label.getHeight())
                                                   : 11.0f);
}

//==============================================================================
void FrostyLookAndFeel::drawDottedArc (juce::Graphics& g, juce::Point<float> centre, float radius,
                                       float startAngle, float endAngle, juce::Colour colour,
                                       float dotSize)
{
    const auto span = endAngle - startAngle;
    const auto count = juce::jlimit (8, 96, juce::roundToInt (radius * span * 0.16f));

    g.setColour (colour);

    for (int i = 0; i <= count; ++i)
    {
        const auto a = startAngle + span * (float) i / (float) count;
        const juce::Point<float> at { centre.x + radius * std::sin (a),
                                      centre.y - radius * std::cos (a) };

        g.fillEllipse (juce::Rectangle<float> (dotSize, dotSize).withCentre (at));
    }
}

//==============================================================================
void FrostyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float startAngle, float endAngle,
                                          juce::Slider& slider)
{
    const auto& p = theme::palette();
    auto* knob = dynamic_cast<Knob*> (&slider);
    const auto style = knob != nullptr ? knob->getStyle() : Knob::Style::utility;

    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto centre = bounds.getCentre();
    const auto scale  = knob != nullptr ? knob->getFaceScale() : 1.0f;
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f * scale;
    const auto angle  = startAngle + sliderPos * (endAngle - startAngle);
    const auto enabled = slider.isEnabled();

    const auto dim = [enabled] (juce::Colour c) { return enabled ? c : c.withAlpha (0.35f); };

    const auto at = [centre] (float a, float r)
    {
        return juce::Point<float> { centre.x + r * std::sin (a), centre.y - r * std::cos (a) };
    };

    // The frequency ring of a band: a white annulus with the selected position
    // marked on it, drawn behind the gain control that sits inside it.
    if (style == Knob::Style::bandRing)
    {
        const auto thickness = radius * 0.30f;
        const auto mid = radius - thickness * 0.5f;

        juce::Path ring;
        ring.addCentredArc (centre.x, centre.y, mid, mid, 0.0f, 0.0f,
                            juce::MathConstants<float>::twoPi, true);

        g.setColour (dim (p.white));
        g.strokePath (ring, juce::PathStrokeType (thickness));

        g.setColour (dim (p.grey));
        g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre), 1.6f);
        g.drawEllipse (juce::Rectangle<float> ((radius - thickness) * 2.0f,
                                               (radius - thickness) * 2.0f).withCentre (centre), 1.6f);

        // Where the switch is set.
        juce::Path marker;
        marker.addCentredArc (centre.x, centre.y, mid, mid, 0.0f,
                              angle - 0.10f, angle + 0.10f, true);

        g.setColour (dim (p.azure));
        g.strokePath (marker, juce::PathStrokeType (thickness));
        return;
    }

    const auto band    = style == Knob::Style::bandGain;
    const auto face    = band ? p.pinkFill : p.azure;
    const auto accent  = band ? p.pink : p.blue;
    const auto faceBox = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);

    // Gain controls carry a dotted track, with the zero mark on it and a plus
    // and a minus at its ends.
    if (style == Knob::Style::utility || band)
    {
        const auto given = knob != nullptr ? knob->getTrackRadius() : 0.0f;
        const auto track = given > 0.0f ? given : radius + kTrackGap;

        // The track runs from the minus round to the plus and not a hair
        // further, stopping just clear of each symbol rather than running dots
        // through it.
        constexpr float symbolClearance = 0.11f;

        drawDottedArc (g, centre, track, startAngle + symbolClearance, endAngle - symbolClearance,
                       dim (accent.withAlpha (enabled ? 0.55f : 0.2f)), 1.6f);

        // The heavy dot marks nought, and stays there. It used to follow the
        // pointer, which made it a second, redundant indicator and left no mark
        // for where the control rests.
        const auto range = slider.getRange();
        const auto zero  = range.getLength() > 0.0
                             ? (float) juce::jlimit (0.0, 1.0, (0.0 - range.getStart()) / range.getLength())
                             : 0.5f;

        g.setColour (dim (accent));
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f)
                           .withCentre (at (startAngle + zero * (endAngle - startAngle), track)));

        // A plus and a minus, and nothing else. No number, no readout. They sit
        // exactly on the ends of the sweep, so the pointer arrives on the plus
        // at full boost and on the minus at full cut -- which is the whole of
        // what the control tells you.
        static const auto minus = juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92"));

        const auto box = juce::Rectangle<float> (18.0f, 18.0f);
        const auto font = theme::labelFont (16.0f, true);

        theme::drawOutlinedText (g, minus, box.withCentre (at (startAngle, track)),
                                 juce::Justification::centred, font, dim (accent));
        theme::drawOutlinedText (g, "+", box.withCentre (at (endAngle, track)),
                                 juce::Justification::centred, font, dim (accent));
    }

    g.setColour (dim (face));
    g.fillEllipse (faceBox);
    g.setColour (dim (band ? p.outline : p.grey));
    g.drawEllipse (faceBox.reduced (0.8f), band ? 1.6f : 2.2f);

    // Pointer.
    {
        const auto tip  = radius - 3.0f;
        const auto tail = radius * 0.05f;

        g.setColour (dim (p.white));
        g.drawLine ({ at (angle, tail), at (angle, tip) }, 2.6f);
    }
}

//==============================================================================
void FrostyLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                          bool shouldDrawHighlighted, bool shouldDrawDown)
{
    const auto& p = theme::palette();
    const auto bounds = button.getLocalBounds().toFloat().reduced (3.0f);
    const auto on = button.getToggleState();

    // Hi-Q takes its own azure, a shade deeper than the pastel used for the
    // knob caps; the equaliser and phase switches are pink.
    const auto tint = button.getName() == "blue" ? p.hiQ : p.engagedPink;

    auto fill = on ? tint : p.grey;

    if (shouldDrawDown)             fill = fill.darker (0.12f);
    else if (shouldDrawHighlighted) fill = fill.brighter (0.06f);

    if (! button.isEnabled())
        fill = fill.withAlpha (0.35f);

    // An engaged switch glows: a few rounded rectangles stepping outwards at
    // falling alpha. Kept faint, because the text has to stay first.
    if (on && button.isEnabled())
        for (int i = 3; i >= 1; --i)
            {
                g.setColour (tint.withAlpha (0.10f * (float) i / 3.0f));
                g.fillRoundedRectangle (bounds.expanded ((float) i), theme::corner + (float) i);
            }

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, theme::corner);

    theme::drawOutlinedText (g, button.getButtonText(), bounds, juce::Justification::centred,
                             theme::labelFont (12.0f, true),
                             p.white.withAlpha (button.isEnabled() ? 1.0f : 0.4f));
}

} // namespace frostyeq::gui
