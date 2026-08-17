#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "SceneState.h"

#include <algorithm>
#include <array>

namespace
{
constexpr int kStateSchemaVersion = 7;
const juce::Identifier kSchema7StateType { "ClubCraftState" };
const juce::Identifier kSchema7ApvtsType { "APVTS" };

std::atomic<std::uint64_t> nextRuntimeInstanceToken { 1 };

constexpr std::array<const char*, clubcraft::kSpeakerCount> kSpeakerParameterIds {
    "speakerLevel1", "speakerLevel2", "speakerLevel3", "speakerLevel4",
};
constexpr std::array<const char*, clubcraft::kSpeakerCount> kSpeakerTypeParameterIds {
    "speakerType1", "speakerType2", "speakerType3", "speakerType4",
};
constexpr std::array<const char*, clubcraft::kSpeakerCount> kSpeakerPositionXParameterIds {
    "speakerPositionX1", "speakerPositionX2", "speakerPositionX3", "speakerPositionX4",
};
constexpr std::array<const char*, clubcraft::kSpeakerCount> kSpeakerPositionYParameterIds {
    "speakerPositionY1", "speakerPositionY2", "speakerPositionY3", "speakerPositionY4",
};
constexpr std::array<const char*, clubcraft::kSpeakerCount> kLegacySpeakerNames {
    "Front L", "Front R", "Rear L", "Rear R",
};

[[nodiscard]] float readParameter(const juce::AudioProcessorValueTreeState& parameters,
                                  const juce::String& parameterId) noexcept
{
    if (const auto* value = parameters.getRawParameterValue(parameterId))
        return value->load();
    return 0.0f;
}

[[nodiscard]] juce::String legacySpeakerId(std::size_t index)
{
    return "legacy-speaker-" + juce::String(static_cast<int>(index + 1));
}
}

ClubCraftPhase0AudioProcessor::ClubCraftPhase0AudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, juce::Identifier("ClubCraftPhase1State"), createParameterLayout()),
      sourceId(juce::Uuid().toString()),
      runtimeInstanceToken(nextRuntimeInstanceToken.fetch_add(1, std::memory_order_relaxed))
{
    parameters.addParameterListener("role", this);
    parameters.addParameterListener("masterLevel", this);
    parameters.addParameterListener("genericResponseTone", this);
    parameters.addParameterListener("listenerPositionX", this);
    parameters.addParameterListener("listenerPositionY", this);
    for (const auto* parameterId : kSpeakerParameterIds)
        parameters.addParameterListener(parameterId, this);
    for (const auto* parameterId : kSpeakerTypeParameterIds)
        parameters.addParameterListener(parameterId, this);
    for (const auto* parameterId : kSpeakerPositionXParameterIds)
        parameters.addParameterListener(parameterId, this);
    for (const auto* parameterId : kSpeakerPositionYParameterIds)
        parameters.addParameterListener(parameterId, this);

    rebuildLegacyDynamicSceneFromParameters();
    refreshSessionHandles();
    sourceName = isClubRole() ? "Club" : "Source";
    // Force reconcileRole() to perform initial role registration.
    lastKnownClubRole.store(!isClubRole(), std::memory_order_release);
    reconcileRole();
    startTimerHz(20);
}

ClubCraftPhase0AudioProcessor::~ClubCraftPhase0AudioProcessor()
{
    stopTimer();
    parameters.removeParameterListener("role", this);
    parameters.removeParameterListener("masterLevel", this);
    parameters.removeParameterListener("genericResponseTone", this);
    parameters.removeParameterListener("listenerPositionX", this);
    parameters.removeParameterListener("listenerPositionY", this);
    for (const auto* parameterId : kSpeakerParameterIds)
        parameters.removeParameterListener(parameterId, this);
    for (const auto* parameterId : kSpeakerTypeParameterIds)
        parameters.removeParameterListener(parameterId, this);
    for (const auto* parameterId : kSpeakerPositionXParameterIds)
        parameters.removeParameterListener(parameterId, this);
    for (const auto* parameterId : kSpeakerPositionYParameterIds)
        parameters.removeParameterListener(parameterId, this);

    auto& registry = clubcraft::SessionRegistry::instance();
    registry.unregisterClubPublisher(sessionId.toStdString(), runtimeInstanceToken);
    registry.unregisterSource(sessionId.toStdString(), sourceId.toStdString());
}

