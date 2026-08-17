#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <algorithm>
#include <array>

namespace
{
constexpr int kStateSchemaVersion = 3;
constexpr std::array<const char*, clubcraft::kPhase1SpeakerCount> kSpeakerParameterIds {
    "speakerLevel1",
    "speakerLevel2",
    "speakerLevel3",
    "speakerLevel4",
};

[[nodiscard]] float readParameter(const juce::AudioProcessorValueTreeState& parameters,
                                  const juce::String& parameterId) noexcept
{
    if (const auto* value = parameters.getRawParameterValue(parameterId))
        return value->load();

    return 0.0f;
}
}

ClubCraftPhase0AudioProcessor::ClubCraftPhase0AudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, juce::Identifier("ClubCraftPhase1State"), createParameterLayout()),
      sourceId(juce::Uuid().toString())
{
    parameters.addParameterListener("role", this);
    parameters.addParameterListener("masterLevel", this);
    parameters.addParameterListener("genericResponseTone", this);
    for (const auto* parameterId : kSpeakerParameterIds)
        parameters.addParameterListener(parameterId, this);

    refreshSessionHandle();
    lastKnownClubRole.store(isClubRole(), std::memory_order_release);
    sourceName = isClubRole() ? "Club" : "Source";

    if (isClubRole())
        publishClubSnapshot();
    else
        registerAsSource();

    startTimerHz(20);
}

ClubCraftPhase0AudioProcessor::~ClubCraftPhase0AudioProcessor()
{
    stopTimer();
    parameters.removeParameterListener("role", this);
    parameters.removeParameterListener("masterLevel", this);
    parameters.removeParameterListener("genericResponseTone", this);
    for (const auto* parameterId : kSpeakerParameterIds)
        parameters.removeParameterListener(parameterId, this);
    clubcraft::SessionRegistry::instance().unregisterSource(sourceId.toStdString());
}

void ClubCraftPhase0AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    genericSpeakerResponse.prepare(sampleRate, samplesPerBlock, std::max(1, getTotalNumOutputChannels()));
}

void ClubCraftPhase0AudioProcessor::releaseResources()
{
    genericSpeakerResponse.reset();
}

bool ClubCraftPhase0AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void ClubCraftPhase0AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (isClubRole())
    {
        const auto snapshot = clubcraft::SessionRegistry::readSnapshot(sessionHandle);
        const auto gain = snapshot.has_value()
            ? snapshot->masterLinearGain
            : juce::Decibels::decibelsToGain(readParameter(parameters, "masterLevel"));
        buffer.applyGain(gain);
        return;
    }

    if (const auto snapshot = clubcraft::SessionRegistry::readSnapshot(sessionHandle))
    {
        // All Phase 1 speakers receive the source's full signal. As the generic
        // response is shared, rendering it once before the normalized sum is
        // mathematically equivalent to rendering the same response on each route.
        genericSpeakerResponse.process(buffer, snapshot->genericResponseTone);
        buffer.applyGain(snapshot->normalizedFullSignalGain());
    }
}

juce::AudioProcessorEditor* ClubCraftPhase0AudioProcessor::createEditor()
{
    return new ClubCraftPhase0AudioProcessorEditor(*this);
}

bool ClubCraftPhase0AudioProcessor::hasEditor() const
{
    return true;
}

const juce::String ClubCraftPhase0AudioProcessor::getName() const
{
    return "Club Craft";
}

bool ClubCraftPhase0AudioProcessor::acceptsMidi() const
{
    return false;
}

bool ClubCraftPhase0AudioProcessor::producesMidi() const
{
    return false;
}

bool ClubCraftPhase0AudioProcessor::isMidiEffect() const
{
    return false;
}

double ClubCraftPhase0AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ClubCraftPhase0AudioProcessor::getNumPrograms()
{
    return 1;
}

int ClubCraftPhase0AudioProcessor::getCurrentProgram()
{
    return 0;
}

void ClubCraftPhase0AudioProcessor::setCurrentProgram(int)
{
}

const juce::String ClubCraftPhase0AudioProcessor::getProgramName(int)
{
    return {};
}

void ClubCraftPhase0AudioProcessor::changeProgramName(int, const juce::String&)
{
}

void ClubCraftPhase0AudioProcessor::getStateInformation(juce::MemoryBlock& destinationData)
{
    auto state = parameters.copyState();
    state.setProperty("schemaVersion", kStateSchemaVersion, nullptr);
    state.setProperty("sessionId", sessionId, nullptr);
    state.setProperty("sourceId", sourceId, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destinationData);
}

void ClubCraftPhase0AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const auto oldSourceId = sourceId;
    clubcraft::SessionRegistry::instance().unregisterSource(oldSourceId.toStdString());

    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        const auto restoredState = juce::ValueTree::fromXml(*xml);
        if (restoredState.isValid())
        {
            parameters.replaceState(restoredState);
            restoreLegacyPrimarySpeakerLevel(restoredState);
            sessionId = restoredState.getProperty("sessionId", sessionId).toString();
            sourceId = restoredState.getProperty("sourceId", sourceId).toString();
        }
    }

    refreshSessionHandle();
    sourceName = isClubRole() ? "Club" : "Source";
    lastKnownClubRole.store(!isClubRole(), std::memory_order_release);
    snapshotDirty.store(true, std::memory_order_release);
    genericSpeakerResponse.reset();
    reconcileRole();
}

