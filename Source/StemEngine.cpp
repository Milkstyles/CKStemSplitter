#include "StemEngine.h"

StemEngine::StemEngine()
{
    readAheadThread.startThread();
}

StemEngine::~StemEngine()
{
    shouldStop.store(true);
    if (workerThread.joinable())
        workerThread.join();

    stemsReady.store(false);
    {
        std::scoped_lock lock(stateMutex);
        clearStemSources();
    }
    readAheadThread.stopThread(2000);
}

void StemEngine::prepare(double sampleRate, int samplesPerBlock, int channels)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    currentChannels = channels;
    expectedHostSamplePosition.store(-1);
    lastRenderedMode.store(-1);

    std::scoped_lock lock(stateMutex);
    vocalsTransport.prepareToPlay(samplesPerBlock, sampleRate);
    instrumentalTransport.prepareToPlay(samplesPerBlock, sampleRate);
}

void StemEngine::reset()
{
    expectedHostSamplePosition.store(-1);
    lastRenderedMode.store(-1);
}

void StemEngine::clearStemSources()
{
    vocalsTransport.stop();
    instrumentalTransport.stop();
    vocalsTransport.setSource(nullptr);
    instrumentalTransport.setSource(nullptr);
    vocalsReaderSource.reset();
    instrumentalReaderSource.reset();
    expectedHostSamplePosition.store(-1);
    lastRenderedMode.store(-1);
}

void StemEngine::setSourceFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return;

    stemsReady.store(false);
    std::scoped_lock lock(stateMutex);
    sourceFile = file;
    progress.store(0.0f);
    clearStemSources();
    status = "Ready to split captured selection";
}

juce::File StemEngine::getSourceFile() const
{
    std::scoped_lock lock(stateMutex);
    return sourceFile;
}

bool StemEngine::hasSourceFile() const
{
    std::scoped_lock lock(stateMutex);
    return sourceFile.existsAsFile();
}

juce::File StemEngine::getEngineExecutable() const
{
    auto base = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
                    .getChildFile("Commercial Kings")
                    .getChildFile("CK Stem Splitter")
                    .getChildFile("engine")
                    .getChildFile("ckstem-engine");
#if JUCE_WINDOWS
    return base.getChildFile("ckstem-engine.exe");
#else
    return base.getChildFile("ckstem-engine");
#endif
}

juce::File StemEngine::getModelCacheDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
        .getChildFile("Commercial Kings")
        .getChildFile("CK Stem Splitter")
        .getChildFile("engine")
        .getChildFile("models");
}

juce::File StemEngine::getCacheDirectoryForSource(const juce::File& source) const
{
    const auto keyText = source.getFullPathName() + "|" + juce::String(source.getSize()) + "|" + juce::String(source.getLastModificationTime().toMilliseconds());
    const auto key = juce::String::toHexString(static_cast<juce::int64>(keyText.hashCode64()));

    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Commercial Kings")
        .getChildFile("CK Stem Splitter")
        .getChildFile("Cache")
        .getChildFile(key);
}

void StemEngine::startSeparation()
{
    if (busy.exchange(true))
        return;

    if (!hasSourceFile())
    {
        busy.store(false);
        std::scoped_lock lock(stateMutex);
        status = "Capture an Audition selection first";
        return;
    }

    shouldStop.store(false);
    progress.store(0.02f);
    stemsReady.store(false);

    if (workerThread.joinable())
        workerThread.join();

    workerThread = std::thread([this] { separationWorker(); });
}

void StemEngine::separationWorker()
{
    juce::File localSource;
    {
        std::scoped_lock lock(stateMutex);
        localSource = sourceFile;
        status = "Preparing AI separation...";
    }

    const auto cacheDir = getCacheDirectoryForSource(localSource);
    cacheDir.createDirectory();
    const auto vocals = cacheDir.getChildFile("vocals.wav");
    const auto instrumental = cacheDir.getChildFile("instrumental.wav");

    if (vocals.existsAsFile() && instrumental.existsAsFile())
    {
        progress.store(0.9f);
        if (loadCachedStems(vocals, instrumental))
            progress.store(1.0f);
        busy.store(false);
        return;
    }

    const auto engine = getEngineExecutable();
    if (!engine.existsAsFile())
    {
        std::scoped_lock lock(stateMutex);
        status = "AI engine is missing - reinstall CK Stem Splitter";
        busy.store(false);
        return;
    }

    {
        std::scoped_lock lock(stateMutex);
        status = "Separating vocals and instrumental...";
    }
    progress.store(0.1f);

    juce::ChildProcess process;
    juce::StringArray args;
    args.add(engine.getFullPathName());
    args.add("separate");
    args.add(localSource.getFullPathName());
    args.add(cacheDir.getFullPathName());
    args.add("--model");
    args.add("htdemucs_ft_vocals");
    args.add("--small");
    args.add("--providers");
    args.add("auto");
    args.add("--cache-dir");
    args.add(getModelCacheDirectory().getFullPathName());
    args.add("--karaoke");
    args.add("--verbose");

    if (!process.start(args))
    {
        std::scoped_lock lock(stateMutex);
        status = "Could not start the AI engine";
        busy.store(false);
        return;
    }

    juce::String pending;
    while (process.isRunning() && !shouldStop.load())
    {
        pending += process.readAllProcessOutput();
        auto lines = juce::StringArray::fromLines(pending);
        if (!pending.endsWithChar('\n') && lines.size() > 0)
        {
            pending = lines[lines.size() - 1];
            lines.remove(lines.size() - 1);
        }
        else
        {
            pending.clear();
        }

        for (const auto& line : lines)
        {
            if (line.containsIgnoreCase("download"))
            {
                std::scoped_lock lock(stateMutex);
                status = "Preparing bundled AI model...";
                progress.store(0.15f);
            }
            else if (line.containsIgnoreCase("separat") || line.containsIgnoreCase("segment"))
            {
                std::scoped_lock lock(stateMutex);
                status = "Separating vocals and instrumental...";
                progress.store(0.55f);
            }
            else if (line.containsIgnoreCase("writ") || line.containsIgnoreCase("save"))
            {
                progress.store(0.88f);
            }
        }
        juce::Thread::sleep(75);
    }

    if (shouldStop.load())
    {
        process.kill();
        busy.store(false);
        return;
    }

    const auto exitCode = process.getExitCode();
    const auto karaoke = cacheDir.getChildFile("karaoke.wav");
    if (!instrumental.existsAsFile() && karaoke.existsAsFile())
        karaoke.moveFileTo(instrumental);

    if (exitCode != 0 || !vocals.existsAsFile() || !instrumental.existsAsFile())
    {
        std::scoped_lock lock(stateMutex);
        status = "Stem separation failed (engine code " + juce::String(exitCode) + ")";
        busy.store(false);
        return;
    }

    if (loadCachedStems(vocals, instrumental))
        progress.store(1.0f);
    busy.store(false);
}

