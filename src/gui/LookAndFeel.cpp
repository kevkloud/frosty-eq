#include "LookAndFeel.h"

namespace frostyeq::gui
{

void FrostyLookAndFeel::refreshColours()
{
    const auto& p = theme::palette();

    setColour (juce::ResizableWindow::backgroundColourId, p.background);
    setColour (juce::Label::textColourId,                 p.text);
    setColour (juce::ComboBox::backgroundColourId,        p.panel);
    setColour (juce::ComboBox::textColourId,              p.text);
    setColour (juce::ComboBox::outlineColourId,           p.outline);
    setColour (juce::ComboBox::arrowColourId,             p.textDim);
    setColour (juce::PopupMenu::backgroundColourId,       p.panel);
    setColour (juce::PopupMenu::textColourId,             p.text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, p.accent.withAlpha (0.3f));
    setColour (juce::PopupMenu::highlightedTextColourId,  p.text);
}

juce::Font FrostyLookAndFeel::getLabelFont (juce::Label& label)
{
    return theme::labelFont (label.getHeight() > 0 ? juce::jmin (12.0f, (float) label.getHeight())
                                                   : 11.0f);
}

//==============================================================================
void FrostyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float startAngle, float endAngle,
                                          juce::Slider& slider)
{
    const auto& p = theme::palette();
    const auto* knob = dynamic_cast<Knob*> (&slider);

    const auto scale   = knob != nullptr ? knob->getFaceScale() : 1.0f;
    const auto detents = knob != nullptr ? knob->getDetents() : 0;
    const auto marks   = knob != nullptr && knob->hasPolarityMarks();

    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto full = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto radius = full * scale;
    const auto centre = bounds.getCentre();

    const auto angle = startAngle + sliderPos * (endAngle - startAngle);
    const auto enabled = slider.isEnabled();

    const auto face    = enabled ? p.knobFace : p.knobFace.withAlpha (0.4f);
    const auto edge    = enabled ? p.knobEdge : p.knobEdge.withAlpha (0.4f);
    const auto pointer = enabled ? p.pointer  : p.pointer.withAlpha (0.35f);

    // Detent marks sit outside the face, where a switch's legend would be.
    if (detents > 1)
    {
        for (int i = 0; i < detents; ++i)
        {
            const auto t = (float) i / (float) (detents - 1);
            const auto a = startAngle + t * (endAngle - startAngle);
            const auto inner = radius + 3.0f;
            const auto outer = radius + 7.0f;

            const juce::Point<float> p1 { centre.x + inner * std::sin (a), centre.y - inner * std::cos (a) };
            const juce::Point<float> p2 { centre.x + outer * std::sin (a), centre.y - outer * std::cos (a) };

            const auto isActive = std::abs (a - angle) < 1.0e-3f;
            g.setColour (isActive ? (enabled ? p.pointer : pointer) : edge);
            g.drawLine ({ p1, p2 }, isActive ? 2.4f : 1.2f);
        }
    }

    // Face.
    const auto faceBounds = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);
    g.setColour (face);
    g.fillEllipse (faceBounds);
    g.setColour (edge);
    g.drawEllipse (faceBounds.reduced (0.5f), 1.4f);

    // Pointer, from the middle of the face to its edge.
    {
        const auto tip  = radius - 3.0f;
        const auto tail = radius * 0.2f;

        const juce::Point<float> a { centre.x + tail * std::sin (angle), centre.y - tail * std::cos (angle) };
        const juce::Point<float> b { centre.x + tip  * std::sin (angle), centre.y - tip  * std::cos (angle) };

        g.setColour (pointer);
        g.drawLine ({ a, b }, 2.4f);
    }

    // A plus and a minus, and nothing else. No number, no arc.
    if (marks)
    {
        g.setColour (enabled ? p.textDim : p.textDim.withAlpha (0.4f));
        g.setFont (theme::labelFont (12.0f));

        const auto side = radius + 11.0f;
        const auto box = juce::Rectangle<float> (14.0f, 14.0f);

        g.drawText ("-", box.withCentre ({ centre.x - side, centre.y + radius * 0.55f }),
                    juce::Justification::centred, false);
        g.drawText ("+", box.withCentre ({ centre.x + side, centre.y + radius * 0.55f }),
                    juce::Justification::centred, false);
    }
}

//==============================================================================
void FrostyLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                          bool shouldDrawHighlighted, bool shouldDrawDown)
{
    const auto& p = theme::palette();
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const auto on = button.getToggleState();

    auto fill = on ? p.active : p.panel;

    if (shouldDrawDown)             fill = fill.darker (0.2f);
    else if (shouldDrawHighlighted) fill = fill.brighter (0.1f);

    if (! button.isEnabled())
        fill = fill.withAlpha (0.35f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, theme::corner);
    g.setColour (p.outline);
    g.drawRoundedRectangle (bounds, theme::corner, 1.0f);

    g.setColour (on ? p.background.contrasting (0.9f)
                    : (button.isEnabled() ? p.text : p.textDim.withAlpha (0.5f)));
    g.setFont (theme::labelFont (10.0f));
    g.drawText (button.getButtonText(), bounds, juce::Justification::centred, false);
}

} // namespace frostyeq::gui
