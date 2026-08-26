#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>

class CKAuditionOfflineProbeProcessor final : public juce::AudioProcessor
{
public:
    enum class OutputMode { acapella = 0, instrumental = 1 };

    CKAuditionOfflineProbeProcessor();
    ~CKAuditionOfflineProbeProcessor() override;
    void prepareToPlay(double sampleRate, int maximumBlockSize) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return modelWindowSeconds; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& destination) override;
    void setStateInformation(const void* data, int size) override;

    void setOutputMode(OutputMode mode) noexcept { selectedMode.store(static_cast<int>(mode)); }
    OutputMode getOutputMode() const noexcept { return static_cast<OutputMode>(selectedMode.load()); }
    void setModelProcessingEnabledForTests(bool enabled) noexcept { modelProcessingEnabled.store(enabled); }
    juce::String getReportText() const;
    juce::File getLogFile() const;

private:
    static constexpr double modelWindowSeconds = 7.8;
    static constexpr double overlapFraction = 0.25;
    bool processNextModelChunk();
    bool ensurePersistentWorker();
    bool runChunkThroughPersistentWorker(const juce::File& chunkDirectory);
    bool runChunkOneShot(const juce::File& input, const juce::File& output);
    void stopPersistentWorker();
    bool writeChunkInput(const juce::File& file, juce::int64 startSample) const;
    bool loadVocalChunk(const juce::File& file, juce::AudioBuffer<float>& destination) const;
    float windowWeight(int sample) const noexcept;
    void appendSnapshot(const juce::String& event, const juce::String& detail = {});
    void resetStreamState();

    std::vector<float> inputLeft, inputRight, vocalLeft, vocalRight, vocalWeight;
    int modelWindowSamples = 0;
    int modelStrideSamples = 0;
    juce::int64 streamPosition = 0;
    juce::int64 nextChunkStart = 0;
    double activeSampleRate = 0.0;
    juce::File sessionDirectory;
    std::unique_ptr<juce::ChildProcess> persistentWorker;
    bool persistentWorkerAttempted = false;
    bool persistentWorkerReady = false;
    std::atomic<int> selectedMode { static_cast<int>(OutputMode::acapella) };
    std::atomic<int> completedChunks { 0 };
    std::atomic<bool> engineFailed { false };
    std::atomic<bool> observedNonRealtime { false };
    std::atomic<bool> modelProcessingEnabled { true };
    std::atomic<juce::int64> totalBlocks { 0 };
    std::atomic<juce::int64> totalSamples { 0 };
    juce::String lastEngineError;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CKAuditionOfflineProbeProcessor)
};