bool StemEngine::loadCachedStems(const juce::File& vocalsFile, const juce::File& instrumentalFile)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    auto vocalReader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(vocalsFile));
    auto instrumentalReader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(instrumentalFile));

    if (vocalReader == nullptr || instrumentalReader == nullptr)
    {
        std::scoped_lock lock(stateMutex);
        status = "Could not read separated WAV files";
        return false;
    }

    const auto vocalRate = vocalReader->sampleRate;
    const auto instrumentalRate = instrumentalReader->sampleRate;
    if (std::abs(vocalRate - instrumentalRate) > 1.0)
    {
        std::scoped_lock lock(stateMutex);
        status = "Separated stems have mismatched sample rates";
        return false;
    }

    auto newVocals = std::make_unique<juce::AudioFormatReaderSource>(vocalReader.release(), true);
    auto newInstrumental = std::make_unique<juce::AudioFormatReaderSource>(instrumentalReader.release(), true);

    stemsReady.store(false);
    {
        std::scoped_lock lock(stateMutex);
        clearStemSources();
        vocalsReaderSource = std::move(newVocals);
        instrumentalReaderSource = std::move(newInstrumental);
        stemSampleRate = vocalRate;

        constexpr int readAheadSamples = 262144;
        vocalsTransport.setSource(vocalsReaderSource.get(), readAheadSamples, &readAheadThread, stemSampleRate, 2);
        instrumentalTransport.setSource(instrumentalReaderSource.get(), readAheadSamples, &readAheadThread, stemSampleRate, 2);
        vocalsTransport.prepareToPlay(currentBlockSize, currentSampleRate);
        instrumentalTransport.prepareToPlay(currentBlockSize, currentSampleRate);
        vocalsTransport.setPosition(0.0);
        instrumentalTransport.setPosition(0.0);
        vocalsTransport.start();
        instrumentalTransport.start();
        expectedHostSamplePosition.store(-1);
        lastRenderedMode.store(-1);
        status = "Stems ready - choose Acapella or Instrumental, then click Audition Apply";
    }

    stemsReady.store(true);
    return true;
}

void StemEngine::process(juce::AudioBuffer<float>& buffer, StemMode mode, juce::int64 hostSamplePosition)
{
    const auto modeIndex = static_cast<int>(mode);
    if (mode == StemMode::original)
    {
        expectedHostSamplePosition.store(-1);
        lastRenderedMode.store(modeIndex);
        return;
    }

    if (!stemsReady.load() || currentSampleRate <= 0.0)
    {
        buffer.clear();
        expectedHostSamplePosition.store(-1);
        return;
    }

    std::unique_lock<std::mutex> lock(stateMutex, std::try_to_lock);
    if (!lock.owns_lock())
    {
        buffer.clear();
        return;
    }

    auto& transport = (mode == StemMode::vocals) ? vocalsTransport : instrumentalTransport;

    if (hostSamplePosition >= 0)
    {
        const auto relativeSample = hostSamplePosition - timelineOffsetSamples.load();
        if (relativeSample < 0)
        {
            buffer.clear();
            expectedHostSamplePosition.store(-1);
            lastRenderedMode.store(modeIndex);
            return;
        }

        const auto expected = expectedHostSamplePosition.load();
        const auto modeChanged = lastRenderedMode.load() != modeIndex;
        const auto toleranceSamples = static_cast<juce::int64>(juce::jmax(buffer.getNumSamples() * 2, 2048));
        const auto hostJumped = expected < 0 || std::llabs(hostSamplePosition - expected) > toleranceSamples;

        if (modeChanged || hostJumped)
        {
            const double targetSeconds = static_cast<double>(relativeSample) / currentSampleRate;
            transport.setPosition(targetSeconds);
        }

        expectedHostSamplePosition.store(hostSamplePosition + buffer.getNumSamples());
        lastRenderedMode.store(modeIndex);
    }
    else
    {
        expectedHostSamplePosition.store(-1);
        lastRenderedMode.store(modeIndex);
    }

    juce::AudioSourceChannelInfo info(&buffer, 0, buffer.getNumSamples());
    transport.getNextAudioBlock(info);
}

juce::String StemEngine::getStatus() const
{
    std::scoped_lock lock(stateMutex);
    return status;
}
