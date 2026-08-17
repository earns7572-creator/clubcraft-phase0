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

constexpr std::array<const char*, clubcraft::kSpeakerCount> kSpeakerNames {
    "FRONT L",
    "FRONT R",
    "REAR L",
    "REAR R",
};

constexpr std::array<const char*, clubcraft::kSpeakerCount> kSpeakerLevelParameterIds {
    "speakerLevel1",
    "speakerLevel2",
    "speakerLevel3",
    "speakerLevel4",
};

constexpr std::array<const char*, clubcraft::kSpeakerCount> kSpeakerTypeParameterIds {
    "speakerType1",
    "speakerType2",
    "speakerType3",
    "speakerType4",
};

constexpr std::array<const char*, clubcraft::kSpeakerCount> kSpeakerPositionXParameterIds {
    "speakerPositionX1",
    "speakerPositionX2",
    "speakerPositionX3",
    "speakerPositionX4",
};

constexpr std::array<const char*, clubcraft::kSpeakerCount> kSpeakerPositionYParameterIds {
    "speakerPositionY1",
    "speakerPositionY2",
    "speakerPositionY3",
    "speakerPositionY4",
};
}

ClubCraftPhase0AudioProcessorEditor::ClubCraftPhase0AudioProcessorEditor(
    ClubCraftPhase0AudioProcessor& processorToUse)
    : AudioProcessorEditor(&processorToUse), pluginProcessor(processorToUse)
{
    setSize(940, 820);
    setResizable(true, true);
    setResizeLimits(700, 640, 1280, 1120);

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

    listenerSectionLabel.setText("LISTENER / AUDIENCE POSITION", juce::dontSendNotification);
    listenerSectionLabel.setFont(juce::FontOptions(11.0f).withKerningFactor(0.12f));
    listenerSectionLabel.setColour(juce::Label::textColourId, kMutedGraphite);
    addAndMakeVisible(listenerSectionLabel);
    configureMeterSlider(listenerXSlider, listenerXLabel, "LISTENER X  (L - / R +)");
    configureMeterSlider(listenerYSlider, listenerYLabel, "LISTENER Y  (REAR - / FRONT +)");

    speakerSectionLabel.setText("INDIVIDUAL SPEAKER PLACEMENT & VOICES", juce::dontSendNotification);
    speakerSectionLabel.setFont(juce::FontOptions(11.0f).withKerningFactor(0.12f));
    speakerSectionLabel.setColour(juce::Label::textColourId, kMutedGraphite);
    addAndMakeVisible(speakerSectionLabel);

    for (std::size_t index = 0; index < clubcraft::kSpeakerCount; ++index)
    {
        configureDbSlider(
            speakerSliders[index], speakerLabels[index], juce::String(kSpeakerNames[index]) + " LEVEL");
        configureMeterSlider(speakerXSliders[index], speakerXLabels[index], "X");
        configureMeterSlider(speakerYSliders[index], speakerYLabels[index], "Y");

        auto& selector = speakerTypeSelectors[index];
        selector.addItem("SUB", 1);
        selector.addItem("WOOFER", 2);
        selector.addItem("FULL RANGE", 3);
        selector.addItem("MID", 4);
        selector.addItem("HIGH", 5);
        selector.setJustificationType(juce::Justification::centred);
        selector.setColour(juce::ComboBox::backgroundColourId, kBackground);
        selector.setColour(juce::ComboBox::outlineColourId, kGraphite.withAlpha(0.18f));
        selector.setColour(juce::ComboBox::textColourId, kGraphite);
        selector.setColour(juce::ComboBox::arrowColourId, kGraphite);
        addAndMakeVisible(selector);
    }

    roleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        pluginProcessor.getParameters(), "role", roleSelector);
    masterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        pluginProcessor.getParameters(), "masterLevel", masterSlider);
    responseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        pluginProcessor.getParameters(), "genericResponseTone", responseSlider);
    listenerXAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        pluginProcessor.getParameters(), "listenerPositionX", listenerXSlider);
    listenerYAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        pluginProcessor.getParameters(), "listenerPositionY", listenerYSlider);

    for (std::size_t index = 0; index < clubcraft::kSpeakerCount; ++index)
    {
        speakerAttachments[index] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            pluginProcessor.getParameters(), kSpeakerLevelParameterIds[index], speakerSliders[index]);
        speakerTypeAttachments[index] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            pluginProcessor.getParameters(), kSpeakerTypeParameterIds[index], speakerTypeSelectors[index]);
        speakerXAttachments[index] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            pluginProcessor.getParameters(), kSpeakerPositionXParameterIds[index], speakerXSliders[index]);
        speakerYAttachments[index] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            pluginProcessor.getParameters(), kSpeakerPositionYParameterIds[index], speakerYSliders[index]);
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
    constexpr auto rowHeight = 40;

    auto masterRow = panel.removeFromTop(rowHeight);
    masterLabel.setBounds(masterRow.removeFromLeft(190));
    masterSlider.setBounds(masterRow.reduced(4, 4));

    auto responseRow = panel.removeFromTop(rowHeight);
    responseLabel.setBounds(responseRow.removeFromLeft(190));
    responseSlider.setBounds(responseRow.reduced(4, 4));

    listenerSectionLabel.setBounds(panel.removeFromTop(25));
    auto listenerXRow = panel.removeFromTop(rowHeight);
    listenerXLabel.setBounds(listenerXRow.removeFromLeft(220));
    listenerXSlider.setBounds(listenerXRow.reduced(4, 4));
    auto listenerYRow = panel.removeFromTop(rowHeight);
    listenerYLabel.setBounds(listenerYRow.removeFromLeft(220));
    listenerYSlider.setBounds(listenerYRow.reduced(4, 4));

    speakerSectionLabel.setBounds(panel.removeFromTop(25));
    const auto speakerRowHeight = panel.getHeight() / static_cast<int>(clubcraft::kSpeakerCount);
    for (std::size_t index = 0; index < clubcraft::kSpeakerCount; ++index)
    {
        auto speakerRow = panel.removeFromTop(speakerRowHeight);
        auto voiceRow = speakerRow.removeFromTop(speakerRowHeight / 2);
        speakerLabels[index].setBounds(voiceRow.removeFromLeft(150));
        speakerTypeSelectors[index].setBounds(voiceRow.removeFromLeft(132).reduced(3, 7));
        speakerSliders[index].setBounds(voiceRow.reduced(4, 5));

        auto positionRow = speakerRow;
        speakerXLabels[index].setBounds(positionRow.removeFromLeft(26));
        speakerXSliders[index].setBounds(positionRow.removeFromLeft(positionRow.getWidth() / 2).reduced(4, 5));
        speakerYLabels[index].setBounds(positionRow.removeFromLeft(26));
        speakerYSliders[index].setBounds(positionRow.reduced(4, 5));
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

void ClubCraftPhase0AudioProcessorEditor::configureMeterSlider(juce::Slider& slider,
                                                                juce::Label& label,
                                                                const juce::String& labelText)
{
    label.setText(labelText, juce::dontSendNotification);
    label.setFont(juce::FontOptions(12.0f).withKerningFactor(0.08f));
    label.setColour(juce::Label::textColourId, kGraphite);
    addAndMakeVisible(label);

    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 68, 24);
    slider.setTextValueSuffix(" m");
    slider.setColour(juce::Slider::trackColourId, kAccent.withAlpha(0.55f));
    slider.setColour(juce::Slider::thumbColourId, kAccent);
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

    masterSlider.setEnabled(isClub);
    responseSlider.setEnabled(isClub);
    listenerXSlider.setEnabled(isClub);
    listenerYSlider.setEnabled(isClub);
    for (auto& speakerSlider : speakerSliders)
        speakerSlider.setEnabled(isClub);
    for (auto& speakerTypeSelector : speakerTypeSelectors)
        speakerTypeSelector.setEnabled(isClub);
    for (auto& speakerXSlider : speakerXSliders)
        speakerXSlider.setEnabled(isClub);
    for (auto& speakerYSlider : speakerYSliders)
        speakerYSlider.setEnabled(isClub);

    if (isClub)
    {
        connectionLabel.setText("CLUB / publishes speaker & listener layout", juce::dontSendNotification);
    }
    else
    {
        connectionLabel.setText(pluginProcessor.isConnectedToClub()
                ? "Connected / fixed source follows CLUB layout"
                : "Waiting for CLUB",
            juce::dontSendNotification);
    }
}
