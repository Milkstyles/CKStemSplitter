#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdint>

namespace
{
juce::File getLastScanStateFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Commercial Kings")
        .getChildFile("CK Stem Splitter")
        .getChildFile("last-scan.txt");
}

juce::File getAutomationFile(const juce::String& name)
{
    return getLastScanStateFile().getSiblingFile(name);
}
}

CKStemSplitterAudioProcessor::CKStemSplitterAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

CKStemSplitterAudioProcessor::~CKStemSplitterAudioProcessor()
{
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
            if (captureWriter->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()))
            {
                capturedSamples.fetch_add(buffer.getNumSamples());
                lastCaptureActivityTicks.store(juce::Time::getHighResolutionTicks());
            }
        }
    }

    const auto* modeParam = apvts.getRawParameterValue("mode");
    const auto modeIndex = capturingSelection.load()
        ? 0
        : (modeParam != nullptr ? static_cast<int>(modeParam->load()) : 0);

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

    capturedSamples.store(0);
    captureStartHostSample.store(-1);
    lastCaptureActivityTicks.store(0);
    capturingSelection.store(true);
    setCaptureStatus("SCAN ARMED - click Audition Apply once; playback is not required");
    return true;
}

bool CKStemSplitterAudioProcessor::startAutomatedWorkflow(int modeIndex, void* editorWindowHandle)
{
    if (modeIndex != 1 && modeIndex != 2)
        return false;

    if (!startSelectionCapture())
        return false;

    automatedCapture = true;
    const auto requestId = juce::String(juce::Time::currentTimeMillis());
    const auto companion = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
        .getChildFile("Commercial Kings")
        .getChildFile("CK Stem Splitter")
        .getChildFile("companion")
        .getChildFile("CKStemBridge.exe");

    if (!companion.existsAsFile())
    {
        automatedCapture = false;
        capturingSelection.store(false);
        setCaptureStatus("Automation companion is missing - reinstall CK Stem Splitter");
        return false;
    }

    const juce::String mode = modeIndex == 1 ? "acapella" : "instrumental";
    const auto windowValue = static_cast<juce::int64>(reinterpret_cast<std::intptr_t>(editorWindowHandle));
    const juce::String parameters = "orchestrate " + mode + " " + requestId + " " + juce::String(windowValue);
    if (!juce::Process::openDocument(companion.getFullPathName(),
                                     parameters))
    {
        automatedCapture = false;
        capturingSelection.store(false);
        setCaptureStatus("Could not start the Audition automation companion");
        return false;
    }

    setCaptureStatus("Working offline - keep Audition open; no playback is required");
    return true;
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
    saveLastScanState(startSample);
    if (automatedCapture)
    {
        automatedCapture = false;
        setCaptureStatus("Selection scanned - companion is continuing automatically...");
        return;
    }
    setCaptureStatus("Selection scanned - starting AI separation...");
    stemEngine.startSeparation();
}

void CKStemSplitterAudioProcessor::saveLastScanState(juce::int64 timelineOffset)
{
    auto stateFile = getLastScanStateFile();
    stateFile.getParentDirectory().createDirectory();
    stateFile.replaceWithText(capturedSelectionFile.getFullPathName()
                              + "\n" + juce::String(timelineOffset));
}

void CKStemSplitterAudioProcessor::restoreLastScanState()
{
    restoredLastScan = true;
    const auto stateFile = getLastScanStateFile();
    if (!stateFile.existsAsFile())
        return;

    const auto lines = juce::StringArray::fromLines(stateFile.loadFileAsString());
    if (lines.isEmpty())
        return;

    const juce::File scanFile(lines[0].trim());
    if (!scanFile.existsAsFile())
        return;

    constexpr juce::int64 maximumAgeMs = 24LL * 60LL * 60LL * 1000LL;
    const auto ageMs = juce::Time::currentTimeMillis()
        - scanFile.getLastModificationTime().toMilliseconds();
    if (ageMs < 0 || ageMs > maximumAgeMs)
        return;

    const auto timelineOffset = lines.size() > 1 ? lines[1].getLargeIntValue() : 0;
    capturedSelectionFile = scanFile;
    stemEngine.setTimelineOffsetSamples(juce::jmax<juce::int64>(0, timelineOffset));
    stemEngine.setSourceFile(scanFile);
    setCaptureStatus("Restoring the completed selection scan...");
    stemEngine.startSeparation();
}

void CKStemSplitterAudioProcessor::restoreLastScanFromUi()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (!restoredLastScan)
        restoreLastScanState();
}

void CKStemSplitterAudioProcessor::loadPreparedAutomationStemFromUi()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (automationRequestChecked)
        return;

    const auto requestFile = getAutomationFile("automation-process.txt");
    if (!requestFile.existsAsFile())
        return;
    automationRequestChecked = true;

    const auto lines = juce::StringArray::fromLines(requestFile.loadFileAsString());
    requestFile.deleteFile();
    if (lines.size() < 6)
        return;

    automationRequestId = lines[0].trim();
    const auto requestedMode = lines[1].trim().equalsIgnoreCase("instrumental") ? 2 : 1;
    const juce::File scanFile(lines[2].trim());
    const auto timelineOffset = lines[3].trim().getLargeIntValue();
    const juce::File preparedFile(requestedMode == 2 ? lines[5].trim() : lines[4].trim());
    if (auto* mode = apvts.getRawParameterValue("mode"))
        mode->store(static_cast<float>(requestedMode));
    capturedSelectionFile = scanFile;
    stemEngine.setTimelineOffsetSamples(juce::jmax<juce::int64>(0, timelineOffset));
    stemEngine.setSourceFile(scanFile);
    setCaptureStatus("Loading finished stems from the companion...");
    if (!stemEngine.loadPreparedStem(preparedFile, static_cast<StemEngine::StemMode>(requestedMode)))
    {
        automationRequestId.clear();
        return;
    }

    const auto readyFile = getAutomationFile("automation-ready.txt");
    readyFile.getParentDirectory().createDirectory();
    readyFile.replaceWithText(automationRequestId);
    automationRequestId.clear();
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

