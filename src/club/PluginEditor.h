#pragma once

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
    void configureSlider(juce::Slider& slider, juce::Label& label, const juce::String& labelText);
    void refreshStatus();

    ClubCraftPhase0AudioProcessor& processor;

    juce::Label titleLabel;
    juce::Label roleLabel;
    juce::ComboBox roleSelector;
    juce::Label sessionLabel;
    juce::Label connectionLabel;
    juce::Label masterLabel;
    juce::Label speakerLabel;
    juce::Slider masterSlider;
    juce::Slider speakerSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> roleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> speakerAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClubCraftPhase0AudioProcessorEditor)
};
