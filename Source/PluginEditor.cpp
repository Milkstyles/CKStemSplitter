#include "PluginEditor.h"

CKStemSplitterAudioProcessorEditor::CKStemSplitterAudioProcessorEditor(CKStemSplitterAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), progressBar(progressValue)
{
    setSize(620, 430);

    titleLabel.setText("CK STEM SPLITTER", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(30.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("FAST VOCAL / INSTRUMENTAL SEPARATION", juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(13.0f));
    subtitleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(subtitleLabel);

    loadButton.onClick = [this] { chooseAudioFile(); };
    addAndMakeVisible(loadButton);

    fileLabel.setText("No audio file selected", juce::dontSendNotification);
    fileLabel.setJustificationType(juce::Justification::centredLeft);
    fileLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(fileLabel);

    modeBox.addItem("Original", 1);
    modeBox.addItem("Vocals", 2);
    modeBox.addItem("Instrumental", 3);
    addAndMakeVisible(modeBox);

    outputGainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    outputGainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 24);
    outputGainSlider.setTextValueSuffix(" dB");
    addAndMakeVisible(outputGainSlider);

    outputGainLabel.setText("OUTPUT", juce::dontSendNotification);
    outputGainLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(outputGainLabel);

    analyzeButton.onClick = [this]
    {
        processor.getStemEngine().startSeparation();
    };
    addAndMakeVisible(analyzeButton);

    progressBar.setPercentageDisplay(true);
    addAndMakeVisible(progressBar);

    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(statusLabel);

    modeAttachment = std::make_unique<ComboAttachment>(processor.getAPVTS(), "mode", modeBox);
    gainAttachment = std::make_unique<SliderAttachment>(processor.getAPVTS(), "outputGain", outputGainSlider);

    startTimerHz(12);
}

void CKStemSplitterAudioProcessorEditor::chooseAudioFile()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Choose a song to split",
        juce::File{},
        "*.wav;*.mp3;*.flac;*.ogg;*.aif;*.aiff");

    const auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync(flags, [this](const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (file.existsAsFile())
        {
            processor.getStemEngine().setSourceFile(file);
            fileLabel.setText(file.getFileName(), juce::dontSendNotification);
        }
    });
}

void CKStemSplitterAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(16, 16, 20));

    auto panel = getLocalBounds().reduced(20).toFloat();
    g.setColour(juce::Colour::fromRGB(30, 30, 38));
    g.fillRoundedRectangle(panel, 14.0f);

    g.setColour(juce::Colour::fromRGB(210, 35, 45));
    g.fillRoundedRectangle(45.0f, 102.0f, 530.0f, 4.0f, 2.0f);
}

void CKStemSplitterAudioProcessorEditor::resized()
{
    titleLabel.setBounds(50, 35, 520, 42);
    subtitleLabel.setBounds(50, 75, 520, 22);

    loadButton.setBounds(55, 125, 145, 38);
    fileLabel.setBounds(215, 125, 350, 38);

    modeBox.setBounds(55, 185, 300, 38);

    outputGainLabel.setBounds(425, 172, 120, 22);
    outputGainSlider.setBounds(430, 195, 110, 110);

    analyzeButton.setBounds(55, 250, 300, 48);
    progressBar.setBounds(55, 320, 490, 22);
    statusLabel.setBounds(55, 360, 510, 28);
}

void CKStemSplitterAudioProcessorEditor::timerCallback()
{
    auto& engine = processor.getStemEngine();
    progressValue = engine.getProgress();
    statusLabel.setText(engine.getStatus(), juce::dontSendNotification);
    analyzeButton.setEnabled(!engine.isBusy() && engine.hasSourceFile());
    loadButton.setEnabled(!engine.isBusy());
}
