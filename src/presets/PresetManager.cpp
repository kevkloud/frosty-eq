#include "PresetManager.h"

namespace frostyeq
{

namespace P = frostyeq::params;

//==============================================================================
PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s)
    : state (s)
{
    directory().createDirectory();

    for (const auto& id : P::allIds())
        state.addParameterListener (id, this);
}

PresetManager::~PresetManager()
{
    for (const auto& id : P::allIds())
        state.removeParameterListener (id, this);
}

namespace { juce::File testDirectory; }

void PresetManager::setDirectoryForTesting (const juce::File& f) { testDirectory = f; }

juce::File PresetManager::directory()
{
    if (testDirectory != juce::File {})
        return testDirectory;

    const auto root = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);

   #if JUCE_MAC
    // Where a Mac user expects to find plugin presets.
    return root.getChildFile ("Audio/Presets/LT3 Audio/FrostyEQ");
   #else
    return root.getChildFile ("LT3 Audio/FrostyEQ/Presets");
   #endif
}

//==============================================================================
void PresetManager::parameterChanged (const juce::String&, float)
{
    // Fired from whichever thread moved the parameter; an atomic store is all
    // this may do.
    if (! loading.load (std::memory_order_relaxed))
        edited.store (true, std::memory_order_relaxed);
}

void PresetManager::resetToDefaults()
{
    for (const auto& id : P::allIds())
        if (auto* p = state.getParameter (id))
            p->setValueNotifyingHost (p->getDefaultValue());
}

void PresetManager::apply (const std::vector<presets::Setting>& settings)
{
    for (const auto& s : settings)
        if (auto* p = state.getParameter (s.id))
            p->setValueNotifyingHost (p->convertTo0to1 (s.value));
}

//==============================================================================
void PresetManager::loadFactory (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) factoryPresets.size()))
        return;

    loading.store (true, std::memory_order_relaxed);

    resetToDefaults();
    apply (factoryPresets[(size_t) index].settings);
    currentName = factoryPresets[(size_t) index].name;

    loading.store (false, std::memory_order_relaxed);
    edited.store (false, std::memory_order_relaxed);
}

void PresetManager::loadUser (const juce::String& name)
{
    loadFile (directory().getChildFile (name + extension()));
}

bool PresetManager::loadFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    auto xml = juce::XmlDocument::parse (file);

    if (xml == nullptr || ! xml->hasTagName (state.state.getType()))
        return false;

    loading.store (true, std::memory_order_relaxed);

    // Defaults first, so a preset written by an older version cannot leave
    // parameters it never knew about sitting wherever they happened to be.
    resetToDefaults();
    state.replaceState (juce::ValueTree::fromXml (*xml));
    currentName = file.getFileNameWithoutExtension();

    loading.store (false, std::memory_order_relaxed);
    edited.store (false, std::memory_order_relaxed);
    return true;
}

void PresetManager::step (int delta)
{
    const auto users = getUserNames();
    const auto factoryCount = (int) factoryPresets.size();
    const auto total = factoryCount + users.size();

    if (total == 0)
        return;

    // Where we are now, or just before the start if the current name is not in
    // the list -- which is the case after Save As under a new name.
    int index = -1;

    for (int i = 0; i < factoryCount; ++i)
        if (currentName == factoryPresets[(size_t) i].name)
            index = i;

    if (index < 0)
        for (int i = 0; i < users.size(); ++i)
            if (currentName == users[i])
                index = factoryCount + i;

    index = (index + delta + total) % total;

    if (index < factoryCount) loadFactory (index);
    else                      loadUser (users[index - factoryCount]);
}

//==============================================================================
juce::StringArray PresetManager::getUserNames() const
{
    juce::StringArray names;

    for (const auto& f : directory().findChildFiles (juce::File::findFiles, false,
                                                     "*" + extension()))
        names.add (f.getFileNameWithoutExtension());

    names.sort (true);
    return names;
}

bool PresetManager::saveUser (const juce::String& name)
{
    const auto trimmed = name.trim();

    if (trimmed.isEmpty())
        return false;

    // Keep it to something that is a legal filename everywhere, since these
    // get sent between machines.
    const auto safe = juce::File::createLegalFileName (trimmed);
    const auto file = directory().getChildFile (safe + extension());

    if (! exportTo (file))
        return false;

    currentName = safe;
    edited.store (false, std::memory_order_relaxed);
    return true;
}

bool PresetManager::exportTo (juce::File destination)
{
    if (destination.getFileExtension().isEmpty())
        destination = destination.withFileExtension (extension());

    destination.getParentDirectory().createDirectory();

    auto tree = state.copyState();
    tree.setProperty ("stateVersion", P::kStateVersion, nullptr);

    auto xml = tree.createXml();

    return xml != nullptr && xml->writeTo (destination);
}

bool PresetManager::importFrom (const juce::File& source)
{
    if (! source.existsAsFile())
        return false;

    // Copy it into the folder so it joins the list, then load it. Importing
    // something that only works until the file moves is not importing.
    const auto destination = directory().getChildFile (source.getFileName())
                                        .withFileExtension (extension());

    if (! source.copyFileTo (destination))
        return false;

    return loadFile (destination);
}

bool PresetManager::deleteUser (const juce::String& name)
{
    return directory().getChildFile (name + extension()).deleteFile();
}

} // namespace frostyeq
