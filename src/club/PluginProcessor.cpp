#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace
{
constexpr int kStateSchemaVersion = 2;

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
      parameters(*this, nullptr, juce::Identifier("ClubCraftPhase0State"), createParameterLayout()),
      sourceId(juce::Uuid().toString())
{
    parameters.addParameterListener("role", this);
    parameters.addParameterListener("masterLevel", this);
    parameters.addParameterListener("primarySpeakerLevel", this);

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
    parameters.removeParameterListener("primarySpeakerLevel", this);
    clubcraft::SessionRegistry::instance().unregisterSource(sourceId.toStdString());
}

void ClubCraftPhase0AudioProcessor::prepareToPlay(double, int)
{
}

void ClubCraftPhase0AudioProcessor::releaseResources()
{
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
        buffer.applyGain(snapshot->primarySpeakerLinearGain);
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
            sessionId = restoredState.getProperty("sessionId", sessionId).toString();
            sourceId = restoredState.getProperty("sourceId", sourceId).toString();
        }
    }

    refreshSessionHandle();
    sourceName = isClubRole() ? "Club" : "Source";
    lastKnownClubRole.store(!isClubRole(), std::memory_order_release);
    snapshotDirty.store(true, std::memory_order_release);
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
        "primarySpeakerLevel", "Primary Speaker Level", juce::NormalisableRange<float>(-60.0f, 12.0f, 0.01f), 0.0f));

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

    auto snapshot = std::make_shared<clubcraft::SceneSnapshot>();
    snapshot->sessionId = sessionId.toStdString();
    snapshot->revision = revision.fetch_add(1, std::memory_order_relaxed) + 1;
    snapshot->masterLevelDb = readParameter(parameters, "masterLevel");
    snapshot->primarySpeakerLevelDb = readParameter(parameters, "primarySpeakerLevel");

    clubcraft::SessionRegistry::instance().publishSnapshot(*snapshot);
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

    if (currentRole)
        publishClubSnapshot();
    else
        registerAsSource();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ClubCraftPhase0AudioProcessor();
}
