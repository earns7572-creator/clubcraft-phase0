#include "PluginEditor.h"

#include <array>
#include <cmath>

namespace
{
const juce::Colour kBackground { 0xffd9d5cc };
const juce::Colour kPanel { 0xffebe8e1 };
const juce::Colour kGraphite { 0xff2c2d2b };
const juce::Colour kMutedGraphite { 0xff676862 };
const juce::Colour kAccent { 0xff6c8275 };
const juce::Colour kDisabled { 0xff969791 };

constexpr std::array<const char*, clubcraft::kPhase1SpeakerCount> kSpeakerNames {
    "FRONT L",
    "FRONT R",
    "REAR L",
    "REAR R",
};

constexpr std::array<const char*, clubcraft::kPhase1SpeakerCount> kSpeakerParameterIds {
    "speakerLevel1",
    "speakerLevel2",
    "speakerLevel3",
    "speakerLevel4",
};
}

ClubCraftPhase0AudioProcessorEditor::ClubCraftPhase0AudioProcessorEditor(
    ClubCraftPhase0AudioProcessor& processorToUse)
    : AudioProcessorEditor(&processorToUse), pluginProcessor(processorToUse)
{
    setSize(660, 520);
    setResizable(true, true);
    setResizeLimits(520, 410, 1000, 760);

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
    connectionLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(connectionLabel);

    configureDbSlider(masterSlider, masterLabel, "MASTER");
    configureToneSlider();

    speakerSectionLabel.setText("FULL SIGNAL SPEAKERS", juce::dontSendNotification);
    speakerSectionLabel.setFont(juce::FontOptions(11.0f).withKerningFactor(0.12f));
    speakerSectionLabel.setColour(juce::Label::textColourId, kMutedGraphite);
    addAndMakeVisible(speakerSectionLabel);

    for (std::size_t index = 0; index < clubcraft::kPhase1SpeakerCount; ++index)
        configureDbSlider(speakerSliders[index], speakerLabels[index], kSpeakerNames[index]);

    roleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        pluginProcessor.getParameters(), "role", roleSelector);
    masterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        pluginProcessor.getParameters(), "masterLevel", masterSlider);
    responseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        pluginProcessor.getParameters(), "genericResponseTone", responseSlider);

    for (std::size_t index = 0; index < clubcraft::kPhase1SpeakerCount; ++index)
    {
        speakerAttachments[index] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            pluginProcessor.getParameters(), kSpeakerParameterIds[index], speakerSliders[index]);
    }

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

    auto divider = panel.reduced(20);
    const auto dividerY = divider.getY() + 102;
    graphics.drawLine(static_cast<float>(divider.getX()), static_cast<float>(dividerY),
                      static_cast<float>(divider.getRight()), static_cast<float>(dividerY), 1.0f);
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
    auto masterRow = panel.removeFromTop(46);
    masterLabel.setBounds(masterRow.removeFromLeft(150));
    masterSlider.setBounds(masterRow.reduced(4, 4));

    auto responseRow = panel.removeFromTop(46);
    responseLabel.setBounds(responseRow.removeFromLeft(150));
    responseSlider.setBounds(responseRow.reduced(4, 4));

    speakerSectionLabel.setBounds(panel.removeFromTop(28));
    const auto speakerRowHeight = panel.getHeight() / static_cast<int>(clubcraft::kPhase1SpeakerCount);
    for (std::size_t index = 0; index < clubcraft::kPhase1SpeakerCount; ++index)
    {
        auto row = panel.removeFromTop(speakerRowHeight);
        speakerLabels[index].setBounds(row.removeFromLeft(150));
        speakerSliders[index].setBounds(row.reduced(4, 5));
    }
}

void ClubCraftPhase0AudioProcessorEditor::timerCallback()
{
    refreshStatus();
}

void ClubCraftPhase0AudioProcessorEditor::configureDbSlider(juce::Slider& slider,
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

void ClubCraftPhase0AudioProcessorEditor::configureToneSlider()
{
    responseLabel.setText("GENERIC RESPONSE", juce::dontSendNotification);
    responseLabel.setFont(juce::FontOptions(12.0f).withKerningFactor(0.08f));
    responseLabel.setColour(juce::Label::textColourId, kGraphite);
    addAndMakeVisible(responseLabel);

    responseSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    responseSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 68, 24);
    responseSlider.textFromValueFunction = [] (double value)
    {
        return juce::String(static_cast<int>(std::round(value * 100.0))) + " %";
    };
    responseSlider.valueFromTextFunction = [] (const juce::String& text)
    {
        return juce::jlimit(0.0, 1.0, text.retainCharacters("0123456789.").getDoubleValue() / 100.0);
    };
    responseSlider.setColour(juce::Slider::trackColourId, kAccent.withAlpha(0.60f));
    responseSlider.setColour(juce::Slider::thumbColourId, kAccent);
    responseSlider.setColour(juce::Slider::backgroundColourId, kGraphite.withAlpha(0.10f));
    responseSlider.setColour(juce::Slider::textBoxTextColourId, kGraphite);
    responseSlider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    responseSlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(responseSlider);
}

void ClubCraftPhase0AudioProcessorEditor::refreshStatus()
{
    const auto isClub = pluginProcessor.isClubRole();
    sessionLabel.setText("SESSION  " + pluginProcessor.getSessionId(), juce::dontSendNotification);

    if (isClub)
    {
        connectionLabel.setText("CLUB / 4-speaker scene active", juce::dontSendNotification);
        masterSlider.setEnabled(true);
        responseSlider.setEnabled(true);
        for (auto& speakerSlider : speakerSliders)
            speakerSlider.setEnabled(true);
    }
    else
    {
        connectionLabel.setText(pluginProcessor.isConnectedToClub()
                ? "Connected / Full Signal to 4 speakers"
                : "Waiting for CLUB",
            juce::dontSendNotification);
        masterSlider.setEnabled(false);
        responseSlider.setEnabled(false);
        for (auto& speakerSlider : speakerSliders)
            speakerSlider.setEnabled(false);
    }
}
