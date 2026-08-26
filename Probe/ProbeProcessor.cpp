#include "ProbeProcessor.h"

namespace
{
class StemPrototypeEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit StemPrototypeEditor(CKAuditionOfflineProbeProcessor& owner)
        : AudioProcessorEditor(owner), processor(owner)
    {
        setSize(620, 390);
        setOpaque(true);
        addAndMakeVisible(acapellaButton);
        addAndMakeVisible(instrumentalButton);
        acapellaButton.onClick = [this]
        {
            processor.setOutputMode(CKAuditionOfflineProbeProcessor::OutputMode::acapella);
            repaint();
        };
        instrumentalButton.onClick = [this]
        {
            processor.setOutputMode(CKAuditionOfflineProbeProcessor::OutputMode::instrumental);
            repaint();
        };
        startTimer(25);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour::fromRGB(16, 17, 21));
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(26.0f, juce::Font::bold)));
        g.drawText("CK STEM SPLITTER", 28, 20, getWidth() - 56, 40,
                   juce::Justification::centred);
        g.setColour(juce::Colour::fromRGB(220, 40, 53));
        g.fillRect(35, 70, getWidth() - 70, 4);
        g.setColour(juce::Colours::white);
        g.setFont(17.0f);
        g.drawFittedText("Choose the result, then click Audition's Apply button.\n"
                         "No playback, capture, Save As, or extension is used.",
                         45, 92, getWidth() - 90, 58, juce::Justification::centred, 3);
        const auto acapella = processor.getOutputMode()
            == CKAuditionOfflineProbeProcessor::OutputMode::acapella;
        g.setColour(acapella ? juce::Colour::fromRGB(235, 92, 101)
                             : juce::Colour::fromRGB(190, 193, 202));
        g.drawText(acapella ? "Selected: ACAPELLA" : "Selected: INSTRUMENTAL",
                   45, 218, getWidth() - 90, 30, juce::Justification::centred);
        g.setColour(juce::Colour::fromRGB(190, 193, 202));
        g.setFont(13.5f);
        g.drawFittedText(processor.getReportText(), 45, 260, getWidth() - 90, 82,
                         juce::Justification::topLeft, 5);
    }

    void resized() override
    {
        acapellaButton.setBounds(55, 166, 245, 42);
        instrumentalButton.setBounds(320, 166, 245, 42);
    }

private:
    void timerCallback() override
    {
        if (auto* peer = getPeer())
        {
            peer->setCurrentRenderingEngine(0);
            stopTimer();
        }
    }
    CKAuditionOfflineProbeProcessor& processor;
    juce::TextButton acapellaButton { "MAKE ACAPELLA" };
    juce::TextButton instrumentalButton { "MAKE INSTRUMENTAL" };
};

juce::File engineExecutable()
{
    const auto overridePath = juce::SystemStats::getEnvironmentVariable("CK_STEM_ENGINE_PATH", {});
    if (overridePath.isNotEmpty()) return juce::File(overridePath);
    return juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
        .getChildFile("Commercial Kings").getChildFile("CK Stem Splitter")
        .getChildFile("engine").getChildFile("ckstem-engine")
        .getChildFile("ckstem-engine.exe");
}

juce::File modelCacheDirectory()
{
    const auto overridePath = juce::SystemStats::getEnvironmentVariable("CK_STEM_MODEL_CACHE", {});
    if (overridePath.isNotEmpty()) return juce::File(overridePath);
    return juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
        .getChildFile("Commercial Kings").getChildFile("CK Stem Splitter")
        .getChildFile("engine").getChildFile("models");
}
}

CKAuditionOfflineProbeProcessor::CKAuditionOfflineProbeProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}

CKAuditionOfflineProbeProcessor::~CKAuditionOfflineProbeProcessor()
{
    stopPersistentWorker();
    appendSnapshot("destroy");
}

void CKAuditionOfflineProbeProcessor::prepareToPlay(double sampleRate, int)
{
    stopPersistentWorker();
    activeSampleRate = sampleRate;
    modelWindowSamples = juce::jmax(1, juce::roundToInt(sampleRate * modelWindowSeconds));
    modelStrideSamples = modelWindowSamples
        - juce::roundToInt(modelWindowSamples * overlapFraction);
    setLatencySamples(modelWindowSamples);
    resetStreamState();
    sessionDirectory = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("Commercial Kings").getChildFile("CK Stem Splitter")
        .getChildFile("OfflineVST").getNonexistentChildFile("render", {}, false);
    sessionDirectory.createDirectory();
    appendSnapshot("prepare");
}