void ClubCraftPhase0AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    spatialRenderer.prepare(sampleRate, samplesPerBlock);
    sourceRouteRenderer.prepare(sampleRate, samplesPerBlock);
}

void ClubCraftPhase0AudioProcessor::releaseResources()
{
    spatialRenderer.reset();
    sourceRouteRenderer.reset();
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
        if (const auto scene = clubcraft::SessionRegistry::readRealtimeScene(dynamicSessionHandle))
        {
            buffer.applyGain(scene->masterLinearGain);
            return;
        }

        if (const auto legacy = clubcraft::SessionRegistry::readSnapshot(legacySessionHandle))
            buffer.applyGain(legacy->masterLinearGain);
        else
            buffer.applyGain(juce::Decibels::decibelsToGain(readParameter(parameters, "masterLevel")));
        return;
    }

    const auto scene = clubcraft::SessionRegistry::readRealtimeScene(dynamicSessionHandle);
    const auto routePlan = clubcraft::SessionRegistry::readSourceRoutePlan(sourceRouteHandle);
    if (scene.has_value() && routePlan.has_value() && scene->revision == routePlan->revision)
    {
        lastGoodScene = *scene;
        lastGoodRoutePlan = *routePlan;
        hasLastGoodRoutePlan = true;
        sourceRouteRenderer.render(buffer, lastGoodScene, lastGoodRoutePlan);
        return;
    }

    if (hasLastGoodRoutePlan)
    {
        sourceRouteRenderer.render(buffer, lastGoodScene, lastGoodRoutePlan);
        return;
    }

    // A SOURCE opened before its CLUB remains pass-through until a dynamic plan
    // arrives. If an old schema session publishes only the legacy snapshot, use
    // the existing renderer while migration is taking place.
    if (const auto legacy = clubcraft::SessionRegistry::readSnapshot(legacySessionHandle))
        spatialRenderer.render(buffer, *legacy);
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

bool ClubCraftPhase0AudioProcessor::acceptsMidi() const { return false; }
bool ClubCraftPhase0AudioProcessor::producesMidi() const { return false; }
bool ClubCraftPhase0AudioProcessor::isMidiEffect() const { return false; }
double ClubCraftPhase0AudioProcessor::getTailLengthSeconds() const { return 0.0; }
int ClubCraftPhase0AudioProcessor::getNumPrograms() { return 1; }
int ClubCraftPhase0AudioProcessor::getCurrentProgram() { return 0; }
void ClubCraftPhase0AudioProcessor::setCurrentProgram(int) {}
const juce::String ClubCraftPhase0AudioProcessor::getProgramName(int) { return {}; }
void ClubCraftPhase0AudioProcessor::changeProgramName(int, const juce::String&) {}

void ClubCraftPhase0AudioProcessor::getStateInformation(juce::MemoryBlock& destinationData)
{
    if (auto xml = makeSchema7State().createXml())
        copyXmlToBinary(*xml, destinationData);
}

void ClubCraftPhase0AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto& registry = clubcraft::SessionRegistry::instance();
    registry.unregisterClubPublisher(sessionId.toStdString(), runtimeInstanceToken);
    registry.unregisterSource(sessionId.toStdString(), sourceId.toStdString());
    clubPublisherRegistered = false;
    clubConflict.store(false, std::memory_order_release);

    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        const auto restoredState = juce::ValueTree::fromXml(*xml);
        if (restoredState.isValid())
        {
            if (restoredState.hasType(kSchema7StateType))
                restoreSchema7State(restoredState);
            else
                restoreLegacyState(restoredState);
        }
    }

    refreshSessionHandles();
    sourceName = isClubRole() ? "Club" : "Source";
    lastKnownClubRole.store(!isClubRole(), std::memory_order_release);
    snapshotDirty.store(true, std::memory_order_release);
    hasLastGoodRoutePlan = false;
    spatialRenderer.reset();
    sourceRouteRenderer.reset();
    reconcileRole();
}

