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

#include "SpeakerType.h"

namespace clubcraft
{

inline constexpr std::size_t kSpeakerCount = 4;
// Retained for Phase 1 source compatibility while callers migrate to kSpeakerCount.
inline constexpr std::size_t kPhase1SpeakerCount = kSpeakerCount;

struct PlanarPosition
{
    float x = 0.0f;
    float y = 0.0f;
};

/**
    Non-realtime control data submitted by the CLUB instance.
    Audio buffers never enter the registry.
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

/** A value-only snapshot that is safe to copy inside an audio callback. */
struct RealtimeSceneSnapshot
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
};

/**
    Process-local storage holding only primitive atomics on the realtime path.

    This intentionally avoids std::atomic<std::shared_ptr<T>>, which requires
    newer C++ library support than some Intel macOS toolchains provide.
*/
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

/**
    Best-effort, process-local registry for the SOURCE / CLUB handshake.

    Source instances acquire a SessionHandle outside the audio callback and use
    readSnapshot(handle) from processBlock(). That operation only reads primitive
    atomics; it does not lock, allocate, wait, or require C++20 atomic shared_ptr.
*/
class SessionRegistry final
{
public:
    using SessionHandle = std::shared_ptr<SessionSlot>;

    static SessionRegistry& instance();

    void publishSnapshot(const SceneSnapshot& snapshot);
    [[nodiscard]] SessionHandle acquireSession(const std::string& sessionId);
    [[nodiscard]] static std::optional<RealtimeSceneSnapshot> readSnapshot(const SessionHandle& handle) noexcept;

    void registerSource(SourceRegistration registration);
    void unregisterSource(const std::string& sourceId);
    [[nodiscard]] std::optional<SourceRegistration> getSource(const std::string& sourceId) const;

    void resetForTests();

private:
    SessionRegistry() = default;

    mutable std::mutex mutex;
    std::unordered_map<std::string, SessionHandle> sessions;
    std::unordered_map<std::string, SourceRegistration> sources;
};

} // namespace clubcraft
