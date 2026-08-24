#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <mutex>
#include <thread>

class StemEngine
{
public:
    enum class StemMode
    {
        original = 0,
        vocals = 1,
        instrumental = 2
    };

    StemEngine();
    ~StemEngine();

    void prepare(double sampleRate, int samplesPerBlock, int channels);
    void reset();

    void process(juce::AudioBuffer<float>& buffer, StemMode mode, juce::int64 hostSamplePosition);

    void setSourceFile(const juce::File& file);
    juce::File getSourceFile() const;
    bool hasSourceFile() const;
    void setTimelineOffsetSamples(juce::int64 offsetSamples) noexcept { timelineOffsetSamples.store(offsetSamples); }

    void startSeparation();
    bool isBusy() const noexcept { return busy.load(); }
    bool hasSeparatedStems() const noexcept { return stemsReady.load(); }
    float getProgress() const noexcept { return progress.load(); }

    juce::String getStatus() const;

private:
    juce::File getEngineExecutable() const;
    juce::File getModelCacheDirectory() const;
    juce::File getCacheDirectoryForSource(const juce::File&) const;
    void separationWorker();
    bool loadCachedStems(const juce::File& vocalsFile, const juce::File& instrumentalFile);
    void clearStemSources();

    mutable std::mutex stateMutex;
    juce::File sourceFile;

    juce::TimeSliceThread readAheadThread { "CK Stem Splitter Read Ahead" };
    std::unique_ptr<juce::AudioFormatReaderSource> vocalsReaderSource;
    std::unique_ptr<juce::AudioFormatReaderSource> instrumentalReaderSource;
    juce::AudioTransportSource vocalsTransport;
    juce::AudioTransportSource instrumentalTransport;

    double stemSampleRate = 44100.0;
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    int currentChannels = 2;
    std::atomic<juce::int64> timelineOffsetSamples { 0 };

    std::atomic<bool> busy { false };
    std::atomic<bool> stemsReady { false };
    std::atomic<float> progress { 0.0f };
    std::atomic<bool> shouldStop { false };
    std::thread workerThread;
    juce::String status { "Capture an Audition selection to split" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemEngine)
};