juce::AudioProcessorValueTreeState& ClubCraftPhase0AudioProcessor::getParameters() noexcept { return parameters; }
const juce::String& ClubCraftPhase0AudioProcessor::getSessionId() const noexcept { return sessionId; }
const juce::String& ClubCraftPhase0AudioProcessor::getSourceId() const noexcept { return sourceId; }

bool ClubCraftPhase0AudioProcessor::isClubRole() const noexcept
{
    return readParameter(parameters, "role") >= 0.5f;
}

bool ClubCraftPhase0AudioProcessor::isConnectedToClub() const noexcept
{
    return clubcraft::SessionRegistry::readRealtimeScene(dynamicSessionHandle).has_value();
}

bool ClubCraftPhase0AudioProcessor::hasClubConflict() const noexcept
{
    return clubConflict.load(std::memory_order_acquire);
}

bool ClubCraftPhase0AudioProcessor::wasSourceRekeyed() const noexcept
{
    return sourceRekeyed.load(std::memory_order_acquire);
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
    parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
        "speakerType1", "Front L Type", juce::StringArray { "SUB", "WOOFER", "FULL RANGE", "MID", "HIGH" },
        clubcraft::speakerTypeToParameterIndex(clubcraft::SpeakerType::fullRange)));
    parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
        "speakerType2", "Front R Type", juce::StringArray { "SUB", "WOOFER", "FULL RANGE", "MID", "HIGH" },
        clubcraft::speakerTypeToParameterIndex(clubcraft::SpeakerType::fullRange)));
    parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
        "speakerType3", "Rear L Type", juce::StringArray { "SUB", "WOOFER", "FULL RANGE", "MID", "HIGH" },
        clubcraft::speakerTypeToParameterIndex(clubcraft::SpeakerType::fullRange)));
    parameterLayout.push_back(std::make_unique<juce::AudioParameterChoice>(
        "speakerType4", "Rear R Type", juce::StringArray { "SUB", "WOOFER", "FULL RANGE", "MID", "HIGH" },
        clubcraft::speakerTypeToParameterIndex(clubcraft::SpeakerType::fullRange)));
    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        "genericResponseTone", "Generic Response Tone", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));
    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        "speakerPositionX1", "Front L X", juce::NormalisableRange<float>(-15.0f, 15.0f, 0.01f), -6.0f));
    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        "speakerPositionY1", "Front L Y", juce::NormalisableRange<float>(-15.0f, 15.0f, 0.01f), 6.0f));
    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        "speakerPositionX2", "Front R X", juce::NormalisableRange<float>(-15.0f, 15.0f, 0.01f), 6.0f));
    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        "speakerPositionY2", "Front R Y", juce::NormalisableRange<float>(-15.0f, 15.0f, 0.01f), 6.0f));
    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        "speakerPositionX3", "Rear L X", juce::NormalisableRange<float>(-15.0f, 15.0f, 0.01f), -6.0f));
    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        "speakerPositionY3", "Rear L Y", juce::NormalisableRange<float>(-15.0f, 15.0f, 0.01f), -6.0f));
    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        "speakerPositionX4", "Rear R X", juce::NormalisableRange<float>(-15.0f, 15.0f, 0.01f), 6.0f));
    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        "speakerPositionY4", "Rear R Y", juce::NormalisableRange<float>(-15.0f, 15.0f, 0.01f), -6.0f));
    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        "listenerPositionX", "Listener X", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f));
    parameterLayout.push_back(std::make_unique<juce::AudioParameterFloat>(
        "listenerPositionY", "Listener Y", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f));
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
        synchroniseLegacyBridge();
        publishClubScenes();
    }
    else
    {
        registerAsSource();
    }
}

void ClubCraftPhase0AudioProcessor::refreshSessionHandles()
{
    auto& registry = clubcraft::SessionRegistry::instance();
    legacySessionHandle = registry.acquireSession(sessionId.toStdString());
    dynamicSessionHandle = registry.acquireDynamicSession(sessionId.toStdString());
    sourceRouteHandle = registry.acquireSourceRoute(sessionId.toStdString(), sourceId.toStdString());
}

