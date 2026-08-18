#pragma once

#include <memory>
#include <string>

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
    class FloorView;
    class RoutingMatrix;

    void timerCallback() override;
    void configureDbSlider(juce::Slider& slider, juce::Label& label, const juce::String& labelText);
    void configureToneSlider();
    void refreshStatus();
    void refreshDynamicUi();
    void selectSpeaker(const std::string& stableId);
    void addSpeaker();
    void deleteSelectedSpeaker();
    void materialiseLegacyRouting();

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

    juce::Label floorLabel;
    std::unique_ptr<FloorView> floorView;
    juce::TextButton addSpeakerButton { "ADD SPEAKER" };
    juce::TextButton deleteSpeakerButton { "DELETE SELECTED" };
    juce::Label selectedSpeakerLabel;
    juce::Label selectedSpeakerTypeLabel;
    juce::ComboBox selectedSpeakerType;
    juce::Label selectedSpeakerLevelLabel;
    juce::Slider selectedSpeakerLevel;

    juce::Label routingLabel;
    juce::TextEditor matrixSearch;
    juce::TextButton materialiseButton { "MATERIALISE LEGACY ROUTES" };
    juce::Viewport matrixViewport;
    std::unique_ptr<RoutingMatrix> routingMatrix;

    std::string selectedSpeakerId;
    std::uint64_t displayedSceneRevision = 0;
    bool updatingInspector = false;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> roleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> responseAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClubCraftPhase0AudioProcessorEditor)
};
