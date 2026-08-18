#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "RealtimePlan.h"
#include "SceneTypes.h"
#include "SpeakerType.h"

namespace clubcraft
{

// Legacy Phase 1–0.5.0 fixed speaker count. New 0.6.0 code uses kMaxSpeakers.
inline constexpr std::size_t kSpeakerCount = 4;
inline constexpr std::size_t kPhase1SpeakerCount = kSpeakerCount;

/**
    Legacy control data submitted by a 0.5.0 CLUB. Audio buffers never enter
    the registry. It is retained solely while schema <= 6 projects migrate.
*/
struct SceneSnapshot
{
    std::string sessionId;
    std::uint64_t revision = 0;
    float masterLevelDb = 0.0f;
    std::array<float, kSpeakerCount> speakerLevelDb { 0.0f, 0.0f, 0.0f, 0.0f };
    std::array<PlanarPosition, kSpeakerCount> speakerPositions {
        PlanarPosition { -6.0f, 6.0f },
        PlanarPosition { 6.0f, 6.0f },
        PlanarPosition { -6.0f, -6.0f },
        PlanarPosition { 6.0f, -6.0f },
    };
    float genericResponseTone = 1.0f;
    PlanarPosition listenerPosition { 0.0f, 0.0f };
    std::array<SpeakerType, kSpeakerCount> speakerTypes {
        SpeakerType::fullRange,
        SpeakerType::fullRange,
        SpeakerType::fullRange,
        SpeakerType::fullRange,
    };
};

/** Value-only legacy snapshot safe to copy in an audio callback. */
struct LegacyRealtimeSceneSnapshot
{
    std::uint64_t revision = 0;
    float masterLinearGain = 1.0f;
    std::array<float, kSpeakerCount> speakerLinearGains { 1.0f, 1.0f, 1.0f, 1.0f };
    std::array<PlanarPosition, kSpeakerCount> speakerPositions {
        PlanarPosition { -6.0f, 6.0f },
        PlanarPosition { 6.0f, 6.0f },
        PlanarPosition { -6.0f, -6.0f },
        PlanarPosition { 6.0f, -6.0f },
    };
    float genericResponseTone = 1.0f;
    PlanarPosition listenerPosition { 0.0f, 0.0f };
    std::array<SpeakerType, kSpeakerCount> speakerTypes {
        SpeakerType::fullRange,
        SpeakerType::fullRange,
        SpeakerType::fullRange,
        SpeakerType::fullRange,
    };

    [[nodiscard]] float normalizedFullSignalGain() const noexcept;
};

struct SourceRegistration
{
    std::string sourceId;
    std::string sessionId;
    std::string displayName;
    PlanarPosition position;
    std::uint64_t heartbeat = 0;
    // Runtime-only identity. It is not part of the plugin state and lets the
    // registry distinguish normal heartbeats from a duplicated DAW instance.
    std::uint64_t runtimeInstanceToken = 0;
};

enum class SourceRegistrationStatus
{
    accepted,
    rekeyRequired,
    invalid,
};

struct SourceRegistrationResult
{
    SourceRegistrationStatus status = SourceRegistrationStatus::invalid;

    [[nodiscard]] bool accepted() const noexcept { return status == SourceRegistrationStatus::accepted; }
    [[nodiscard]] bool requiresRekey() const noexcept { return status == SourceRegistrationStatus::rekeyRequired; }
};

struct ClubPublisherStatus
{
    bool authoritative = false;
    bool conflict = false;
};

/** Process-local primitive-atomic slot for legacy 4 Speaker snapshots. */
struct SessionSlot
{
    std::atomic<std::uint64_t> sequence { 0 };
    std::atomic<std::uint64_t> revision { 0 };
    std::atomic<float> masterLinearGain { 1.0f };
    std::array<std::atomic<float>, kSpeakerCount> speakerLinearGains;
    std::array<std::atomic<float>, kSpeakerCount> speakerPositionX;
    std::array<std::atomic<float>, kSpeakerCount> speakerPositionY;
    std::array<std::atomic<int>, kSpeakerCount> speakerTypeIndices;
    std::atomic<float> listenerPositionX { 0.0f };
    std::atomic<float> listenerPositionY { 0.0f };
    std::atomic<float> genericResponseTone { 1.0f };
    std::atomic<bool> published { false };

    SessionSlot();
};

/** Primitive-atomic fixed-capacity Dynamic Scene for the 0.6 audio path. */
struct DynamicSessionSlot
{
    std::atomic<std::uint64_t> sequence { 0 };
    std::atomic<std::uint64_t> revision { 0 };
    std::array<std::atomic<bool>, kMaxSpeakers> speakerActive;
    std::array<std::atomic<std::uint32_t>, kMaxSpeakers> speakerGeneration;
    std::array<std::atomic<int>, kMaxSpeakers> speakerTypeIndices;
    std::array<std::atomic<float>, kMaxSpeakers> speakerLinearGains;
    std::array<std::atomic<float>, kMaxSpeakers> speakerPositionX;
    std::array<std::atomic<float>, kMaxSpeakers> speakerPositionY;
    std::atomic<std::uint16_t> activeSpeakerCount { 0 };
    std::atomic<float> listenerPositionX { 0.0f };
    std::atomic<float> listenerPositionY { 0.0f };
    std::atomic<float> masterLinearGain { 1.0f };
    std::atomic<float> genericResponseTone { 1.0f };
    std::atomic<bool> published { false };

