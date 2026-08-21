#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
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
    void chooseAudioFile();

    CKStemSplitterAudioProcessor& processor;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::TextButton loadButton { "LOAD AUDIO" };
    juce::Label fileLabel;
    juce::ComboBox modeBox;
    juce::Slider outputGainSlider;
    juce::Label outputGainLabel;
    juce::TextButton analyzeButton { "SPLIT STEMS" };
    juce::ProgressBar progressBar;
    double progressValue = 0.0;
    juce::Label statusLabel;

    std::unique_ptr<juce::FileChooser> fileChooser;

    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<ComboAttachment> modeAttachment;
    std::unique_ptr<SliderAttachment> gainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CKStemSplitterAudioProcessorEditor)
};