void CKAuditionOfflineProbeProcessor::releaseResources()
{
    stopPersistentWorker();
    appendSnapshot("release");
}

void CKAuditionOfflineProbeProcessor::resetStreamState()
{
    inputLeft.clear(); inputRight.clear(); vocalLeft.clear(); vocalRight.clear(); vocalWeight.clear();
    streamPosition = 0; nextChunkStart = 0; completedChunks.store(0); engineFailed.store(false);
    observedNonRealtime.store(false); totalBlocks.store(0); totalSamples.store(0); lastEngineError.clear();
    persistentWorkerAttempted = false; persistentWorkerReady = false;
}

bool CKAuditionOfflineProbeProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return input == output && (output == juce::AudioChannelSet::mono()
                               || output == juce::AudioChannelSet::stereo());
}

void CKAuditionOfflineProbeProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto samples = buffer.getNumSamples();
    const auto channels = buffer.getNumChannels();
    const auto* left = buffer.getReadPointer(0);
    const auto* right = buffer.getReadPointer(juce::jmin(1, channels - 1));
    inputLeft.insert(inputLeft.end(), left, left + samples);
    inputRight.insert(inputRight.end(), right, right + samples);
    observedNonRealtime.store(observedNonRealtime.load() || isNonRealtime());
    totalBlocks.fetch_add(1); totalSamples.fetch_add(samples);

    // Audition 26.3 inconsistently labels destructive Apply as realtime when
    // a VST3 reports a large model latency. The proven host signal is the
    // actual buffered render stream, so do not gate inference on that flag.
    if (modelProcessingEnabled.load() && !engineFailed.load())
        while (nextChunkStart + modelWindowSamples <= static_cast<juce::int64>(inputLeft.size()))
        {
            if (!processNextModelChunk()) { engineFailed.store(true); break; }
            nextChunkStart += modelStrideSamples;
        }

    const auto mode = getOutputMode();
    for (int sample = 0; sample < samples; ++sample)
    {
        const auto sourceIndex = streamPosition + sample - modelWindowSamples;
        for (int channel = 0; channel < channels; ++channel)
        {
            float result = 0.0f;
            if (sourceIndex >= 0 && sourceIndex < static_cast<juce::int64>(inputLeft.size()))
            {
                const auto index = static_cast<size_t>(sourceIndex);
                const auto original = channel == 0 ? inputLeft[index] : inputRight[index];
                if (engineFailed.load() || !modelProcessingEnabled.load()) result = original;
                else if (index < vocalWeight.size() && vocalWeight[index] > 1.0e-7f)
                {
                    const auto vocalSum = channel == 0 ? vocalLeft[index] : vocalRight[index];
                    const auto vocal = vocalSum / vocalWeight[index];
                    result = mode == OutputMode::acapella ? vocal : original - vocal;
                }
            }
            buffer.setSample(channel, sample, result);
        }
    }
    streamPosition += samples;
}

bool CKAuditionOfflineProbeProcessor::processNextModelChunk()
{
    const auto startedAt = juce::Time::getMillisecondCounterHiRes();
    const auto number = completedChunks.load();
    const auto dir = sessionDirectory.getChildFile("chunk-" + juce::String(number));
    dir.createDirectory();
    const auto input = dir.getChildFile("input.wav");
    const auto output = dir.getChildFile("output"); output.createDirectory();
    if (!writeChunkInput(input, nextChunkStart))
    { lastEngineError = "Could not write temporary model chunk"; appendSnapshot("engine-error", lastEngineError); return false; }
    const auto engine = engineExecutable();
    if (!engine.existsAsFile())
    { lastEngineError = "Self-contained stem engine is not installed"; appendSnapshot("engine-error", lastEngineError); return false; }

    auto usedPersistentWorker = ensurePersistentWorker();
    auto processed = usedPersistentWorker && runChunkThroughPersistentWorker(dir);
    if (!processed && usedPersistentWorker)
    {
        appendSnapshot("worker-fallback", lastEngineError);
        stopPersistentWorker();
        usedPersistentWorker = false;
    }
    if (!processed) processed = runChunkOneShot(input, output);
    if (!processed) return false;

    juce::AudioBuffer<float> vocals;
    if (!loadVocalChunk(output.getChildFile("vocals.wav"), vocals))
    { lastEngineError = "Engine produced no readable vocal chunk"; appendSnapshot("engine-error", lastEngineError); return false; }
    const auto required = static_cast<size_t>(nextChunkStart + modelWindowSamples);
    vocalLeft.resize(required, 0.0f); vocalRight.resize(required, 0.0f); vocalWeight.resize(required, 0.0f);
    for (int sample = 0; sample < modelWindowSamples; ++sample)
    {
        const auto destination = static_cast<size_t>(nextChunkStart + sample);
        const auto weight = windowWeight(sample);
        vocalLeft[destination] += vocals.getSample(0, sample) * weight;
        vocalRight[destination] += vocals.getSample(juce::jmin(1, vocals.getNumChannels() - 1), sample) * weight;
        vocalWeight[destination] += weight;
    }
    completedChunks.fetch_add(1);
    const auto elapsed = juce::Time::getMillisecondCounterHiRes() - startedAt;
    appendSnapshot("chunk-complete", "chunk=" + juce::String(number)
        + " mode=" + (usedPersistentWorker ? "persistent" : "one-shot")
        + " elapsedMs=" + juce::String(elapsed, 0));
    return true;
}