void ClubCraftPhase0AudioProcessor::publishClubScenes()
{
    if (!isClubRole())
        return;

    auto& registry = clubcraft::SessionRegistry::instance();
    const auto publisher = registry.registerClubPublisher(sessionId.toStdString(), runtimeInstanceToken);
    clubPublisherRegistered = publisher.authoritative;
    clubConflict.store(publisher.conflict, std::memory_order_release);
    if (!publisher.authoritative)
        return;

    const auto sources = registry.getSourcesForSession(sessionId.toStdString());
    std::vector<std::string> sourceIds;
    sourceIds.reserve(sources.size());
    for (const auto& source : sources)
        sourceIds.push_back(source.sourceId);

    clubcraft::CompiledScene compiled;
    const auto nextRevision = revision.fetch_add(1, std::memory_order_relaxed) + 1;
    const auto compileError = sceneCompiler.compile(dynamicScene, sourceIds, nextRevision, compiled);
    if (compileError.hasError())
        return;

    if (!registry.publishRealtimeScene(sessionId.toStdString(), runtimeInstanceToken, compiled.realtimeScene))
        return;

    for (const auto& [compiledSourceId, routePlan] : compiled.sourcePlans)
    {
        const auto published = registry.publishSourceRoutePlan(
            sessionId.toStdString(), runtimeInstanceToken, compiledSourceId, routePlan);
        if (!published)
            return;
    }

    snapshotDirty.store(false, std::memory_order_release);
}

void ClubCraftPhase0AudioProcessor::registerAsSource()
{
    if (isClubRole())
        return;

    auto& registry = clubcraft::SessionRegistry::instance();
    const auto result = registry.registerSource({
        .sourceId = sourceId.toStdString(),
        .sessionId = sessionId.toStdString(),
        .displayName = sourceName.toStdString(),
        .position = { 0.0f, 0.0f },
        .heartbeat = revision.fetch_add(1, std::memory_order_relaxed) + 1,
        .runtimeInstanceToken = runtimeInstanceToken,
    });

    if (result.requiresRekey())
    {
        sourceId = juce::Uuid().toString();
        sourceRekeyed.store(true, std::memory_order_release);
        const auto retry = registry.registerSource({
            .sourceId = sourceId.toStdString(),
            .sessionId = sessionId.toStdString(),
            .displayName = sourceName.toStdString(),
            .position = { 0.0f, 0.0f },
            .heartbeat = revision.fetch_add(1, std::memory_order_relaxed) + 1,
            .runtimeInstanceToken = runtimeInstanceToken,
        });
        if (!retry.accepted())
            return;
    }

    refreshSessionHandles();
}

