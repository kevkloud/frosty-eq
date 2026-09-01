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

        g.setColour (dim (p.outline));
        g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre), 1.6f);
        g.drawEllipse (juce::Rectangle<float> ((radius - thickness) * 2.0f,
                                               (radius - thickness) * 2.0f).withCentre (centre), 1.6f);

        // Where the switch is set.
        juce::Path marker;
        marker.addCentredArc (centre.x, centre.y, mid, mid, 0.0f,
                              angle - 0.10f, angle + 0.10f, true);

        g.setColour (dim (p.blue));
        g.strokePath (marker, juce::PathStrokeType (thickness));
        return;
    }

    const auto face    = style == Knob::Style::bandGain ? p.pinkFill : p.blueFill;
    const auto accent  = style == Knob::Style::bandGain ? p.pink : p.blue;
    const auto faceBox = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);

    // Gain controls carry a dotted track with the setting marked on it.
    if (style == Knob::Style::utility || style == Knob::Style::bandGain)
    {
        // Clear of the frequency ring, between it and the legend.
        const auto trackRadius = radius * (style == Knob::Style::bandGain ? 1.98f : 1.34f);

        drawDottedArc (g, centre, trackRadius, startAngle, endAngle,
                       dim (accent.withAlpha (enabled ? 0.55f : 0.2f)), 1.6f);

        const juce::Point<float> at { centre.x + trackRadius * std::sin (angle),
                                      centre.y - trackRadius * std::cos (angle) };

        g.setColour (dim (accent));
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (at));
    }

    g.setColour (dim (face));
    g.fillEllipse (faceBox);
    g.setColour (dim (p.outline));
    g.drawEllipse (faceBox.reduced (0.8f), 1.6f);

    // Pointer.
    {
        const auto tip  = radius - 3.0f;
        const auto tail = radius * 0.05f;

        const juce::Point<float> a { centre.x + tail * std::sin (angle), centre.y - tail * std::cos (angle) };
        const juce::Point<float> b { centre.x + tip  * std::sin (angle), centre.y - tip  * std::cos (angle) };

        g.setColour (dim (p.white));
        g.drawLine ({ a, b }, 2.6f);
    }

    // A plus and a minus, and nothing else. No number, no arc.
    // Plus on the left, minus on the right, as the design has them.
    if (style == Knob::Style::utility || style == Knob::Style::bandGain)
    {
        g.setColour (dim (accent));
        g.setFont (theme::labelFont (15.0f, true));

        const auto box = juce::Rectangle<float> (16.0f, 16.0f);

        const auto mark = [&] (const char* text, float a, float r)
        {
            g.drawText (text, box.withCentre ({ centre.x + r * std::sin (a),
                                                centre.y - r * std::cos (a) }),
                        juce::Justification::centred, false);
        };

        if (style == Knob::Style::bandGain)
        {
            // Below the ring on either side, outside it, where the design puts
            // them -- roughly eight and four o'clock.
            const auto r = radius * 1.98f;
            mark ("+", 4.05f, r);
            mark ("-", 2.23f, r);
        }
        else
        {
            const auto r = radius * 1.62f;
            mark ("+", 4.45f, r);
            mark ("-", 1.83f, r);
        }
    }
}

//==============================================================================
void FrostyLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                          bool shouldDrawHighlighted, bool shouldDrawDown)
{
    const auto& p = theme::palette();
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const auto on = button.getToggleState();

    // Hi-Q is blue, the equaliser and phase switches pink, as in the design.
    const auto tint = button.getName() == "blue" ? p.blueFill : p.pinkFill;

    auto fill = on ? tint : p.background;

    if (shouldDrawDown)             fill = fill.darker (0.12f);
    else if (shouldDrawHighlighted) fill = fill.brighter (0.06f);

    if (! button.isEnabled())
        fill = fill.withAlpha (0.35f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, theme::corner);

    if (! on)
    {
        g.setColour (p.outline.withAlpha (button.isEnabled() ? 0.7f : 0.3f));
        g.drawRoundedRectangle (bounds, theme::corner, 1.2f);
    }

    g.setColour (on ? p.white : (button.isEnabled() ? p.textDim : p.textDim.withAlpha (0.4f)));
    g.setFont (theme::labelFont (11.0f, true));
    g.drawText (button.getButtonText(), bounds, juce::Justification::centred, false);
}

} // namespace frostyeq::gui
