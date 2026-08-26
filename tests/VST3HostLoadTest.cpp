#include <juce_audio_utils/juce_audio_utils.h>

#include <iostream>

int main(int argumentCount, char** arguments)
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    if (argumentCount != 2)
    {
        std::cerr << "Usage: CKVST3HostLoadTest <plugin.vst3>\n";
        return 2;
    }

    const juce::File plugin(arguments[1]);
    if (!plugin.exists())
    {
        std::cerr << "Plugin does not exist: " << plugin.getFullPathName() << '\n';
        return 3;
    }

    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile(descriptions, plugin.getFullPathName());
    if (descriptions.isEmpty())
    {
        std::cerr << "VST3 scan found no audio components\n";
        return 4;
    }

    for (const auto* description : descriptions)
    {
        std::cout << "FOUND name=" << description->name
                  << " manufacturer=" << description->manufacturerName
                  << " version=" << description->version
                  << " uid=" << description->uniqueId << '\n';

        juce::String error;
        auto instance = format.createInstanceFromDescription(*description, 48000.0, 512, error);
        if (instance == nullptr)
        {
            std::cerr << "Instantiation failed: " << error << '\n';
            return 5;
        }

        instance->setNonRealtime(true);
        instance->prepareToPlay(48000.0, 512);
        std::unique_ptr<juce::AudioProcessorEditor> editor(instance->createEditorIfNeeded());
        if (editor == nullptr)
        {
            std::cerr << "Plugin did not create an editor\n";
            return 6;
        }

        std::cout << "PASS inputs=" << instance->getTotalNumInputChannels()
                  << " outputs=" << instance->getTotalNumOutputChannels()
                  << " latency=" << instance->getLatencySamples()
                  << " tail=" << instance->getTailLengthSeconds()
                  << " editor=" << editor->getWidth() << 'x' << editor->getHeight() << '\n';
        editor.reset();
        instance->releaseResources();
    }

    return 0;
}
