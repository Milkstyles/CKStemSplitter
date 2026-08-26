#include "ProbeProcessor.h"

#include <cmath>
#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    constexpr int expectedLatency = 374400;

    CKAuditionOfflineProbeProcessor processor;
    processor.setModelProcessingEnabledForTests(false);
    // Realtime preview intentionally uses the latency-aligned dry signal;
    // the expensive external model is only allowed during offline Apply.
    processor.setNonRealtime(false);
    processor.prepareToPlay(sampleRate, blockSize);

    if (processor.getLatencySamples() != expectedLatency)
    {
        std::cerr << "Wrong latency: " << processor.getLatencySamples() << '\n';
        return 1;
    }
    if (std::abs(processor.getTailLengthSeconds() - 7.8) > 0.000001)
    {
        std::cerr << "Wrong tail: " << processor.getTailLengthSeconds() << '\n';
        return 2;
    }

    juce::MidiBuffer midi;
    float sampleAtExpectedLatency = 0.0f;
    float maximumBeforeLatency = 0.0f;
    int absolutePosition = 0;
    const auto samplesToProcess = expectedLatency + blockSize;
    while (absolutePosition < samplesToProcess)
    {
        const auto count = juce::jmin(blockSize, samplesToProcess - absolutePosition);
        juce::AudioBuffer<float> audio(2, count);
        audio.clear();
        if (absolutePosition == 0)
        {
            audio.setSample(0, 0, 1.0f);
            audio.setSample(1, 0, 1.0f);
        }

        processor.processBlock(audio, midi);
        for (int sample = 0; sample < count; ++sample)
        {
            const auto position = absolutePosition + sample;
            if (position < expectedLatency)
                maximumBeforeLatency = juce::jmax(maximumBeforeLatency,
                                                   std::abs(audio.getSample(0, sample)));
            else if (position == expectedLatency)
                sampleAtExpectedLatency = audio.getSample(0, sample);
        }
        absolutePosition += count;
    }

    if (maximumBeforeLatency > 0.000001f)
    {
        std::cerr << "Probe emitted audio before its reported latency\n";
        return 3;
    }
    if (std::abs(sampleAtExpectedLatency - 1.0f) > 0.000001f)
    {
        std::cerr << "Latency-aligned dry impulse was " << sampleAtExpectedLatency << '\n';
        return 4;
    }
    if (!processor.getReportText().contains("Audition offline flag: no"))
    {
        std::cerr << "Realtime safety mode was not recorded\n";
        return 5;
    }

    std::cout << "PASS latencySamples=" << processor.getLatencySamples()
              << " tailSeconds=" << processor.getTailLengthSeconds()
              << " impulse=" << sampleAtExpectedLatency << '\n';
    return 0;
}
