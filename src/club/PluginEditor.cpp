#include "PluginEditor.h"

namespace
{
const juce::Colour kBackground { 0xffd9d5cc };
const juce::Colour kPanel { 0xffebe8e1 };
const juce::Colour kGraphite { 0xff2c2d2b };
const juce::Colour kMutedGraphite { 0xff676862 };
const juce::Colour kAccent { 0xff6c8275 };
}

ClubCraftPhase0AudioProcessorEditor::ClubCraftPhase0AudioProcessorEditor(
    ClubCraftPhase0AudioProcessor& processorToUse)
    : AudioProcessorEditor(&processorToUse), processor(processorToUse)
{
    setSize(520, 310);
    setResizable(true, true);
    setResizeLimits(420, 260, 900, 600);

    titleLabel.setText("CLUB CRAFT", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(20.0f).withKerningFactor(0.18f));
    titleLabel.setColour(juce::Label::textColourId, kGraphite);
    addAndMakeVisible(titleLabel);

    roleLabel.setText("ROLE", juce::dontSendNotification);
    roleLabel.setFont(juce::FontOptions(11.0f).withKerningFactor(0.10f));
    roleLabel.setColour(juce::Label::textColourId, kMutedGraphite);
    roleLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(roleLabel);

    roleSelector.addItem("SOURCE", 1);
    roleSelector.addItem("CLUB", 2);
    roleSelector.setJustificationType(juce::Justification::centred);
    roleSelector.setColour(juce::ComboBox::backgroundColourId, kPanel);
    roleSelector.setColour(juce::ComboBox::outlineColourId, kGraphite.withAlpha(0.18f));
    roleSelector.setColour(juce::ComboBox::textColourId, kGraphite);
    roleSelector.setColour(juce::ComboBox::arrowColourId, kGraphite);
    addAndMakeVisible(roleSelector);

    sessionLabel.setFont(juce::FontOptions(12.0f));
    sessionLabel.setColour(juce::Label::textColourId, kMutedGraphite);
    addAndMakeVisible(sessionLabel);

    connectionLabel.setFont(juce::FontOptions(12.0f));
    connectionLabel.setColour(juce::Label::textColourId, kAccent);
    addAndMakeVisible(connectionLabel);

    configureSlider(masterSlider, masterLabel, "MASTER");
    configureSlider(speakerSlider, speakerLabel, "PRIMARY SPEAKER");

    roleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.getParameters(), "role", roleSelector);
    masterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "masterLevel", masterSlider);
    speakerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "primarySpeakerLevel", speakerSlider);

    refreshStatus();
    startTimerHz(10);
}

ClubCraftPhase0AudioProcessorEditor::~ClubCraftPhase0AudioProcessorEditor()
{
    stopTimer();
}

void ClubCraftPhase0AudioProcessorEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(kBackground);

    const auto panel = getLocalBounds().reduced(16).withTrimmedTop(64);
    graphics.setColour(kPanel);
    graphics.fillRoundedRectangle(panel.toFloat(), 9.0f);

    graphics.setColour(kGraphite.withAlpha(0.12f));
    graphics.drawRoundedRectangle(panel.toFloat(), 9.0f, 1.0f);

    const auto dividerY = panel.getY() + panel.getHeight() / 2;
    graphics.drawLine(static_cast<float>(panel.getX() + 20), static_cast<float>(dividerY),
                      static_cast<float>(panel.getRight() - 20), static_cast<float>(dividerY), 1.0f);
}

void ClubCraftPhase0AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(24);
    auto header = bounds.removeFromTop(34);
    titleLabel.setBounds(header.removeFromLeft(header.getWidth() / 2));
    roleSelector.setBounds(header.removeFromRight(120));
    roleLabel.setBounds(header);

    auto details = bounds.removeFromTop(30);
    sessionLabel.setBounds(details.removeFromLeft(details.getWidth() / 2));
    connectionLabel.setBounds(details);

    auto panel = bounds.withTrimmedTop(8).reduced(18);
    auto masterRow = panel.removeFromTop(panel.getHeight() / 2);
    auto speakerRow = panel;

    masterLabel.setBounds(masterRow.removeFromLeft(150));
    masterSlider.setBounds(masterRow.reduced(4, 2));
    speakerLabel.setBounds(speakerRow.removeFromLeft(150));
    speakerSlider.setBounds(speakerRow.reduced(4, 2));
}

void ClubCraftPhase0AudioProcessorEditor::timerCallback()
{
    refreshStatus();
}

void ClubCraftPhase0AudioProcessorEditor::configureSlider(juce::Slider& slider,
                                                           juce::Label& label,
                                                           const juce::String& labelText)
{
    label.setText(labelText, juce::dontSendNotification);
    label.setFont(juce::FontOptions(12.0f).withKerningFactor(0.08f));
    label.setColour(juce::Label::textColourId, kGraphite);
    addAndMakeVisible(label);

    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 68, 24);
    slider.setTextValueSuffix(" dB");
    slider.setColour(juce::Slider::trackColourId, kGraphite.withAlpha(0.20f));
    slider.setColour(juce::Slider::thumbColourId, kGraphite);
    slider.setColour(juce::Slider::backgroundColourId, kGraphite.withAlpha(0.10f));
    slider.setColour(juce::Slider::textBoxTextColourId, kGraphite);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(slider);
}

void ClubCraftPhase0AudioProcessorEditor::refreshStatus()
{
    const auto isClub = processor.isClubRole();
    sessionLabel.setText("SESSION  " + processor.getSessionId(), juce::dontSendNotification);

    if (isClub)
    {
        connectionLabel.setText("CLUB publisher active", juce::dontSendNotification);
        masterSlider.setEnabled(true);
        speakerSlider.setEnabled(true);
    }
    else
    {
        connectionLabel.setText(processor.isConnectedToClub() ? "Connected to CLUB" : "Waiting for CLUB",
                                juce::dontSendNotification);
        masterSlider.setEnabled(false);
        speakerSlider.setEnabled(false);
    }
}
