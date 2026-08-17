#include "SessionRegistry.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

namespace clubcraft
{

namespace
{
constexpr float kMinLevelDb = -96.0f;
constexpr float kMaxLevelDb = 24.0f;
constexpr float kMinResponseTone = 0.0f;
constexpr float kMaxResponseTone = 1.0f;
constexpr float kMinPosition = -30.0f;
constexpr float kMaxPosition = 30.0f;

[[nodiscard]] float decibelsToLinearGain(float levelDb) noexcept
{
    const auto clampedDb = std::clamp(levelDb, kMinLevelDb, kMaxLevelDb);
    return std::pow(10.0f, clampedDb / 20.0f);
}

[[nodiscard]] float clampPosition(float value) noexcept
{
    return std::clamp(value, kMinPosition, kMaxPosition);
}
}

float RealtimeSceneSnapshot::normalizedFullSignalGain() const noexcept
{
    const auto totalGain = std::accumulate(speakerLinearGains.begin(), speakerLinearGains.end(), 0.0f);
    return totalGain / static_cast<float>(speakerLinearGains.size());
}

SessionSlot::SessionSlot()
{
    constexpr std::array<PlanarPosition, kSpeakerCount> defaultPositions {
        PlanarPosition { -6.0f, 6.0f },
        PlanarPosition { 6.0f, 6.0f },
        PlanarPosition { -6.0f, -6.0f },
        PlanarPosition { 6.0f, -6.0f },
    };

    for (std::size_t index = 0; index < kSpeakerCount; ++index)
    {
        speakerLinearGains[index].store(1.0f, std::memory_order_relaxed);
        speakerPositionX[index].store(defaultPositions[index].x, std::memory_order_relaxed);
        speakerPositionY[index].store(defaultPositions[index].y, std::memory_order_relaxed);
        speakerTypeIndices[index].store(
            speakerTypeToParameterIndex(SpeakerType::fullRange), std::memory_order_relaxed);
    }
}

SessionRegistry& SessionRegistry::instance()
{
    static SessionRegistry registry;
    return registry;
}

void SessionRegistry::publishSnapshot(const SceneSnapshot& snapshot)
{
    if (snapshot.sessionId.empty())
        return;

    const auto slot = acquireSession(snapshot.sessionId);
    if (slot == nullptr)
        return;

    const auto beforeWrite = slot->sequence.load(std::memory_order_relaxed);
    slot->sequence.store(beforeWrite + 1, std::memory_order_release);

    slot->masterLinearGain.store(decibelsToLinearGain(snapshot.masterLevelDb), std::memory_order_relaxed);
    for (std::size_t index = 0; index < kSpeakerCount; ++index)
    {
        slot->speakerLinearGains[index].store(decibelsToLinearGain(snapshot.speakerLevelDb[index]), std::memory_order_relaxed);
        slot->speakerPositionX[index].store(clampPosition(snapshot.speakerPositions[index].x), std::memory_order_relaxed);
        slot->speakerPositionY[index].store(clampPosition(snapshot.speakerPositions[index].y), std::memory_order_relaxed);
        slot->speakerTypeIndices[index].store(
            speakerTypeToParameterIndex(snapshot.speakerTypes[index]), std::memory_order_relaxed);
    }

    slot->listenerPositionX.store(clampPosition(snapshot.listenerPosition.x), std::memory_order_relaxed);
    slot->listenerPositionY.store(clampPosition(snapshot.listenerPosition.y), std::memory_order_relaxed);
    slot->genericResponseTone.store(
        std::clamp(snapshot.genericResponseTone, kMinResponseTone, kMaxResponseTone), std::memory_order_relaxed);
    slot->revision.store(snapshot.revision, std::memory_order_relaxed);
    slot->published.store(true, std::memory_order_release);
    slot->sequence.store(beforeWrite + 2, std::memory_order_release);
}

SessionRegistry::SessionHandle SessionRegistry::acquireSession(const std::string& sessionId)
{
    if (sessionId.empty())
        return {};

    std::scoped_lock lock { mutex };

    if (const auto iterator = sessions.find(sessionId); iterator != sessions.end())
        return iterator->second;

    auto slot = std::make_shared<SessionSlot>();
    sessions.emplace(sessionId, slot);
    return slot;
}

std::optional<RealtimeSceneSnapshot> SessionRegistry::readSnapshot(const SessionHandle& handle) noexcept
{
    if (handle == nullptr || !handle->published.load(std::memory_order_acquire))
        return std::nullopt;

    for (int attempt = 0; attempt < 3; ++attempt)
    {
        const auto sequenceBefore = handle->sequence.load(std::memory_order_acquire);
        if ((sequenceBefore & 1U) != 0U)
            continue;

        RealtimeSceneSnapshot snapshot;
        snapshot.revision = handle->revision.load(std::memory_order_relaxed);
        snapshot.masterLinearGain = handle->masterLinearGain.load(std::memory_order_relaxed);
        for (std::size_t index = 0; index < kSpeakerCount; ++index)
        {
            snapshot.speakerLinearGains[index] = handle->speakerLinearGains[index].load(std::memory_order_relaxed);
            snapshot.speakerPositions[index].x = handle->speakerPositionX[index].load(std::memory_order_relaxed);
            snapshot.speakerPositions[index].y = handle->speakerPositionY[index].load(std::memory_order_relaxed);
            snapshot.speakerTypes[index] = speakerTypeFromParameterIndex(
                handle->speakerTypeIndices[index].load(std::memory_order_relaxed));
        }
        snapshot.listenerPosition.x = handle->listenerPositionX.load(std::memory_order_relaxed);
        snapshot.listenerPosition.y = handle->listenerPositionY.load(std::memory_order_relaxed);
        snapshot.genericResponseTone = handle->genericResponseTone.load(std::memory_order_relaxed);

        const auto sequenceAfter = handle->sequence.load(std::memory_order_acquire);
        if (sequenceBefore == sequenceAfter && (sequenceAfter & 1U) == 0U)
            return snapshot;
    }

    return std::nullopt;
}

void SessionRegistry::registerSource(SourceRegistration registration)
{
    if (registration.sourceId.empty() || registration.sessionId.empty())
        return;

    std::scoped_lock lock { mutex };
    sources.insert_or_assign(registration.sourceId, std::move(registration));
}

void SessionRegistry::unregisterSource(const std::string& sourceId)
{
    std::scoped_lock lock { mutex };
    sources.erase(sourceId);
}

std::optional<SourceRegistration> SessionRegistry::getSource(const std::string& sourceId) const
{
    std::scoped_lock lock { mutex };

    if (const auto iterator = sources.find(sourceId); iterator != sources.end())
        return iterator->second;

    return std::nullopt;
}

void SessionRegistry::resetForTests()
{
    std::scoped_lock lock { mutex };
    sessions.clear();
    sources.clear();
}

} // namespace clubcraft
