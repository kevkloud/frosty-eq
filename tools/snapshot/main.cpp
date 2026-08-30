// Renders the plugin editor to a PNG without needing a display or screen
// recording permission. Useful for reviewing layout changes, and for attaching
// a picture of the UI to a pull request.
//
//   snapshot out.png [width height] [param=value ...]
//
// e.g.  snapshot boosted.png 760 452 mid_gain=12 lf_gain=-8 hpf_freq=2

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::File out = juce::File::getCurrentWorkingDirectory()
                               .getChildFile (argc > 1 ? argv[1] : "editor.png");

    FrostyEqAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    // Parameters first, editor second. Attachments read the current value in
    // their constructors, synchronously; setting parameters afterwards relies
    // on the message queue, which is not running here, so the panel would
    // render at its defaults no matter what was asked for.
    for (int i = 4; i < argc; ++i)
    {
        const juce::String arg { argv[i] };
        const auto split = arg.indexOfChar ('=');

        if (split < 0)
            continue;

        const auto id    = arg.substring (0, split);
        const auto value = arg.substring (split + 1).getFloatValue();

        if (auto* param = processor.getApvts().getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
        else
            std::cerr << "unknown parameter: " << id << '\n';
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    if (editor == nullptr)
    {
        std::cerr << "no editor\n";
        return 1;
    }

    if (argc > 3)
        editor->setSize (std::atoi (argv[2]), std::atoi (argv[3]));

    // Parts of the panel refresh on timers, so let those fire before capturing.
    for (int i = 0; i < 8; ++i)
    {
        juce::Thread::sleep (40);
        juce::Timer::callPendingTimersSynchronously();
    }


    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), false, 2.0f);

    juce::PNGImageFormat png;
    std::unique_ptr<juce::FileOutputStream> stream (out.createOutputStream());

    if (stream == nullptr || ! png.writeImageToStream (image, *stream))
    {
        std::cerr << "could not write " << out.getFullPathName() << '\n';
        return 1;
    }

    std::cout << "wrote " << out.getFullPathName()
              << " (" << image.getWidth() << "x" << image.getHeight() << ")\n";
    return 0;
}
