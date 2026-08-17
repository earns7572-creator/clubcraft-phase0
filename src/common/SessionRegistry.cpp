#include "SessionRegistry.h"

#include <algorithm>
#include <cmath>
#include <limits>
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

[[nodiscard]] std::uint64_t beginWrite(const std::atomic<std::uint64_t>& sequence) noexcept
{
    const auto before = sequence.load(std::memory_order_relaxed);
    return (before & 1U) == 0U ? before : before + 1U;
}
}

float LegacyRealtimeSceneSnapshot::normalizedFullSignalGain() const noexcept
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

DynamicSessionSlot::DynamicSessionSlot()
{
    for (std::size_t index = 0; index < kMaxSpeakers; ++index)
    {
        speakerActive[index].store(false, std::memory_order_relaxed);
        speakerGeneration[index].store(0, std::memory_order_relaxed);
        speakerTypeIndices[index].store(
            speakerTypeToParameterIndex(SpeakerType::fullRange), std::memory_order_relaxed);
        speakerLinearGains[index].store(1.0f, std::memory_order_relaxed);
        speakerPositionX[index].store(0.0f, std::memory_order_relaxed);
        speakerPositionY[index].store(0.0f, std::memory_order_relaxed);
    }
}

SourceRouteSlot::SourceRouteSlot()
{
    for (std::size_t index = 0; index < kMaxRoutesPerSource; ++index)
    {
        enabled[index].store(false, std::memory_order_relaxed);
        speakerSlot[index].store(0, std::memory_order_relaxed);
        speakerGeneration[index].store(0, std::memory_order_relaxed);
        modeIndices[index].store(static_cast<int>(RouteMode::full), std::memory_order_relaxed);
        inputModeIndices[index].store(static_cast<int>(InputChannelMode::sumMono), std::memory_order_relaxed);
        linearGains[index].store(1.0f, std::memory_order_relaxed);
        bandLowHz[index].store(20.0f, std::memory_order_relaxed);
        bandHighHz[index].store(20000.0f, std::memory_order_relaxed);
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

    const auto beforeWrite = beginWrite(slot->sequence);
    slot->sequence.store(beforeWrite + 1U, std::memory_order_release);

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
    slot->sequence.store(beforeWrite + 2U, std::memory_order_release);
}

SessionRegistry::SessionHandle SessionRegistry::acquireSession(const std::string& sessionId)
{
    if (sessionId.empty())
        return {};

    std::scoped_lock lock { mutex };
    if (const auto iterator = legacySessions.find(sessionId); iterator != legacySessions.end())
        return iterator->second;

    auto slot = std::make_shared<SessionSlot>();
    legacySessions.emplace(sessionId, slot);
    return slot;
}

std::optional<LegacyRealtimeSceneSnapshot> SessionRegistry::readSnapshot(const SessionHandle& handle) noexcept
{
    if (handle == nullptr || !handle->published.load(std::memory_order_acquire))
        return std::nullopt;

    for (int attempt = 0; attempt < 3; ++attempt)
    {
        const auto sequenceBefore = handle->sequence.load(std::memory_order_acquire);
        if ((sequenceBefore & 1U) != 0U)
            continue;

        LegacyRealtimeSceneSnapshot snapshot;
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

ClubPublisherStatus SessionRegistry::registerClubPublisher(const std::string& sessionId,
                                                            std::uint64_t runtimeInstanceToken)
{
    if (sessionId.empty() || runtimeInstanceToken == 0)
        return { false, true };

    std::scoped_lock lock { mutex };
    auto& publisher = clubPublishers[sessionId];
    publisher.registeredTokens.insert(runtimeInstanceToken);

    if (publisher.authoritativeToken == 0)
        publisher.authoritativeToken = runtimeInstanceToken;

    return {
        .authoritative = publisher.authoritativeToken == runtimeInstanceToken,
        .conflict = publisher.registeredTokens.size() > 1,
    };
}

void SessionRegistry::unregisterClubPublisher(const std::string& sessionId,
                                               std::uint64_t runtimeInstanceToken)
{
    std::scoped_lock lock { mutex };
    const auto iterator = clubPublishers.find(sessionId);
    if (iterator == clubPublishers.end())
        return;

    auto& publisher = iterator->second;
    publisher.registeredTokens.erase(runtimeInstanceToken);
    if (publisher.authoritativeToken == runtimeInstanceToken)
    {
        publisher.authoritativeToken = publisher.registeredTokens.empty()
            ? 0
            : *std::min_element(publisher.registeredTokens.begin(), publisher.registeredTokens.end());
    }

    if (publisher.registeredTokens.empty())
        clubPublishers.erase(iterator);
}

bool SessionRegistry::isClubPublisher(const std::string& sessionId,
                                      std::uint64_t runtimeInstanceToken) const
{
    std::scoped_lock lock { mutex };
    if (const auto iterator = clubPublishers.find(sessionId); iterator != clubPublishers.end())
        return iterator->second.authoritativeToken == runtimeInstanceToken;

    return false;
}

bool SessionRegistry::hasClubConflict(const std::string& sessionId) const
{
    std::scoped_lock lock { mutex };
    if (const auto iterator = clubPublishers.find(sessionId); iterator != clubPublishers.end())
        return iterator->second.registeredTokens.size() > 1;

    return false;
}

SessionRegistry::DynamicSessionHandle SessionRegistry::acquireDynamicSession(const std::string& sessionId)
{
    if (sessionId.empty())
        return {};

    std::scoped_lock lock { mutex };
    if (const auto iterator = dynamicSessions.find(sessionId); iterator != dynamicSessions.end())
        return iterator->second;

    auto slot = std::make_shared<DynamicSessionSlot>();
    dynamicSessions.emplace(sessionId, slot);
    return slot;
}

bool SessionRegistry::publishRealtimeScene(const std::string& sessionId,
                                           std::uint64_t publisherToken,
                                           const RealtimeSceneSnapshot& snapshot)
{
    if (!isClubPublisher(sessionId, publisherToken))
        return false;

    const auto slot = acquireDynamicSession(sessionId);
    if (slot == nullptr)
        return false;

    const auto beforeWrite = beginWrite(slot->sequence);
    slot->sequence.store(beforeWrite + 1U, std::memory_order_release);

    for (std::size_t index = 0; index < kMaxSpeakers; ++index)
    {
        const auto& speaker = snapshot.speakers[index];
        slot->speakerActive[index].store(speaker.active, std::memory_order_relaxed);
        slot->speakerGeneration[index].store(speaker.generation, std::memory_order_relaxed);
        slot->speakerTypeIndices[index].store(
            speakerTypeToParameterIndex(speaker.type), std::memory_order_relaxed);
        slot->speakerLinearGains[index].store(speaker.linearGain, std::memory_order_relaxed);
        slot->speakerPositionX[index].store(clampPosition(speaker.position.x), std::memory_order_relaxed);
        slot->speakerPositionY[index].store(clampPosition(speaker.position.y), std::memory_order_relaxed);
    }

    slot->activeSpeakerCount.store(snapshot.activeSpeakerCount, std::memory_order_relaxed);
    slot->listenerPositionX.store(clampPosition(snapshot.listener.x), std::memory_order_relaxed);
    slot->listenerPositionY.store(clampPosition(snapshot.listener.y), std::memory_order_relaxed);
    slot->masterLinearGain.store(snapshot.masterLinearGain, std::memory_order_relaxed);
    slot->genericResponseTone.store(
        std::clamp(snapshot.genericResponseTone, kMinResponseTone, kMaxResponseTone), std::memory_order_relaxed);
    slot->revision.store(snapshot.revision, std::memory_order_relaxed);
    slot->published.store(true, std::memory_order_release);
    slot->sequence.store(beforeWrite + 2U, std::memory_order_release);
    return true;
}

std::optional<RealtimeSceneSnapshot> SessionRegistry::readRealtimeScene(const DynamicSessionHandle& handle) noexcept
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
        snapshot.activeSpeakerCount = handle->activeSpeakerCount.load(std::memory_order_relaxed);
        snapshot.listener.x = handle->listenerPositionX.load(std::memory_order_relaxed);
        snapshot.listener.y = handle->listenerPositionY.load(std::memory_order_relaxed);
        snapshot.masterLinearGain = handle->masterLinearGain.load(std::memory_order_relaxed);
        snapshot.genericResponseTone = handle->genericResponseTone.load(std::memory_order_relaxed);
        for (std::size_t index = 0; index < kMaxSpeakers; ++index)
        {
            auto& speaker = snapshot.speakers[index];
            speaker.active = handle->speakerActive[index].load(std::memory_order_relaxed);
            speaker.generation = handle->speakerGeneration[index].load(std::memory_order_relaxed);
            speaker.type = speakerTypeFromParameterIndex(
                handle->speakerTypeIndices[index].load(std::memory_order_relaxed));
            speaker.linearGain = handle->speakerLinearGains[index].load(std::memory_order_relaxed);
            speaker.position.x = handle->speakerPositionX[index].load(std::memory_order_relaxed);
            speaker.position.y = handle->speakerPositionY[index].load(std::memory_order_relaxed);
        }

        const auto sequenceAfter = handle->sequence.load(std::memory_order_acquire);
        if (sequenceBefore == sequenceAfter && (sequenceAfter & 1U) == 0U)
            return snapshot;
    }

    return std::nullopt;
}

SourceRegistrationResult SessionRegistry::registerSource(SourceRegistration registration)
{
    if (registration.sourceId.empty() || registration.sessionId.empty())
        return { SourceRegistrationStatus::invalid };

    std::scoped_lock lock { mutex };
    auto& sources = sourcesBySession[registration.sessionId];
    const auto existing = sources.find(registration.sourceId);
    if (existing != sources.end())
    {
        const auto sameRuntimeInstance = registration.runtimeInstanceToken != 0
            && registration.runtimeInstanceToken == existing->second.runtimeInstanceToken;
        const auto legacyHeartbeat = registration.runtimeInstanceToken == 0
            && existing->second.runtimeInstanceToken == 0;
        if (!sameRuntimeInstance && !legacyHeartbeat)
            return { SourceRegistrationStatus::rekeyRequired };

        existing->second = std::move(registration);
        return { SourceRegistrationStatus::accepted };
    }

    if (sources.size() >= kMaxSources)
        return { SourceRegistrationStatus::invalid };

    const auto sessionId = registration.sessionId;
    const auto sourceId = registration.sourceId;
    sources.emplace(sourceId, std::move(registration));
    routeSlotsBySession[sessionId].try_emplace(sourceId, std::make_shared<SourceRouteSlot>());
    return { SourceRegistrationStatus::accepted };
}

void SessionRegistry::unregisterSource(const std::string& sessionId, const std::string& sourceId)
{
    std::scoped_lock lock { mutex };
    if (const auto iterator = sourcesBySession.find(sessionId); iterator != sourcesBySession.end())
    {
        iterator->second.erase(sourceId);
        if (iterator->second.empty())
            sourcesBySession.erase(iterator);
    }

    if (const auto iterator = routeSlotsBySession.find(sessionId); iterator != routeSlotsBySession.end())
    {
        iterator->second.erase(sourceId);
        if (iterator->second.empty())
            routeSlotsBySession.erase(iterator);
    }
}

void SessionRegistry::unregisterSource(const std::string& sourceId)
{
    std::scoped_lock lock { mutex };
    for (auto sourceIterator = sourcesBySession.begin(); sourceIterator != sourcesBySession.end();)
    {
        sourceIterator->second.erase(sourceId);
        if (sourceIterator->second.empty())
            sourceIterator = sourcesBySession.erase(sourceIterator);
        else
            ++sourceIterator;
    }

    for (auto routeIterator = routeSlotsBySession.begin(); routeIterator != routeSlotsBySession.end();)
    {
        routeIterator->second.erase(sourceId);
        if (routeIterator->second.empty())
            routeIterator = routeSlotsBySession.erase(routeIterator);
        else
            ++routeIterator;
    }
}

std::optional<SourceRegistration> SessionRegistry::getSource(const std::string& sessionId,
                                                              const std::string& sourceId) const
{
    std::scoped_lock lock { mutex };
    if (const auto session = sourcesBySession.find(sessionId); session != sourcesBySession.end())
    {
        if (const auto source = session->second.find(sourceId); source != session->second.end())
            return source->second;
    }
    return std::nullopt;
}

std::optional<SourceRegistration> SessionRegistry::getSource(const std::string& sourceId) const
{
    std::scoped_lock lock { mutex };
    for (const auto& [sessionId, sources] : sourcesBySession)
    {
        (void) sessionId;
        if (const auto source = sources.find(sourceId); source != sources.end())
            return source->second;
    }
    return std::nullopt;
}

std::vector<SourceRegistration> SessionRegistry::getSourcesForSession(const std::string& sessionId) const
{
    std::scoped_lock lock { mutex };
    std::vector<SourceRegistration> result;
    if (const auto session = sourcesBySession.find(sessionId); session != sourcesBySession.end())
    {
        result.reserve(session->second.size());
        for (const auto& [sourceId, source] : session->second)
        {
            (void) sourceId;
            result.push_back(source);
        }
    }
    return result;
}

SessionRegistry::SourceRouteHandle SessionRegistry::acquireSourceRoute(const std::string& sessionId,
                                                                        const std::string& sourceId)
{
    if (sessionId.empty() || sourceId.empty())
        return {};

    std::scoped_lock lock { mutex };
    const auto session = routeSlotsBySession.find(sessionId);
    if (session == routeSlotsBySession.end())
        return {};
    if (const auto source = session->second.find(sourceId); source != session->second.end())
        return source->second;
    return {};
}

bool SessionRegistry::publishSourceRoutePlan(const std::string& sessionId,
                                             std::uint64_t publisherToken,
                                             const std::string& sourceId,
                                             const SourceRoutePlan& plan)
{
    if (!isClubPublisher(sessionId, publisherToken))
        return false;

    const auto slot = acquireSourceRoute(sessionId, sourceId);
    if (slot == nullptr)
        return false;

    const auto beforeWrite = beginWrite(slot->sequence);
    slot->sequence.store(beforeWrite + 1U, std::memory_order_release);
    for (std::size_t index = 0; index < kMaxRoutesPerSource; ++index)
    {
        const auto isActiveRoute = index < plan.routeCount;
        const auto route = isActiveRoute ? plan.routes[index] : CompiledRoute {};
        slot->enabled[index].store(isActiveRoute && route.enabled, std::memory_order_relaxed);
        slot->speakerSlot[index].store(route.speakerSlot, std::memory_order_relaxed);
        slot->speakerGeneration[index].store(route.speakerGeneration, std::memory_order_relaxed);
        slot->modeIndices[index].store(static_cast<int>(route.mode), std::memory_order_relaxed);
        slot->inputModeIndices[index].store(static_cast<int>(route.inputMode), std::memory_order_relaxed);
        slot->linearGains[index].store(route.linearGain, std::memory_order_relaxed);
        slot->bandLowHz[index].store(route.bandLowHz, std::memory_order_relaxed);
        slot->bandHighHz[index].store(route.bandHighHz, std::memory_order_relaxed);
    }
    slot->routeCount.store(plan.routeCount, std::memory_order_relaxed);
    slot->revision.store(plan.revision, std::memory_order_relaxed);
    slot->published.store(true, std::memory_order_release);
    slot->sequence.store(beforeWrite + 2U, std::memory_order_release);
    return true;
}

std::optional<SourceRoutePlan> SessionRegistry::readSourceRoutePlan(const SourceRouteHandle& handle) noexcept
{
    if (handle == nullptr || !handle->published.load(std::memory_order_acquire))
        return std::nullopt;

    for (int attempt = 0; attempt < 3; ++attempt)
    {
        const auto sequenceBefore = handle->sequence.load(std::memory_order_acquire);
        if ((sequenceBefore & 1U) != 0U)
            continue;

        SourceRoutePlan plan;
        plan.revision = handle->revision.load(std::memory_order_relaxed);
        plan.routeCount = std::min<std::uint16_t>(
            handle->routeCount.load(std::memory_order_relaxed), static_cast<std::uint16_t>(kMaxRoutesPerSource));
        for (std::size_t index = 0; index < plan.routeCount; ++index)
        {
            auto& route = plan.routes[index];
            route.enabled = handle->enabled[index].load(std::memory_order_relaxed);
            route.speakerSlot = handle->speakerSlot[index].load(std::memory_order_relaxed);
            route.speakerGeneration = handle->speakerGeneration[index].load(std::memory_order_relaxed);
            route.mode = static_cast<RouteMode>(handle->modeIndices[index].load(std::memory_order_relaxed));
            route.inputMode = static_cast<InputChannelMode>(handle->inputModeIndices[index].load(std::memory_order_relaxed));
            route.linearGain = handle->linearGains[index].load(std::memory_order_relaxed);
            route.bandLowHz = handle->bandLowHz[index].load(std::memory_order_relaxed);
            route.bandHighHz = handle->bandHighHz[index].load(std::memory_order_relaxed);
        }

        const auto sequenceAfter = handle->sequence.load(std::memory_order_acquire);
        if (sequenceBefore == sequenceAfter && (sequenceAfter & 1U) == 0U)
            return plan;
    }

    return std::nullopt;
}

void SessionRegistry::resetForTests()
{
    std::scoped_lock lock { mutex };
    legacySessions.clear();
    dynamicSessions.clear();
    sourcesBySession.clear();
    routeSlotsBySession.clear();
    clubPublishers.clear();
}

} // namespace clubcraft
