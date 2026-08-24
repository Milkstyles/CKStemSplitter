#pragma once
#include <JuceHeader.h>
#include "StemEngine.h"

class CKStemSplitterAudioProcessor : public juce::AudioProcessor
{
public:
    CKStemSplitterAudioProcessor();
    ~CKStemSplitterAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    StemEngine& getStemEngine() noexcept { return stemEngine; }

    bool startSelectionCapture();
    void stopSelectionCaptureAndSplit();
    bool isCapturingSelection() const noexcept { return capturingSelection.load(); }
    juce::int64 getCapturedSamples() const noexcept { return capturedSamples.load(); }
    juce::String getCaptureStatus() const;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void setCaptureStatus(const juce::String& newStatus);

    juce::AudioProcessorValueTreeState apvts;
    StemEngine stemEngine;

    juce::TimeSliceThread captureWriterThread { "CK Selection Capture Writer" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> captureWriter;
    juce::SpinLock captureWriterLock;
    juce::File capturedSelectionFile;
    std::atomic<bool> capturingSelection { false };
    std::atomic<juce::int64> capturedSamples { 0 };
    std::atomic<juce::int64> captureStartHostSample { -1 };
    double captureSampleRate = 44100.0;
    int captureChannels = 2;

    mutable juce::CriticalSection captureStatusLock;
    juce::String captureStatus { "For no-playback splitting, open Window > Extensions > CK Stem Splitter" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CKStemSplitterAudioProcessor)
};
