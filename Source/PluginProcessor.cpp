#include "PluginProcessor.h"
#include "PluginEditor.h"

CKStemSplitterAudioProcessor::CKStemSplitterAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    startTimerHz(10);
}

CKStemSplitterAudioProcessor::~CKStemSplitterAudioProcessor()
{
    stopTimer();
    capturingSelection.store(false);
    {
        const juce::SpinLock::ScopedLockType lock(captureWriterLock);
        captureWriter.reset();
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout CKStemSplitterAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"mode", 1},
        "Output",
        juce::StringArray{"Original", "Acapella", "Instrumental"},
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
    captureSampleRate = sampleRate;
    captureChannels = juce::jmax(1, getTotalNumInputChannels());
    stemEngine.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
}

void CKStemSplitterAudioProcessor::releaseResources()
{
    if (capturingSelection.load() && capturedSamples.load() > 0)
        stopSelectionCaptureAndSplit();
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

    juce::int64 hostSamplePosition = -1;
    if (auto* currentPlayHead = getPlayHead())
    {
        if (auto pos = currentPlayHead->getPosition())
        {
            if (auto samplePos = pos->getTimeInSamples())
                hostSamplePosition = *samplePos;
        }
    }

    if (capturingSelection.load())
    {
        if (captureStartHostSample.load() < 0 && hostSamplePosition >= 0)
            captureStartHostSample.store(hostSamplePosition);

        const juce::SpinLock::ScopedTryLockType lock(captureWriterLock);
        if (lock.isLocked() && captureWriter != nullptr)
        {
            const auto channelsToWrite = juce::jmin(captureChannels, buffer.getNumChannels());
            const float* channelData[2] { nullptr, nullptr };
            for (int ch = 0; ch < channelsToWrite && ch < 2; ++ch)
                channelData[ch] = buffer.getReadPointer(ch);

            if (channelsToWrite == 1)
                channelData[1] = channelData[0];

            if (captureWriter->write(channelData, buffer.getNumSamples()))
            {
                capturedSamples.fetch_add(buffer.getNumSamples());
                lastCaptureActivityTicks.store(juce::Time::getHighResolutionTicks());
            }
        }
    }

    const auto* modeParam = apvts.getRawParameterValue("mode");
    const auto modeIndex = modeParam != nullptr ? static_cast<int>(modeParam->load()) : 0;

    stemEngine.process(buffer, static_cast<StemEngine::StemMode>(modeIndex), hostSamplePosition);

    const auto* gainParam = apvts.getRawParameterValue("outputGain");
    const float gainDb = gainParam != nullptr ? gainParam->load() : 0.0f;
    buffer.applyGain(juce::Decibels::decibelsToGain(gainDb));
}

bool CKStemSplitterAudioProcessor::startSelectionCapture()
{
    if (stemEngine.isBusy() || capturingSelection.load())
        return false;

    const auto capturesDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("Commercial Kings")
        .getChildFile("CK Stem Splitter")
        .getChildFile("Captures");
    capturesDir.createDirectory();

    capturedSelectionFile = capturesDir.getNonexistentChildFile(
        "audition-selection-" + juce::String(juce::Time::currentTimeMillis()), ".wav", false);

    auto stream = capturedSelectionFile.createOutputStream();
    if (stream == nullptr || !stream->openedOk())
    {
        setCaptureStatus("Could not create the temporary capture file");
        return false;
    }

    juce::WavAudioFormat wav;
    auto* writer = wav.createWriterFor(stream.release(),
                                       captureSampleRate,
                                       static_cast<unsigned int>(juce::jlimit(1, 2, captureChannels)),
                                       32,
                                       {},
                                       0);
    if (writer == nullptr)
    {
        setCaptureStatus("Could not start the WAV capture writer");
        return false;
    }

    {
        const juce::SpinLock::ScopedLockType lock(captureWriterLock);
        captureWriter.reset(writer);
    }

    if (auto* mode = apvts.getParameter("mode"))
        mode->setValueNotifyingHost(0.0f);

    capturedSamples.store(0);
    captureStartHostSample.store(-1);
    lastCaptureActivityTicks.store(0);
    capturingSelection.store(true);
    setCaptureStatus("SCAN ARMED - click Audition Apply once; playback is not required");
    return true;
}

void CKStemSplitterAudioProcessor::timerCallback()
{
    if (!capturingSelection.load() || capturedSamples.load() == 0)
        return;

    const auto lastTicks = lastCaptureActivityTicks.load();
    if (lastTicks <= 0)
        return;

    const auto elapsed = juce::Time::highResolutionTicksToSeconds(
        juce::Time::getHighResolutionTicks() - lastTicks);
    if (elapsed >= 0.65)
        stopSelectionCaptureAndSplit();
}

void CKStemSplitterAudioProcessor::stopSelectionCaptureAndSplit()
{
    if (!capturingSelection.exchange(false))
        return;

    {
        const juce::SpinLock::ScopedLockType lock(captureWriterLock);
        captureWriter.reset();
    }

    if (capturedSamples.load() < static_cast<juce::int64>(captureSampleRate * 0.10))
    {
        capturedSelectionFile.deleteFile();
        setCaptureStatus("No audio was scanned - keep the range highlighted and click Audition Apply");
        return;
    }

    const auto startSample = juce::jmax<juce::int64>(0, captureStartHostSample.load());
    stemEngine.setTimelineOffsetSamples(startSample);
    stemEngine.setSourceFile(capturedSelectionFile);
    setCaptureStatus("Selection scanned - starting AI separation...");
    stemEngine.startSeparation();
}

void CKStemSplitterAudioProcessor::setCaptureStatus(const juce::String& newStatus)
{
    const juce::ScopedLock lock(captureStatusLock);
    captureStatus = newStatus;
}

juce::String CKStemSplitterAudioProcessor::getCaptureStatus() const
{
    const juce::ScopedLock lock(captureStatusLock);
    return captureStatus;
}

juce::AudioProcessorEditor* CKStemSplitterAudioProcessor::createEditor()
{
    return new CKStemSplitterAudioProcessorEditor(*this);
}

void CKStemSplitterAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void CKStemSplitterAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CKStemSplitterAudioProcessor();
}
