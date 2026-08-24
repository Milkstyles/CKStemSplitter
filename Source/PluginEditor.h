#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class CKStemSplitterAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit CKStemSplitterAudioProcessorEditor(CKStemSplitterAudioProcessor&);
    ~CKStemSplitterAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    CKStemSplitterAudioProcessor& processor;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label instructionLabel;
    juce::TextButton captureButton { "MAKE ACAPELLA" };
    juce::TextButton stopSplitButton { "MAKE INSTRUMENTAL" };
    juce::TextButton loadPreparedButton { "LOAD PREPARED STEM" };
    juce::ComboBox modeBox;
    juce::Slider outputGainSlider;
    juce::Label outputGainLabel;
    juce::ProgressBar progressBar;
    double progressValue = 0.0;
    juce::Label statusLabel;

    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<ComboAttachment> modeAttachment;
    std::unique_ptr<SliderAttachment> gainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CKStemSplitterAudioProcessorEditor)
};
