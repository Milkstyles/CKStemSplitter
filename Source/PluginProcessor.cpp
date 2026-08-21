#include "PluginProcessor.h"
#include "PluginEditor.h"

CKStemSplitterAudioProcessor::CKStemSplitterAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout CKStemSplitterAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"mode", 1},
        "Output",
        juce::StringArray{"Original", "Vocals", "Instrumental"},
        0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputGain", 1},
        "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f),
        0.0f,
        "dB"));

    return { params.begin(), params.end() };
}

void CKStemSplitterAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    stemEngine.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
}

void CKStemSplitterAudioProcessor::releaseResources()
{
    stemEngine.reset();
}

bool CKStemSplitterAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (input != output)
        return false;

    return output == juce::AudioChannelSet::mono()
        || output == juce::AudioChannelSet::stereo();
}

void CKStemSplitterAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    const auto* modeParam = apvts.getRawParameterValue("mode");
    const auto modeIndex = modeParam != nullptr ? static_cast<int>(modeParam->load()) : 0;

    juce::int64 hostSamplePosition = -1;
    if (auto* playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            if (auto samplePos = pos->getTimeInSamples())
                hostSamplePosition = *samplePos;
        }
    }

    stemEngine.process(buffer, static_cast<StemEngine::StemMode>(modeIndex), hostSamplePosition);

    const auto* gainParam = apvts.getRawParameterValue("outputGain");
    const float gainDb = gainParam != nullptr ? gainParam->load() : 0.0f;
    buffer.applyGain(juce::Decibels::decibelsToGain(gainDb));
}

juce::AudioProcessorEditor* CKStemSplitterAudioProcessor::createEditor()
{
    return new CKStemSplitterAudioProcessorEditor(*this);
}

void CKStemSplitterAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    const auto source = stemEngine.getSourceFile();
    if (source.existsAsFile())
        state.setProperty("sourceFile", source.getFullPathName(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void CKStemSplitterAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(apvts.state.getType()))
        {
            auto restored = juce::ValueTree::fromXml(*xml);
            const auto sourcePath = restored.getProperty("sourceFile").toString();
            restored.removeProperty("sourceFile", nullptr);
            apvts.replaceState(restored);

            if (sourcePath.isNotEmpty())
            {
                const juce::File source(sourcePath);
                if (source.existsAsFile())
                    stemEngine.setSourceFile(source);
            }
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CKStemSplitterAudioProcessor();
}
