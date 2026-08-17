#include "SessionRegistry.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace clubcraft
{

namespace
{
constexpr float kMinLevelDb = -96.0f;
constexpr float kMaxLevelDb = 24.0f;

[[nodiscard]] float decibelsToLinearGain(float levelDb) noexcept
{
    const auto clampedDb = std::clamp(levelDb, kMinLevelDb, kMaxLevelDb);
    return std::pow(10.0f, clampedDb / 20.0f);
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
    slot->primarySpeakerLinearGain.store(decibelsToLinearGain(snapshot.primarySpeakerLevelDb), std::memory_order_relaxed);
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

        const RealtimeSceneSnapshot snapshot {
            .revision = handle->revision.load(std::memory_order_relaxed),
            .masterLinearGain = handle->masterLinearGain.load(std::memory_order_relaxed),
            .primarySpeakerLinearGain = handle->primarySpeakerLinearGain.load(std::memory_order_relaxed),
        };

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
