#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace frostyeq::theme
{

/** Light scheme, from the design: pink for the equaliser bands and the
    section legends, azure for gain and filters, white rings, everything on a
    near-white panel. Every piece of text on the panel is drawn with a thin
    black outline, so a pale fill still reads against a pale ground.

    Deliberately not a reproduction of the module's livery -- copying that
    would be trade dress, and it invites the plugin to be judged as a failed
    clone rather than used on its own terms.
*/
struct Palette
{
    juce::Colour background, panel, outline, hairline;
    juce::Colour text, textDim;
    juce::Colour blue, blueFill, pink, pinkFill, white;
    juce::Colour azure, hiQ, engagedPink, labelPink, grey;
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

    juce::Colour (0xff4fb8e8),   // blue: dotted gain tracks
    juce::Colour (0xff7fd0f2),   // blueFill
    juce::Colour (0xfff08cb4),   // pink: plus and minus on a band
    juce::Colour (0xfffbc8d9),   // pinkFill: band knob faces
    juce::Colour (0xffffffff),   // white: rings, pointers, unselected legends

    juce::Colour (0xff97ddff),   // azure: knob caps, selected legend, INPUT/OUTPUT
    juce::Colour (0xff4cacdc),   // hiQ: the Hi-Q switch when engaged -- not the azure above
    juce::Colour (0xfff08eb5),   // engagedPink: EQL and phase when engaged
    juce::Colour (0xffffc8dd),   // labelPink: the band section legends
    juce::Colour (0xffa6a6a6),   // grey: single-element knob rings, disengaged switches

    juce::Colour (0xff6bbf7a),
    juce::Colour (0xffe0b040),
    juce::Colour (0xffe0685a),
    juce::Colour (0xffd6d6d6)    // meterWell
};

const Palette& palette() noexcept;

inline constexpr float corner = 3.0f;

/** The two places a typeface is named.

    Both faces are embedded in the binary rather than looked up on the machine.
    A name resolved at runtime gives every listener a different panel: the
    first round of design feedback on this plugin came from two people looking
    at two different fonts without either of them knowing it.

    See assets/fonts/README.md, including the note on licensing.

    labelFont is Minerva Black, which is everything on the panel, tracked out a
    little as the mockup has it. captionFont is Blender, which is INPUT and
    OUTPUT and nothing else.
*/
juce::Font labelFont (float height, bool bold = false);
juce::Font captionFont (float height);

/** Text with a thin black outline around it, which is how every label on the
    panel is drawn. The outline takes the fill's alpha, so dimming a label
    dims its outline with it rather than leaving a hard black ghost. */
void drawOutlinedText (juce::Graphics&, const juce::String&, juce::Rectangle<float>,
                       juce::Justification, const juce::Font&, juce::Colour fill,
                       float outlineThickness = 1.0f);

} // namespace frostyeq::theme
