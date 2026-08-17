#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "SessionRegistry.h"
#include "StereoSpatialRenderer.h"

class ClubCraftPhase0AudioProcessor final : public juce::AudioProcessor,
                                             private juce::AudioProcessorValueTreeState::Listener,
                                             private juce::Timer
{
public:
    using juce::AudioProcessor::processBlock;

    ClubCraftPhase0AudioProcessor();
    ~ClubCraftPhase0AudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    [[nodiscard]] juce::AudioProcessorEditor* createEditor() override;
    [[nodiscard]] bool hasEditor() const override;

    [[nodiscard]] const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destinationData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    [[nodiscard]] juce::AudioProcessorValueTreeState& getParameters() noexcept;
    [[nodiscard]] const juce::String& getSessionId() const noexcept;
    [[nodiscard]] const juce::String& getSourceId() const noexcept;
    [[nodiscard]] bool isClubRole() const noexcept;
    [[nodiscard]] bool isConnectedToClub() const noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void parameterChanged(const juce::String& parameterId, float newValue) override;
    void timerCallback() override;
    void refreshSessionHandle();
    void publishClubSnapshot();
    void registerAsSource();
    void reconcileRole();
    void restoreLegacyPrimarySpeakerLevel(const juce::ValueTree& restoredState);
    [[nodiscard]] clubcraft::PlanarPosition sourcePosition() const noexcept;

    juce::AudioProcessorValueTreeState parameters;
    juce::String sessionId { "phase0-default-club" };
    juce::String sourceId;
    juce::String sourceName { "Source" };
    clubcraft::SessionRegistry::SessionHandle sessionHandle;
    clubcraft::StereoSpatialRenderer spatialRenderer;
    std::atomic<bool> snapshotDirty { true };
    std::atomic<std::uint64_t> revision { 0 };
    std::atomic<bool> lastKnownClubRole { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClubCraftPhase0AudioProcessor)
};
