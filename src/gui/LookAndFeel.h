#pragma once

#include "Theme.h"

namespace frostyeq::gui
{

/** A rotary control drawn as a potentiometer: a face, an edge and a pointer.

    No value arc and no numeric readout. On the hardware a gain pot is marked
    only with a plus on one side and a minus on the other, and that is the
    point of it -- you make the move the sound needs rather than the move the
    number suggests. Anyone mixing with their eyes will hunt for a tidy figure
    and shy away from a large one.
*/
class Knob : public juce::Slider
{
public:
    Knob() : juce::Slider (juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox) {}

    /** Marks a plus and a minus either side, for cut/boost controls. */
    void setPolarityMarks (bool shouldShow) noexcept { polarityMarks = shouldShow; }
    bool hasPolarityMarks() const noexcept           { return polarityMarks; }

    /** Stepped controls get a detent tick at each switch position. */
    void setDetents (int count) noexcept { detents = count; }
    int  getDetents() const noexcept     { return detents; }

    void setFaceScale (float s) noexcept { faceScale = s; }
    float getFaceScale() const noexcept  { return faceScale; }

    /** The inner control of a concentric pair claims only its own circle, so
        the ring around it stays grabbable right up to the corners. */
    void setCircularHitTest (bool shouldBeCircular) noexcept { circularHit = shouldBeCircular; }

    bool hitTest (int x, int y) override
    {
        if (! circularHit)
            return juce::Slider::hitTest (x, y);

        const auto centre = getLocalBounds().toFloat().getCentre();
        const auto radius = (float) juce::jmin (getWidth(), getHeight()) * 0.5f * faceScale;

        return juce::Point<float> ((float) x, (float) y).getDistanceFrom (centre) <= radius + 4.0f;
    }

private:
    bool  circularHit = false;
    bool  polarityMarks = false;
    int   detents = 0;
    float faceScale = 1.0f;
};

//==============================================================================
class FrostyLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void refreshColours();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawHighlighted, bool shouldDrawDown) override;

    juce::Font getLabelFont (juce::Label&) override;
};

} // namespace frostyeq::gui
