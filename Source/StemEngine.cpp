#include "StemEngine.h"
#include <limits>

StemEngine::StemEngine()
{
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
}

void StemEngine::prepare(double sampleRate, int samplesPerBlock, int channels)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    currentChannels = channels;
    expectedHostSamplePosition.store(-1);
    lastRenderedMode.store(-1);

}

void StemEngine::reset()
{
    expectedHostSamplePosition.store(-1);
    lastRenderedMode.store(-1);
}

void StemEngine::clearStemSources()
{
    vocalsBuffer.setSize(0, 0);
    instrumentalBuffer.setSize(0, 0);
    sequentialReadPosition = 0;
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

bool StemEngine::loadPreparedStems(const juce::File& vocalsFile, const juce::File& instrumentalFile)
{
    progress.store(0.9f);
    const auto loaded = loadCachedStems(vocalsFile, instrumentalFile);
    progress.store(loaded ? 1.0f : 0.0f);
    busy.store(false);
    return loaded;
}

bool StemEngine::loadPreparedStem(const juce::File& stemFile, StemMode mode)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    auto reader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(stemFile));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->lengthInSamples > std::numeric_limits<int>::max())
    {
        std::scoped_lock lock(stateMutex);
        status = "Could not read the prepared stem WAV";
        return false;
    }

    juce::AudioBuffer<float> loaded(static_cast<int>(reader->numChannels),
                                    static_cast<int>(reader->lengthInSamples));
    if (!reader->read(&loaded, 0, loaded.getNumSamples(), 0, true, true))
    {
        std::scoped_lock lock(stateMutex);
        status = "Could not load the prepared stem WAV";
        return false;
    }

    stemsReady.store(false);
    {
        std::scoped_lock lock(stateMutex);
        clearStemSources();
        stemSampleRate = reader->sampleRate;
        if (mode == StemMode::instrumental)
            instrumentalBuffer = std::move(loaded);
        else
            vocalsBuffer = std::move(loaded);
        status = mode == StemMode::instrumental ? "Instrumental ready" : "Acapella ready";
    }
    progress.store(1.0f);
    busy.store(false);
    stemsReady.store(true);
    return true;
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

    if (vocalReader->lengthInSamples <= 0 || instrumentalReader->lengthInSamples <= 0
        || vocalReader->lengthInSamples > std::numeric_limits<int>::max()
        || instrumentalReader->lengthInSamples > std::numeric_limits<int>::max())
    {
        std::scoped_lock lock(stateMutex);
        status = "Separated WAV files are too large to load safely";
        return false;
    }

    juce::AudioBuffer<float> newVocals(static_cast<int>(vocalReader->numChannels),
                                       static_cast<int>(vocalReader->lengthInSamples));
    juce::AudioBuffer<float> newInstrumental(static_cast<int>(instrumentalReader->numChannels),
                                             static_cast<int>(instrumentalReader->lengthInSamples));
    if (!vocalReader->read(&newVocals, 0, newVocals.getNumSamples(), 0, true, true)
        || !instrumentalReader->read(&newInstrumental, 0, newInstrumental.getNumSamples(), 0, true, true))
    {
        std::scoped_lock lock(stateMutex);
        status = "Could not load separated WAV files";
        return false;
    }

    stemsReady.store(false);
    {
        std::scoped_lock lock(stateMutex);
        clearStemSources();
        vocalsBuffer = std::move(newVocals);
        instrumentalBuffer = std::move(newInstrumental);
        stemSampleRate = vocalRate;
        expectedHostSamplePosition.store(-1);
        lastRenderedMode.store(-1);
        sequentialReadPosition = 0;
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

    auto& stem = (mode == StemMode::vocals) ? vocalsBuffer : instrumentalBuffer;
    if (stem.getNumChannels() <= 0 || stem.getNumSamples() <= 0)
    {
        buffer.clear();
        return;
    }

    juce::int64 sourceStart = sequentialReadPosition;
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

        sourceStart = relativeSample;
        expectedHostSamplePosition.store(hostSamplePosition + buffer.getNumSamples());
        lastRenderedMode.store(modeIndex);
    }
    else
    {
        expectedHostSamplePosition.store(-1);
        lastRenderedMode.store(modeIndex);
    }

    if (sourceStart < 0 || sourceStart >= stem.getNumSamples())
    {
        buffer.clear();
        return;
    }

    const auto available = juce::jmin(buffer.getNumSamples(),
                                      stem.getNumSamples() - static_cast<int>(sourceStart));
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto sourceChannel = juce::jmin(channel, stem.getNumChannels() - 1);
        buffer.copyFrom(channel, 0, stem, sourceChannel, static_cast<int>(sourceStart), available);
        if (available < buffer.getNumSamples())
            buffer.clear(channel, available, buffer.getNumSamples() - available);
    }
    sequentialReadPosition = sourceStart + available;
}

juce::String StemEngine::getStatus() const
{
    std::scoped_lock lock(stateMutex);
    return status;
}

