#include "ProbeProcessor.h"

#include <cmath>
#include <iostream>

int main(int count, char** values)
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    if (count != 4)
    {
        std::cerr << "Usage: CKStemPrototypeRenderTest <input.wav> <output.wav> <acapella|instrumental>\n";
        return 2;
    }

    const juce::File inputFile(values[1]);
    const juce::File outputFile(values[2]);
    const juce::String mode(values[3]);
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    auto reader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(inputFile));
    if (reader == nullptr || reader->lengthInSamples <= 0
        || reader->lengthInSamples > std::numeric_limits<int>::max())
        return 3;

    const auto sourceSamples = static_cast<int>(reader->lengthInSamples);
    const auto channels = juce::jlimit(1, 2, static_cast<int>(reader->numChannels));
    juce::AudioBuffer<float> source(channels, sourceSamples);
    if (!reader->read(&source, 0, sourceSamples, 0, true, true))
        return 4;

    constexpr int blockSize = 1024;
    CKAuditionOfflineProbeProcessor processor;
    // Match Audition 26.3's observed Apply behavior with 7.8 s latency:
    // it supplies the fast render stream but leaves this flag false.
    processor.setNonRealtime(false);
    processor.setOutputMode(mode.equalsIgnoreCase("instrumental")
        ? CKAuditionOfflineProbeProcessor::OutputMode::instrumental
        : CKAuditionOfflineProbeProcessor::OutputMode::acapella);
    processor.prepareToPlay(reader->sampleRate, blockSize);
    const auto latency = processor.getLatencySamples();

    juce::AudioBuffer<float> rendered(channels, sourceSamples);
    rendered.clear();
    juce::MidiBuffer midi;
    const auto totalInput = sourceSamples + latency;
    for (int position = 0; position < totalInput; position += blockSize)
    {
        const auto blockSamples = juce::jmin(blockSize, totalInput - position);
        juce::AudioBuffer<float> block(channels, blockSamples);
        block.clear();
        const auto available = juce::jmax(0, juce::jmin(blockSamples, sourceSamples - position));
        for (int channel = 0; channel < channels; ++channel)
            if (available > 0)
                block.copyFrom(channel, 0, source, channel, position, available);
        processor.processBlock(block, midi);

        const auto outputStart = juce::jmax(0, position - latency);
        const auto blockOffset = juce::jmax(0, latency - position);
        const auto outputCount = juce::jmin(blockSamples - blockOffset,
                                            sourceSamples - outputStart);
        if (outputCount > 0)
            for (int channel = 0; channel < channels; ++channel)
                rendered.copyFrom(channel, outputStart, block, channel, blockOffset, outputCount);
    }
    processor.releaseResources();

    outputFile.deleteFile();
    auto stream = outputFile.createOutputStream();
    if (stream == nullptr || !stream->openedOk())
        return 5;
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(
        stream.release(), reader->sampleRate, static_cast<unsigned int>(channels), 32, {}, 0));
    if (writer == nullptr || !writer->writeFromAudioSampleBuffer(rendered, 0, sourceSamples))
        return 6;
    writer.reset();

    double outputEnergy = 0.0;
    double differenceEnergy = 0.0;
    for (int channel = 0; channel < channels; ++channel)
        for (int sample = 0; sample < sourceSamples; ++sample)
        {
            const auto output = rendered.getSample(channel, sample);
            const auto difference = output - source.getSample(channel, sample);
            outputEnergy += output * output;
            differenceEnergy += difference * difference;
        }
    const auto divisor = static_cast<double>(channels) * sourceSamples;
    const auto outputRms = std::sqrt(outputEnergy / divisor);
    const auto differenceRms = std::sqrt(differenceEnergy / divisor);
    std::cout << "PASS mode=" << mode << " samples=" << sourceSamples
              << " latency=" << latency << " outputRms=" << outputRms
              << " differenceRms=" << differenceRms << '\n';
    std::cout << processor.getReportText() << '\n';
    return outputRms > 1.0e-5 && differenceRms > 1.0e-5
        && processor.getReportText().contains("Processed chunks:") ? 0 : 7;
}