bool CKAuditionOfflineProbeProcessor::ensurePersistentWorker()
{
    if (persistentWorkerReady && persistentWorker != nullptr && persistentWorker->isRunning()) return true;
    if (persistentWorkerAttempted) return false;
    persistentWorkerAttempted = true;

    const auto engine = engineExecutable();
    if (!engine.existsAsFile()) return false;
    sessionDirectory.getChildFile("server.ready").deleteFile();
    sessionDirectory.getChildFile("server.error.txt").deleteFile();
    sessionDirectory.getChildFile("shutdown.ready").deleteFile();
    juce::StringArray args { engine.getFullPathName(), "serve", sessionDirectory.getFullPathName(),
        "--providers", "cpu", "--cache-dir", modelCacheDirectory().getFullPathName() };
    persistentWorker = std::make_unique<juce::ChildProcess>();
    if (!persistentWorker->start(args))
    {
        persistentWorker.reset();
        lastEngineError = "Could not start persistent stem engine";
        return false;
    }

    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 5.0 * 60.0 * 1000.0;
    const auto ready = sessionDirectory.getChildFile("server.ready");
    const auto error = sessionDirectory.getChildFile("server.error.txt");
    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        if (ready.existsAsFile())
        {
            persistentWorkerReady = true;
            appendSnapshot("worker-ready", "model retained in RAM");
            return true;
        }
        if (error.existsAsFile() || !persistentWorker->isRunning())
        {
            lastEngineError = error.existsAsFile() ? error.loadFileAsString().trim()
                                                   : "Persistent engine exited during startup";
            stopPersistentWorker();
            return false;
        }
        juce::Thread::sleep(10);
    }
    lastEngineError = "Persistent engine startup timed out";
    stopPersistentWorker();
    return false;
}

bool CKAuditionOfflineProbeProcessor::runChunkThroughPersistentWorker(const juce::File& dir)
{
    const auto request = dir.getChildFile("request.ready");
    const auto done = dir.getChildFile("done.ready");
    const auto error = dir.getChildFile("error.txt");
    done.deleteFile(); error.deleteFile(); request.deleteFile();
    if (!request.replaceWithText("ready"))
    {
        lastEngineError = "Could not submit chunk to persistent engine";
        return false;
    }

    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 5.0 * 60.0 * 1000.0;
    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        if (done.existsAsFile()) return true;
        if (error.existsAsFile())
        {
            lastEngineError = error.loadFileAsString().trim();
            return false;
        }
        if (persistentWorker == nullptr || !persistentWorker->isRunning())
        {
            lastEngineError = "Persistent engine stopped while processing";
            return false;
        }
        juce::Thread::sleep(10);
    }
    lastEngineError = "Persistent engine timed out";
    return false;
}

bool CKAuditionOfflineProbeProcessor::runChunkOneShot(const juce::File& input, const juce::File& output)
{
    const auto engine = engineExecutable();
    juce::StringArray args { engine.getFullPathName(), "separate", input.getFullPathName(),
        output.getFullPathName(), "--model", "htdemucs_ft_vocals", "--small",
        "--providers", "cpu", "--cache-dir", modelCacheDirectory().getFullPathName(), "--quiet" };
    juce::ChildProcess process;
    if (!process.start(args))
    { lastEngineError = "Could not start stem engine"; appendSnapshot("engine-error", lastEngineError); return false; }
    if (!process.waitForProcessToFinish(5 * 60 * 1000))
    { process.kill(); lastEngineError = "Stem engine timed out"; appendSnapshot("engine-error", lastEngineError); return false; }
    if (process.getExitCode() != 0)
    { lastEngineError = "Engine code " + juce::String(process.getExitCode()) + ": " + process.readAllProcessOutput().trim(); appendSnapshot("engine-error", lastEngineError); return false; }
    return true;
}

