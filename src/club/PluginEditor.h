#pragma once

#include <array>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

class ClubCraftPhase0AudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                   private juce::Timer
{
public:
    explicit ClubCraftPhase0AudioProcessorEditor(ClubCraftPhase0AudioProcessor& processor);
    ~ClubCraftPhase0AudioProcessorEditor() override;

    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    void timerCallback() override;
    void configureDbSlider(juce::Slider& slider, juce::Label& label, const juce::String& labelText);
    void configureMeterSlider(juce::Slider& slider, juce::Label& label, const juce::String& labelText);
    void configureToneSlider();
    void refreshStatus();

    ClubCraftPhase0AudioProcessor& pluginProcessor;

    juce::Label titleLabel;
    juce::Label roleLabel;
    juce::ComboBox roleSelector;
    juce::Label sessionLabel;
    juce::Label connectionLabel;

    juce::Label masterLabel;
    juce::Slider masterSlider;
    juce::Label responseLabel;
    juce::Slider responseSlider;

    juce::Label listenerSectionLabel;
    juce::Label listenerXLabel;
    juce::Slider listenerXSlider;
    juce::Label listenerYLabel;
    juce::Slider listenerYSlider;

    juce::Label speakerSectionLabel;
    std::array<juce::Label, clubcraft::kSpeakerCount> speakerLabels;
    std::array<juce::ComboBox, clubcraft::kSpeakerCount> speakerTypeSelectors;
    std::array<juce::Slider, clubcraft::kSpeakerCount> speakerSliders;
    std::array<juce::Label, clubcraft::kSpeakerCount> speakerXLabels;
    std::array<juce::Slider, clubcraft::kSpeakerCount> speakerXSliders;
    std::array<juce::Label, clubcraft::kSpeakerCount> speakerYLabels;
    std::array<juce::Slider, clubcraft::kSpeakerCount> speakerYSliders;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> roleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> responseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> listenerXAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> listenerYAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, clubcraft::kSpeakerCount> speakerAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>, clubcraft::kSpeakerCount> speakerTypeAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, clubcraft::kSpeakerCount> speakerXAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, clubcraft::kSpeakerCount> speakerYAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClubCraftPhase0AudioProcessorEditor)
};
