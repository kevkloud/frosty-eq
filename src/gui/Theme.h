#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace frostyeq::theme
{

/** Palette and metrics.

    Styled after Ableton's stock devices rather than the original hardware:
    flat, restrained, no bevels or skeuomorphism, controls drawn as thin value
    arcs. Deliberately *not* a reproduction of the hardware's panel -- that
    livery is trade dress, and copying it would raise the same problem as using
    the name. The control layout follows the hardware; the finish does not.
*/

inline const juce::Colour background   { 0xff303030 };
inline const juce::Colour panel        { 0xff383838 };
inline const juce::Colour panelDeep    { 0xff1a1a1a };   // curve display well
inline const juce::Colour outline      { 0xff4a4a4a };
inline const juce::Colour grid         { 0xff2b2b2b };
inline const juce::Colour gridEmphasis { 0xff3d3d3d };

inline const juce::Colour text         { 0xffd8d8d8 };
inline const juce::Colour textDim      { 0xff8c8c8c };

inline const juce::Colour accent       { 0xff6fc3df };   // value arcs, EQ curve
inline const juce::Colour accentSoft   { 0x336fc3df };
inline const juce::Colour active       { 0xffe8b84b };   // engaged toggles
inline const juce::Colour inactive     { 0xff4f4f4f };

inline const juce::Colour meterLow     { 0xff7fbf5f };
inline const juce::Colour meterHigh    { 0xffe8b84b };
inline const juce::Colour meterClip    { 0xffe05a4a };

inline constexpr float knobTrack = 3.0f;
inline constexpr float knobValue = 3.5f;
inline constexpr float corner    = 3.0f;

inline juce::Font labelFont (float height)
{
    return juce::Font (juce::FontOptions {}.withHeight (height));
}

} // namespace frostyeq::theme