void CKAuditionOfflineProbeProcessor::stopPersistentWorker()
{
    if (persistentWorker == nullptr)
    {
        persistentWorkerReady = false;
        return;
    }
    if (persistentWorker->isRunning())
    {
        sessionDirectory.getChildFile("shutdown.ready").replaceWithText("shutdown");
        if (!persistentWorker->waitForProcessToFinish(1500)) persistentWorker->kill();
    }
    persistentWorker.reset();
    persistentWorkerReady = false;
}

bool CKAuditionOfflineProbeProcessor::writeChunkInput(const juce::File& file, juce::int64 start) const
{
    auto stream = file.createOutputStream();
    if (stream == nullptr || !stream->openedOk()) return false;
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(), activeSampleRate, 2, 32, {}, 0));
    if (writer == nullptr) return false;
    juce::AudioBuffer<float> chunk(2, modelWindowSamples);
    for (int sample = 0; sample < modelWindowSamples; ++sample)
    {
        const auto source = static_cast<size_t>(start + sample);
        chunk.setSample(0, sample, inputLeft[source]); chunk.setSample(1, sample, inputRight[source]);
    }
    return writer->writeFromAudioSampleBuffer(chunk, 0, modelWindowSamples);
}

bool CKAuditionOfflineProbeProcessor::loadVocalChunk(const juce::File& file, juce::AudioBuffer<float>& destination) const
{
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    auto reader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(file));
    if (reader == nullptr || reader->lengthInSamples < modelWindowSamples) return false;
    destination.setSize(juce::jmax(1, static_cast<int>(reader->numChannels)), modelWindowSamples);
    return reader->read(&destination, 0, modelWindowSamples, 0, true, true);
}

float CKAuditionOfflineProbeProcessor::windowWeight(int sample) const noexcept
{
    const auto overlap = modelWindowSamples - modelStrideSamples;
    if (overlap <= 1) return 1.0f;
    if (sample < overlap) return static_cast<float>(sample) / static_cast<float>(overlap - 1);
    if (sample >= modelWindowSamples - overlap)
        return static_cast<float>(modelWindowSamples - 1 - sample) / static_cast<float>(overlap - 1);
    return 1.0f;
}

juce::AudioProcessorEditor* CKAuditionOfflineProbeProcessor::createEditor() { return new StemPrototypeEditor(*this); }
void CKAuditionOfflineProbeProcessor::getStateInformation(juce::MemoryBlock& destination)
{ juce::MemoryOutputStream stream(destination, false); stream.writeInt(selectedMode.load()); }
void CKAuditionOfflineProbeProcessor::setStateInformation(const void* data, int size)
{ if (size >= 4) { juce::MemoryInputStream stream(data, static_cast<size_t>(size), false); selectedMode.store(juce::jlimit(0, 1, stream.readInt())); } }

juce::File CKAuditionOfflineProbeProcessor::getLogFile() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Commercial Kings").getChildFile("CK Stem Splitter")
        .getChildFile("audition-offline-probe.log");
}

juce::String CKAuditionOfflineProbeProcessor::getReportText() const
{
    auto text = "Audition offline flag: " + juce::String(observedNonRealtime.load() ? "yes" : "no (ignored)")
        + "    Model window: 7.8 seconds\nProcessed chunks: " + juce::String(completedChunks.load())
        + "    Host blocks: " + juce::String(totalBlocks.load());
    return text + (engineFailed.load() ? "\nENGINE ERROR: " + lastEngineError
                                      : "\nThe AI engine runs outside Audition for crash isolation.");
}

void CKAuditionOfflineProbeProcessor::appendSnapshot(const juce::String& event, const juce::String& detail)
{
    const auto log = getLogFile(); log.getParentDirectory().createDirectory();
    juce::FileOutputStream stream(log); if (!stream.openedOk()) return; stream.setPosition(log.getSize());
    stream.writeText(juce::Time::getCurrentTime().toISO8601(true) + " event=" + event
        + " sampleRate=" + juce::String(activeSampleRate, 1) + " latency=" + juce::String(modelWindowSamples)
        + " blocks=" + juce::String(totalBlocks.load()) + " samples=" + juce::String(totalSamples.load())
        + " chunks=" + juce::String(completedChunks.load()) + " offline="
        + juce::String(observedNonRealtime.load() ? "yes" : "no")
        + (detail.isNotEmpty() ? " detail=" + detail : juce::String()) + "\n", false, false, "\n");
    stream.flush();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CKAuditionOfflineProbeProcessor(); }
