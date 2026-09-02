#include "Theme.h"

namespace frostyeq::theme
{

const Palette& palette() noexcept { return kLight; }

const juce::String& displayTypeface()
{
    // Resolved once, not per glyph: enumerating installed faces walks the
    // system font list and this is called from paint.
    static const juce::String name = []
    {
        const auto installed = juce::Font::findAllTypefaceNames();

        for (const auto* candidate : { "SF Pro Rounded", "Avenir Next Rounded",
                                       "Arial Rounded MT Bold", "Avenir Next",
                                       "Segoe UI Variable Display", "Segoe UI" })
            if (installed.contains (candidate))
                return juce::String (candidate);

        return juce::String();   // nothing rounded installed: take the default sans
    }();

    return name;
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
