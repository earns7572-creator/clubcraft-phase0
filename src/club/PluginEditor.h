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
    juce::Label speakerSectionLabel;
    std::array<juce::Label, clubcraft::kPhase1SpeakerCount> speakerLabels;
    std::array<juce::Slider, clubcraft::kPhase1SpeakerCount> speakerSliders;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> roleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> responseAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, clubcraft::kPhase1SpeakerCount> speakerAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClubCraftPhase0AudioProcessorEditor)
};
