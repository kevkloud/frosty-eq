#pragma once

#include "Theme.h"

namespace frostyeq::gui
{

/** A rotary control drawn as a potentiometer: a face and a pointer.

    No value readout of any kind on a cut or boost -- a plus one side, a minus
    the other, and nothing else. That is what the hardware does, and it is the
    point: numbers make people mix with their eyes, hunting a tidy figure and
    flinching from a large move.
*/
class Knob : public juce::Slider
{
public:
    enum class Style
    {
        utility,    ///< blue face, dotted arc, plus and minus. Input and output.
        filter,     ///< blue face, legend around it. The cut filters.
        bandGain,   ///< pink face inside a band's ring.
        bandRing    ///< the white frequency ring a band's gain sits inside.
    };

    Knob() : juce::Slider (juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox) {}

    void setStyle (Style s) noexcept   { style = s; }
    Style getStyle() const noexcept    { return style; }

    void setDetents (int count) noexcept { detents = count; }
    int  getDetents() const noexcept     { return detents; }

    void setFaceScale (float s) noexcept { faceScale = s; }
    float getFaceScale() const noexcept  { return faceScale; }

    /** Where the dotted gain track sits, in pixels from the centre.

        Given rather than derived because the gain control of a band has to
        clear the frequency ring drawn around it, and the face inside that ring
        knows nothing about the ring's size. Zero means "just outside my own
        face", which is what the input and output knobs want. */
    void setTrackRadius (float r) noexcept { trackRadius = r; }
    float getTrackRadius() const noexcept  { return trackRadius; }

    /** The inner control of a concentric pair claims only its own circle, so
        the ring around it stays grabbable right up to the corners. */
    void setCircularHitTest (bool b) noexcept { circularHit = b; }

    bool hitTest (int x, int y) override
    {
        if (! circularHit)
            return juce::Slider::hitTest (x, y);

        const auto centre = getLocalBounds().toFloat().getCentre();
        const auto radius = (float) juce::jmin (getWidth(), getHeight()) * 0.5f * faceScale;

        return juce::Point<float> ((float) x, (float) y).getDistanceFrom (centre) <= radius + 4.0f;
    }

private:
    Style style = Style::utility;
    bool  circularHit = false;
    int   detents = 0;
    float faceScale = 1.0f;
    float trackRadius = 0.0f;
};

//==============================================================================
class FrostyLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    FrostyLookAndFeel() { refreshColours(); }

    void refreshColours();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawHighlighted, bool shouldDrawDown) override;

    juce::Font getLabelFont (juce::Label&) override;

    /** How far out a gain track sits from the edge of the face it surrounds,
        and how far the legend then sits beyond the track. The same two gaps
        everywhere, which is what makes the bands and the utility knobs read as
        one family. */
    static constexpr float kTrackGap  = 10.0f;
    static constexpr float kLegendGap = 12.0f;

    /** A filter has no gain track between its face and its legend, so the one
        gap there has to carry what two carry on a band. */
    static constexpr float kFilterLegendGap = 20.0f;

    /** A ring of dots, used for the gain track around a band and around the
        input and output knobs. */
    static void drawDottedArc (juce::Graphics&, juce::Point<float> centre, float radius,
                               float startAngle, float endAngle, juce::Colour, float dotSize);
};

} // namespace frostyeq::gui
