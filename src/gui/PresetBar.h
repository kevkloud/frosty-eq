#pragma once

#include "LookAndFeel.h"
#include "presets/PresetManager.h"

namespace frostyeq::gui
{

/** Preset strip: back, the name, forward, and a menu behind the name.

    The name doubles as the menu button, because there is no room in a module
    for a separate one and no reason for two things when one will do. An
    asterisk after the name means a control has moved since it was loaded, so
    it is clear that what is heard is no longer what the name says.
*/
class PresetBar final : public juce::Component,
                        private juce::Timer
{
public:
    explicit PresetBar (PresetManager&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showMenu();
    void promptForName();
    void chooseImport();
    void chooseExport();

    PresetManager& presets;

    juce::TextButton previous { "<" }, next { ">" }, name;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::String shown;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
};

} // namespace frostyeq::gui