void ClubCraftPhase0AudioProcessor::reconcileRole()
{
    const auto currentRole = isClubRole();
    const auto previousRole = lastKnownClubRole.exchange(currentRole, std::memory_order_acq_rel);
    if (currentRole == previousRole)
        return;

    auto& registry = clubcraft::SessionRegistry::instance();
    if (previousRole)
        registry.unregisterClubPublisher(sessionId.toStdString(), runtimeInstanceToken);
    else
        registry.unregisterSource(sessionId.toStdString(), sourceId.toStdString());

    clubPublisherRegistered = false;
    clubConflict.store(false, std::memory_order_release);
    sourceName = currentRole ? "Club" : "Source";
    snapshotDirty.store(true, std::memory_order_release);
    hasLastGoodRoutePlan = false;
    spatialRenderer.reset();
    sourceRouteRenderer.reset();

    if (currentRole)
    {
        const auto publisher = registry.registerClubPublisher(sessionId.toStdString(), runtimeInstanceToken);
        clubPublisherRegistered = publisher.authoritative;
        clubConflict.store(publisher.conflict, std::memory_order_release);
        publishClubScenes();
    }
    else
    {
        registerAsSource();
    }
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

void ClubCraftPhase0AudioProcessor::rebuildLegacyDynamicSceneFromParameters()
{
    dynamicScene = {};
    dynamicScene.legacyDefaultRouting = true;
    dynamicScene.legacyRouteGain = 0.25f;
    dynamicScene.masterLevelDb = readParameter(parameters, "masterLevel");
    dynamicScene.genericResponseTone = readParameter(parameters, "genericResponseTone");
    dynamicScene.listener = {
        readParameter(parameters, "listenerPositionX"),
        readParameter(parameters, "listenerPositionY"),
    };

    dynamicScene.speakers.reserve(clubcraft::kSpeakerCount);
    for (std::size_t index = 0; index < clubcraft::kSpeakerCount; ++index)
    {
        dynamicScene.speakers.push_back({
            .stableId = legacySpeakerId(index).toStdString(),
            .name = kLegacySpeakerNames[index],
            .type = clubcraft::speakerTypeFromParameterIndex(
                static_cast<int>(readParameter(parameters, kSpeakerTypeParameterIds[index]))),
            .position = {
                readParameter(parameters, kSpeakerPositionXParameterIds[index]),
                readParameter(parameters, kSpeakerPositionYParameterIds[index]),
            },
            .levelDb = readParameter(parameters, kSpeakerParameterIds[index]),
            .enabled = true,
        });
    }
}

void ClubCraftPhase0AudioProcessor::synchroniseLegacyBridge()
{
    if (dynamicScene.speakers.empty())
        rebuildLegacyDynamicSceneFromParameters();

    dynamicScene.masterLevelDb = readParameter(parameters, "masterLevel");
    dynamicScene.genericResponseTone = readParameter(parameters, "genericResponseTone");
    dynamicScene.listener = {
        readParameter(parameters, "listenerPositionX"),
        readParameter(parameters, "listenerPositionY"),
    };

    const auto bridgedSpeakerCount = std::min(dynamicScene.speakers.size(), clubcraft::kSpeakerCount);
    for (std::size_t index = 0; index < bridgedSpeakerCount; ++index)
    {
        auto& speaker = dynamicScene.speakers[index];
        speaker.levelDb = readParameter(parameters, kSpeakerParameterIds[index]);
        speaker.type = clubcraft::speakerTypeFromParameterIndex(
            static_cast<int>(readParameter(parameters, kSpeakerTypeParameterIds[index])));
        speaker.position = {
            readParameter(parameters, kSpeakerPositionXParameterIds[index]),
            readParameter(parameters, kSpeakerPositionYParameterIds[index]),
        };
    }
}

juce::ValueTree ClubCraftPhase0AudioProcessor::makeSchema7State()
{
    juce::ValueTree state { kSchema7StateType };
    state.setProperty("schemaVersion", kStateSchemaVersion, nullptr);
    state.setProperty("sessionId", sessionId, nullptr);
    state.setProperty("sourceId", sourceId, nullptr);

    juce::ValueTree apvtsWrapper { kSchema7ApvtsType };
    apvtsWrapper.appendChild(parameters.copyState(), nullptr);
    state.appendChild(apvtsWrapper, nullptr);
    if (isClubRole())
        state.appendChild(clubcraft::scene_state::toValueTree(dynamicScene), nullptr);
    return state;
}

void ClubCraftPhase0AudioProcessor::restoreSchema7State(const juce::ValueTree& state)
{
    const auto apvtsWrapper = state.getChildWithName(kSchema7ApvtsType);
    if (apvtsWrapper.isValid() && apvtsWrapper.getNumChildren() == 1)
        parameters.replaceState(apvtsWrapper.getChild(0));

    sessionId = state.getProperty("sessionId", sessionId).toString();
    sourceId = state.getProperty("sourceId", sourceId).toString();

    if (const auto dynamicTree = state.getChildWithName(clubcraft::scene_state::kDynamicSceneTreeType);
        const auto restoredScene = clubcraft::scene_state::fromValueTree(dynamicTree))
        dynamicScene = *restoredScene;
    else
        rebuildLegacyDynamicSceneFromParameters();
}

void ClubCraftPhase0AudioProcessor::restoreLegacyState(const juce::ValueTree& state)
{
    parameters.replaceState(state);
    restoreLegacyPrimarySpeakerLevel(state);
    sessionId = state.getProperty("sessionId", sessionId).toString();
    sourceId = state.getProperty("sourceId", sourceId).toString();
    rebuildLegacyDynamicSceneFromParameters();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ClubCraftPhase0AudioProcessor();
}