    DynamicSessionSlot();
};

/** Primitive-atomic source-local RoutePlan for the 0.6 audio path. */
struct SourceRouteSlot
{
    std::atomic<std::uint64_t> sequence { 0 };
    std::atomic<std::uint64_t> revision { 0 };
    std::atomic<std::uint16_t> routeCount { 0 };
    std::array<std::atomic<bool>, kMaxRoutesPerSource> enabled;
    std::array<std::atomic<std::uint16_t>, kMaxRoutesPerSource> speakerSlot;
    std::array<std::atomic<std::uint32_t>, kMaxRoutesPerSource> speakerGeneration;
    std::array<std::atomic<int>, kMaxRoutesPerSource> modeIndices;
    std::array<std::atomic<int>, kMaxRoutesPerSource> inputModeIndices;
    std::array<std::atomic<float>, kMaxRoutesPerSource> linearGains;
    std::array<std::atomic<float>, kMaxRoutesPerSource> bandLowHz;
    std::array<std::atomic<float>, kMaxRoutesPerSource> bandHighHz;
    std::atomic<bool> published { false };

    SourceRouteSlot();
};

/**
    Process-local SOURCE / CLUB registry. Control-side maps are protected by a
    mutex. The audio path reads only already-acquired shared handles containing
    primitive atomics; it never locks, allocates, waits, or performs map lookup.
*/
class SessionRegistry final
{
public:
    using SessionHandle = std::shared_ptr<SessionSlot>;
    using DynamicSessionHandle = std::shared_ptr<DynamicSessionSlot>;
    using SourceRouteHandle = std::shared_ptr<SourceRouteSlot>;

    static SessionRegistry& instance();

    // Legacy 0.5 API retained for old project compatibility during migration.
    void publishSnapshot(const SceneSnapshot& snapshot);
    [[nodiscard]] SessionHandle acquireSession(const std::string& sessionId);
    [[nodiscard]] static std::optional<LegacyRealtimeSceneSnapshot> readSnapshot(const SessionHandle& handle) noexcept;

    // 0.6 authoritative CLUB publisher lifecycle.
    [[nodiscard]] ClubPublisherStatus registerClubPublisher(const std::string& sessionId,
                                                             std::uint64_t runtimeInstanceToken);
    void unregisterClubPublisher(const std::string& sessionId, std::uint64_t runtimeInstanceToken);
    [[nodiscard]] bool isClubPublisher(const std::string& sessionId,
                                       std::uint64_t runtimeInstanceToken) const;
    [[nodiscard]] bool hasClubConflict(const std::string& sessionId) const;

    [[nodiscard]] DynamicSessionHandle acquireDynamicSession(const std::string& sessionId);
    [[nodiscard]] bool publishRealtimeScene(const std::string& sessionId,
                                             std::uint64_t publisherToken,
                                             const RealtimeSceneSnapshot& snapshot);
    [[nodiscard]] static std::optional<RealtimeSceneSnapshot> readRealtimeScene(
        const DynamicSessionHandle& handle) noexcept;

    [[nodiscard]] SourceRegistrationResult registerSource(SourceRegistration registration);
    void unregisterSource(const std::string& sessionId, const std::string& sourceId);
    // Legacy global lookup/removal aliases. New code always supplies sessionId.
    void unregisterSource(const std::string& sourceId);
    [[nodiscard]] std::optional<SourceRegistration> getSource(const std::string& sessionId,
                                                               const std::string& sourceId) const;
    [[nodiscard]] std::optional<SourceRegistration> getSource(const std::string& sourceId) const;
    [[nodiscard]] std::vector<SourceRegistration> getSourcesForSession(const std::string& sessionId) const;
    // Control-side membership generation. CLUB compares this value before
    // compiling, so ordinary heartbeat ticks do not trigger SceneCompiler.
    [[nodiscard]] std::uint64_t getSourceMembershipRevision(const std::string& sessionId) const;

    [[nodiscard]] SourceRouteHandle acquireSourceRoute(const std::string& sessionId,
                                                        const std::string& sourceId);
    [[nodiscard]] bool publishSourceRoutePlan(const std::string& sessionId,
                                               std::uint64_t publisherToken,
                                               const std::string& sourceId,
                                               const SourceRoutePlan& plan);
    [[nodiscard]] static std::optional<SourceRoutePlan> readSourceRoutePlan(
        const SourceRouteHandle& handle) noexcept;

    void resetForTests();

private:
    SessionRegistry() = default;

    struct ClubPublisherState
    {
        std::uint64_t authoritativeToken = 0;
        std::unordered_set<std::uint64_t> registeredTokens;
    };

    mutable std::mutex mutex;
    std::unordered_map<std::string, SessionHandle> legacySessions;
    std::unordered_map<std::string, DynamicSessionHandle> dynamicSessions;
    std::unordered_map<std::string, std::unordered_map<std::string, SourceRegistration>> sourcesBySession;
    std::unordered_map<std::string, std::uint64_t> sourceMembershipRevisions;
    std::unordered_map<std::string, std::unordered_map<std::string, SourceRouteHandle>> routeSlotsBySession;
    std::unordered_map<std::string, ClubPublisherState> clubPublishers;
};

} // namespace clubcraft
