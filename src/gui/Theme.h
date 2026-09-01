#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "dsp/ModelTables.h"

namespace frostyeq::theme
{

/** Light scheme, from the design: pink for the equaliser bands and the
    section legends, blue for gain and filters, white rings, everything on a
    near-white panel.

    Deliberately not a reproduction of either module's livery -- copying that
    would be trade dress, and it invites the plugin to be judged as a failed
    clone rather than used on its own terms.
*/
struct Palette
{
    juce::Colour background, panel, outline, hairline;
    juce::Colour text, textDim;
    juce::Colour blue, blueFill, pink, pinkFill, white;
    juce::Colour meterLow, meterHigh, meterClip, meterWell;
};

inline const Palette kLight
{
    juce::Colour (0xffefefef),   // background
    juce::Colour (0xffe4e4e4),   // panel: header and preset strip
    juce::Colour (0xff9e9e9e),   // outline: knob edges
    juce::Colour (0xffb4b4b4),   // hairline: section rules

    juce::Colour (0xff6f6f6f),   // text
    juce::Colour (0xff9a9a9a),   // textDim

    juce::Colour (0xff4fb8e8),   // blue: frequency legends, gain knobs
    juce::Colour (0xff7fd0f2),   // blueFill: knob faces, Hi-Q
    juce::Colour (0xfff08cb4),   // pink: section legends, plus and minus
    juce::Colour (0xfffbc8d9),   // pinkFill: band knob faces, EQL and phase
    juce::Colour (0xffffffff),   // white: rings, pointers, button text

    juce::Colour (0xff6bbf7a),
    juce::Colour (0xffe0b040),
    juce::Colour (0xffe0685a),
    juce::Colour (0xffd6d6d6)    // meterWell
};

const Palette& palette() noexcept;
void setModel (Model) noexcept;

inline constexpr float corner = 3.0f;

inline juce::Font labelFont (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions {}
                           .withHeight (height)
                           .withStyle (bold ? "Bold" : "Regular"));
}

} // namespace frostyeq::theme