juce::AudioProcessorValueTreeState& ClubCraftPhase0AudioProcessor::getParameters() noexcept
{
    return parameters;
}

const juce::String& ClubCraftPhase0AudioProcessor::getSessionId() const noexcept
{
    return sessionId;
}

const juce::String& ClubCraftPhase0AudioProcessor::getSourceId() const noexcept
{
    return sourceId;
}

bool ClubCraftPhase0AudioProcessor::isClubRole() const noexcept
{
    return readParameter(parameters, "role") >= 0.5f;
}

bool ClubCraftPhase0AudioProcessor::isConnectedToClub() const noexcept
{
    return clubcraft::SessionRegistry::readSnapshot(sessionHandle).has_value();
}

juce::AudioProcessorValueTreeState::ParameterLayout ClubCraftPhase0AudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameterLayout;
    parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
        "role", "Role", juce::StringArray { "SOURCE", "CLUB" }, 0));
    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        "masterLevel", "Master Level", juce::NormalisableRange<float>(-60.0f, 12.0f, 0.01f), 0.0f));
    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        "speakerLevel1", "Front L Level", juce::NormalisableRange<float>(-60.0f, 12.0f, 0.01f), 0.0f));
    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        "speakerLevel2", "Front R Level", juce::NormalisableRange<float>(-60.0f, 12.0f, 0.01f), 0.0f));
    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        "speakerLevel3", "Rear L Level", juce::NormalisableRange<float>(-60.0f, 12.0f, 0.01f), 0.0f));
    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        "speakerLevel4", "Rear R Level", juce::NormalisableRange<float>(-60.0f, 12.0f, 0.01f), 0.0f));
    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        "genericResponseTone", "Generic Response Tone", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));

    return { parameterLayout.begin(), parameterLayout.end() };
}

void ClubCraftPhase0AudioProcessor::parameterChanged(const juce::String& parameterId, float)
{
    if (parameterId == "role" || isClubRole())
        snapshotDirty.store(true, std::memory_order_release);
}

void ClubCraftPhase0AudioProcessor::timerCallback()
{
    reconcileRole();

    if (isClubRole())
    {
        if (snapshotDirty.exchange(false, std::memory_order_acq_rel))
            publishClubSnapshot();
    }
    else
    {
        registerAsSource();
    }
}

void ClubCraftPhase0AudioProcessor::refreshSessionHandle()
{
    sessionHandle = clubcraft::SessionRegistry::instance().acquireSession(sessionId.toStdString());
}

void ClubCraftPhase0AudioProcessor::publishClubSnapshot()
{
    if (!isClubRole())
        return;

    clubcraft::SceneSnapshot snapshot;
    snapshot.sessionId = sessionId.toStdString();
    snapshot.revision = revision.fetch_add(1, std::memory_order_relaxed) + 1;
    snapshot.masterLevelDb = readParameter(parameters, "masterLevel");
    for (std::size_t index = 0; index < clubcraft::kPhase1SpeakerCount; ++index)
        snapshot.speakerLevelDb[index] = readParameter(parameters, kSpeakerParameterIds[index]);
    snapshot.genericResponseTone = readParameter(parameters, "genericResponseTone");

    clubcraft::SessionRegistry::instance().publishSnapshot(snapshot);
}

void ClubCraftPhase0AudioProcessor::registerAsSource()
{
    if (isClubRole())
        return;

    clubcraft::SessionRegistry::instance().registerSource({
        .sourceId = sourceId.toStdString(),
        .sessionId = sessionId.toStdString(),
        .displayName = sourceName.toStdString(),
        .heartbeat = revision.fetch_add(1, std::memory_order_relaxed) + 1,
    });
}

void ClubCraftPhase0AudioProcessor::reconcileRole()
{
    const auto currentRole = isClubRole();
    const auto previousRole = lastKnownClubRole.exchange(currentRole, std::memory_order_acq_rel);

    if (currentRole == previousRole)
        return;

    if (!previousRole)
        clubcraft::SessionRegistry::instance().unregisterSource(sourceId.toStdString());

    sourceName = currentRole ? "Club" : "Source";
    snapshotDirty.store(true, std::memory_order_release);
    genericSpeakerResponse.reset();

    if (currentRole)
        publishClubSnapshot();
    else
        registerAsSource();
}

void ClubCraftPhase0AudioProcessor::restoreLegacyPrimarySpeakerLevel(const juce::ValueTree& restoredState)
{
    const auto legacyParameter = restoredState.getChildWithName("primarySpeakerLevel");
    if (!legacyParameter.isValid())
        return;

    const auto legacyValue = static_cast<float>(legacyParameter.getProperty("value", 0.0f));
    if (auto* speakerOne = parameters.getParameter("speakerLevel1"))
        speakerOne->setValueNotifyingHost(speakerOne->convertTo0to1(legacyValue));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ClubCraftPhase0AudioProcessor();
}
