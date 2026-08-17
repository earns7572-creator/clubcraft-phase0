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

constexpr std::array<const char*, clubcraft::kSpeakerCount> kSpeakerParameterIds {
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
    setSize(720, 690);
    setResizable(true, true);
    setResizeLimits(540, 500, 1080, 920);

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

    sourceSectionLabel.setText("SOURCE POSITION", juce::dontSendNotification);
    sourceSectionLabel.setFont(juce::FontOptions(11.0f).withKerningFactor(0.12f));
    sourceSectionLabel.setColour(juce::Label::textColourId, kMutedGraphite);
    addAndMakeVisible(sourceSectionLabel);
    configureMeterSlider(sourceXSlider, sourceXLabel, "SOURCE X  (L - / R +)");
    configureMeterSlider(sourceYSlider, sourceYLabel, "SOURCE Y  (REAR - / FRONT +)");

    speakerLayoutSectionLabel.setText("SPEAKER LAYOUT", juce::dontSendNotification);
    speakerLayoutSectionLabel.setFont(juce::FontOptions(11.0f).withKerningFactor(0.12f));
    speakerLayoutSectionLabel.setColour(juce::Label::textColourId, kMutedGraphite);
    addAndMakeVisible(speakerLayoutSectionLabel);
    configureMeterSlider(speakerSpreadSlider, speakerSpreadLabel, "SPEAKER SPREAD");
    configureMeterSlider(speakerDepthSlider, speakerDepthLabel, "SPEAKER DEPTH");

    speakerSectionLabel.setText("FULL SIGNAL SPEAKERS", juce::dontSendNotification);
    speakerSectionLabel.setFont(juce::FontOptions(11.0f).withKerningFactor(0.12f));
    speakerSectionLabel.setColour(juce::Label::textColourId, kMutedGraphite);
    addAndMakeVisible(speakerSectionLabel);

    for (std::size_t index = 0; index < clubcraft::kSpeakerCount; ++index)
        configureDbSlider(speakerSliders[index], speakerLabels[index], kSpeakerNames[index]);

    roleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        pluginProcessor.getParameters(), "role", roleSelector);
    masterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        pluginProcessor.getParameters(), "masterLevel", masterSlider);
    responseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        pluginProcessor.getParameters(), "genericResponseTone", responseSlider);
    sourceXAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        pluginProcessor.getParameters(), "sourcePositionX", sourceXSlider);
    sourceYAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        pluginProcessor.getParameters(), "sourcePositionY", sourceYSlider);
    speakerSpreadAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        pluginProcessor.getParameters(), "speakerSpread", speakerSpreadSlider);
    speakerDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        pluginProcessor.getParameters(), "speakerDepth", speakerDepthSlider);

    for (std::size_t index = 0; index < clubcraft::kSpeakerCount; ++index)
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
    const auto rowHeight = 40;

    auto masterRow = panel.removeFromTop(rowHeight);
    masterLabel.setBounds(masterRow.removeFromLeft(190));
    masterSlider.setBounds(masterRow.reduced(4, 4));

    auto responseRow = panel.removeFromTop(rowHeight);
    responseLabel.setBounds(responseRow.removeFromLeft(190));
    responseSlider.setBounds(responseRow.reduced(4, 4));

    sourceSectionLabel.setBounds(panel.removeFromTop(25));
    auto sourceXRow = panel.removeFromTop(rowHeight);
    sourceXLabel.setBounds(sourceXRow.removeFromLeft(190));
    sourceXSlider.setBounds(sourceXRow.reduced(4, 4));
    auto sourceYRow = panel.removeFromTop(rowHeight);
    sourceYLabel.setBounds(sourceYRow.removeFromLeft(190));
    sourceYSlider.setBounds(sourceYRow.reduced(4, 4));

    speakerLayoutSectionLabel.setBounds(panel.removeFromTop(25));
    auto spreadRow = panel.removeFromTop(rowHeight);
    speakerSpreadLabel.setBounds(spreadRow.removeFromLeft(190));
    speakerSpreadSlider.setBounds(spreadRow.reduced(4, 4));
    auto depthRow = panel.removeFromTop(rowHeight);
    speakerDepthLabel.setBounds(depthRow.removeFromLeft(190));
    speakerDepthSlider.setBounds(depthRow.reduced(4, 4));

    speakerSectionLabel.setBounds(panel.removeFromTop(25));
    const auto speakerRowHeight = panel.getHeight() / static_cast<int>(clubcraft::kSpeakerCount);
    for (std::size_t index = 0; index < clubcraft::kSpeakerCount; ++index)
    {
        auto row = panel.removeFromTop(speakerRowHeight);
        speakerLabels[index].setBounds(row.removeFromLeft(190));
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
    speakerSpreadSlider.setEnabled(isClub);
    speakerDepthSlider.setEnabled(isClub);
    for (auto& speakerSlider : speakerSliders)
        speakerSlider.setEnabled(isClub);

    sourceXSlider.setEnabled(!isClub);
    sourceYSlider.setEnabled(!isClub);

    if (isClub)
    {
        connectionLabel.setText("CLUB / publishes 4-speaker spatial scene", juce::dontSendNotification);
    }
    else
    {
        connectionLabel.setText(pluginProcessor.isConnectedToClub()
                ? "Connected / distance, delay & stereo pan active"
                : "Waiting for CLUB",
            juce::dontSendNotification);
    }
}
