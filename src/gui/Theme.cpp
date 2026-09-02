#include "Theme.h"
#include <BinaryData.h>

namespace frostyeq::theme
{

const Palette& palette() noexcept { return kLight; }

namespace
{
    /** Parsed once each. These are called from paint, and building a typeface
        reads and decodes the whole file. */
    juce::Typeface::Ptr minervaBlack()
    {
        static const juce::Typeface::Ptr face = juce::Typeface::createSystemTypefaceFor (
            BinaryData::TGMinervaBlackBlack_otf, (size_t) BinaryData::TGMinervaBlackBlack_otfSize);
        return face;
    }

    juce::Typeface::Ptr blender()
    {
        static const juce::Typeface::Ptr face = juce::Typeface::createSystemTypefaceFor (
            BinaryData::TGBlender_otf, (size_t) BinaryData::TGBlender_otfSize);
        return face;
    }

    juce::Font build (const juce::Typeface::Ptr& face, float height, float tracking)
    {
        if (face == nullptr)   // should not happen: the file is in the binary
            return juce::Font (juce::FontOptions {}.withHeight (height));

        return juce::Font (juce::FontOptions (face).withHeight (height))
                   .withExtraKerningFactor (tracking);
    }
}

juce::Font labelFont (float height, bool)
{
    // Minerva Black is a single weight, so the bold flag has nothing to select
    // and is kept only so callers do not all have to change. The tracking is
    // from the mockup, where the panel legends are set noticeably open.
    return build (minervaBlack(), height, 0.08f);
}

juce::Font captionFont (float height)
{
    return build (blender(), height, 0.04f);
}

void drawOutlinedText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                       juce::Justification justification, const juce::Font& font,
                       juce::Colour fill, float outlineThickness)
{
    if (text.isEmpty())
        return;

    juce::GlyphArrangement glyphs;
    glyphs.addFittedText (font, text, area.getX(), area.getY(),
                          area.getWidth(), area.getHeight(), justification, 1);

    juce::Path path;
    glyphs.createPath (path);

    g.setColour (juce::Colours::black.withAlpha (fill.getFloatAlpha()));
    g.strokePath (path, juce::PathStrokeType (outlineThickness,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    g.setColour (fill);
    g.fillPath (path);
}

} // namespace frostyeq::theme
