#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class CKStemSplitterAudioProcessorEditor : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit CKStemSplitterAudioProcessorEditor(CKStemSplitterAudioProcessor&);
    ~CKStemSplitterAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    CKStemSplitterAudioProcessor& processor;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label instructionLabel;
    juce::TextButton captureButton { "LEGACY REAL-TIME CAPTURE" };
    juce::TextButton stopSplitButton { "STOP & SPLIT" };
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
