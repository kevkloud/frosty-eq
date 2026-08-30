#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "dsp/ModelTables.h"

namespace frostyeq::theme
{

/** Two schemes, one per module, so the panel says which one you are on without
    a label having to.

    Deliberately not a reproduction of either module's livery. Copying that
    would be trade dress, and it invites the plugin to be judged as a failed
    clone rather than used on its own terms.
*/
struct Palette
{
    juce::Colour background, panel, outline, text, textDim;
    juce::Colour knobFace, knobEdge, pointer, accent, active;
    juce::Colour meterLow, meterHigh, meterClip, meterWell;
};

inline const Palette k1073
{
    juce::Colour (0xffc9808f),   // background: muted rose
    juce::Colour (0xffd4909d),   // panel
    juce::Colour (0xff8f5866),   // outline
    juce::Colour (0xff2b1c21),   // text
    juce::Colour (0xff6d4a53),   // textDim
    juce::Colour (0xffe4b7c0),   // knobFace
    juce::Colour (0xff8f5866),   // knobEdge
    juce::Colour (0xff2b1c21),   // pointer
    juce::Colour (0xff3d2930),   // accent
    juce::Colour (0xfff0d9a0),   // active
    juce::Colour (0xff4c7a4f),
    juce::Colour (0xffb08a2e),
    juce::Colour (0xff9c3b32),
    juce::Colour (0xff7d4c58)    // meterWell
};

inline const Palette k1084
{
    juce::Colour (0xff191919),   // background
    juce::Colour (0xff242424),   // panel
    juce::Colour (0xff3f3f3f),   // outline
    juce::Colour (0xffdcdcdc),   // text
    juce::Colour (0xff8a8a8a),   // textDim
    juce::Colour (0xff2e2e2e),   // knobFace
    juce::Colour (0xff4a4a4a),   // knobEdge
    juce::Colour (0xffe6e6e6),   // pointer
    juce::Colour (0xffd6d6d6),   // accent
    juce::Colour (0xffe8b84b),   // active
    juce::Colour (0xff7fbf5f),
    juce::Colour (0xffe8b84b),
    juce::Colour (0xffe05a4a),
    juce::Colour (0xff101010)    // meterWell
};

/** Current scheme. GUI only, message thread only. */
const Palette& palette() noexcept;
void setModel (Model) noexcept;

inline constexpr float corner = 3.0f;

inline juce::Font labelFont (float height)
{
    return juce::Font (juce::FontOptions {}.withHeight (height));
}

} // namespace frostyeq::theme
