#pragma once

#include "FactoryPresets.h"
#include <atomic>

namespace frostyeq
{

/** Presets, both the ones that ship with the plugin and the user's own.

    A preset is the same XML the plugin hands the host when it saves state, so
    a preset file and a saved session hold the same thing and stay compatible
    through the same version tag. User presets are plain files in a folder the
    user can open, copy from and back up; sharing one is sending a file.

    Loading always resets every parameter to its default first. Without that a
    preset silently inherits whatever the previous one left set, which is the
    usual way preset systems come to be quietly wrong.
*/
class PresetManager : private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState&);
    ~PresetManager() override;

    static juce::File directory();
    static juce::String extension() { return ".frostyeq"; }

    /** Redirects the preset folder. Tests use it so they never write into the
        user's own presets; nothing else should. */
    static void setDirectoryForTesting (const juce::File&);

    //== Listing ==============================================================
    const std::vector<presets::Factory>& getFactory() const noexcept { return factoryPresets; }
    juce::StringArray getUserNames() const;

    //== Loading ==============================================================
    void loadFactory (int index);
    void loadUser (const juce::String& name);
    bool loadFile (const juce::File&);

    /** Steps through the factory presets and then the user's, so the arrows
        walk the same list the menu shows. */
    void step (int delta);

    //== Storing ==============================================================
    bool saveUser (const juce::String& name);
    bool exportTo (juce::File destination);
    bool importFrom (const juce::File& source);
    bool deleteUser (const juce::String& name);

    //== What is loaded =======================================================
    juce::String getCurrentName() const { return currentName; }

    /** True once any control has moved since the preset was loaded. The panel
        shows this, so it is clear that what is heard is no longer what the name
        says. */
    bool isEdited() const noexcept { return edited.load (std::memory_order_relaxed); }

private:
    void parameterChanged (const juce::String&, float) override;
    void resetToDefaults();
    void apply (const std::vector<presets::Setting>&);

    juce::AudioProcessorValueTreeState& state;
    std::vector<presets::Factory> factoryPresets { presets::factory() };

    juce::String currentName { "Init" };
    std::atomic<bool> edited { false };
    std::atomic<bool> loading { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};

} // namespace frostyeq
