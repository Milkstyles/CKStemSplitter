#include "PluginEditor.h"

CKStemSplitterAudioProcessorEditor::CKStemSplitterAudioProcessorEditor(CKStemSplitterAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), progressBar(progressValue)
{
    setSize(620, 430);

    titleLabel.setText("CK STEM SPLITTER", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(30.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("AUDITION OFFLINE SELECTION EFFECT", juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(13.0f));
    subtitleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(subtitleLabel);

    instructionLabel.setText("Highlight audio, choose a stem, and wait - no playback or file dialogs",
                             juce::dontSendNotification);
    instructionLabel.setFont(juce::Font(12.0f));
    instructionLabel.setJustificationType(juce::Justification::centred);
    instructionLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(instructionLabel);

    captureButton.onClick = [this]
    {
        processor.startAutomatedWorkflow(1, getPeer() != nullptr ? getPeer()->getNativeHandle() : nullptr);
    };
    addAndMakeVisible(captureButton);

    stopSplitButton.onClick = [this]
    {
        processor.startAutomatedWorkflow(2, getPeer() != nullptr ? getPeer()->getNativeHandle() : nullptr);
    };
    addAndMakeVisible(stopSplitButton);

    loadPreparedButton.onClick = [this]
    {
        processor.checkAutomationRequestFromUi();
        processor.publishAutomationReadyFromUi();
    };
    addAndMakeVisible(loadPreparedButton);

    modeBox.addItem("Original", 1);
    modeBox.addItem("Acapella", 2);
    modeBox.addItem("Instrumental", 3);
    addAndMakeVisible(modeBox);

    outputGainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    outputGainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 24);
    outputGainSlider.setTextValueSuffix(" dB");
    addAndMakeVisible(outputGainSlider);

    outputGainLabel.setText("OUTPUT", juce::dontSendNotification);
    outputGainLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(outputGainLabel);

    progressBar.setPercentageDisplay(true);
    addAndMakeVisible(progressBar);

    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    statusLabel.setMinimumHorizontalScale(0.72f);
    addAndMakeVisible(statusLabel);

    modeAttachment = std::make_unique<ComboAttachment>(processor.getAPVTS(), "mode", modeBox);
    gainAttachment = std::make_unique<SliderAttachment>(processor.getAPVTS(), "outputGain", outputGainSlider);

}

void CKStemSplitterAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(16, 16, 20));

    auto panel = getLocalBounds().reduced(20).toFloat();
    g.setColour(juce::Colour::fromRGB(30, 30, 38));
    g.fillRoundedRectangle(panel, 14.0f);

    g.setColour(juce::Colour::fromRGB(210, 35, 45));
    g.fillRoundedRectangle(45.0f, 102.0f, 530.0f, 4.0f, 2.0f);

    if (processor.isCapturingSelection())
    {
        g.setColour(juce::Colour::fromRGB(210, 35, 45));
        g.drawRoundedRectangle(45.0f, 148.0f, 530.0f, 58.0f, 7.0f, 2.0f);
    }
}

void CKStemSplitterAudioProcessorEditor::resized()
{
    titleLabel.setBounds(50, 30, 520, 42);
    subtitleLabel.setBounds(50, 70, 520, 22);
    instructionLabel.setBounds(40, 112, 540, 28);

    captureButton.setBounds(55, 158, 240, 40);
    stopSplitButton.setBounds(325, 158, 240, 40);

    loadPreparedButton.setBounds(55, 215, 510, 40);
    modeBox.setBounds(55, 270, 300, 38);

    outputGainLabel.setBounds(425, 257, 120, 22);
    outputGainSlider.setBounds(430, 280, 110, 80);

    progressBar.setBounds(55, 325, 300, 22);
    statusLabel.setBounds(45, 365, 530, 42);
}
