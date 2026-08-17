#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

#include "SessionRegistry.h"

namespace clubcraft::spatial
{

inline constexpr float kSpeedOfSoundMetersPerSecond = 343.0f;
inline constexpr float kMaximumRelativeDelaySeconds = 0.060f;

struct StereoGains
{
    float left = 0.70710678f;
    float right = 0.70710678f;
};

[[nodiscard]] inline float distance(PlanarPosition first, PlanarPosition second) noexcept
{
    const auto dx = first.x - second.x;
    const auto dy = first.y - second.y;
    return std::sqrt(dx * dx + dy * dy);
}

[[nodiscard]] inline StereoGains constantPowerPan(float pan) noexcept
{
    const auto clampedPan = std::clamp(pan, -1.0f, 1.0f);
    const auto angle = (clampedPan + 1.0f) * 0.25f * std::numbers::pi_v<float>;
    return { std::cos(angle), std::sin(angle) };
}

/**
    Pan a Speaker from the Listener's point of view. A Speaker directly beside
    the Listener reaches the corresponding hard side; a Speaker directly in
    front or behind is centred in the stereo simulation.
*/
[[nodiscard]] inline StereoGains panForSpeakerAndListener(PlanarPosition speaker,
                                                           PlanarPosition listener) noexcept
{
    constexpr float kMinimumDistance = 0.25f;
    const auto relativeX = speaker.x - listener.x;
    return constantPowerPan(relativeX / std::max(distance(speaker, listener), kMinimumDistance));
}

[[nodiscard]] inline float gainForRelativePath(float referencePath, float path) noexcept
{
    constexpr float kMinimumPath = 0.25f;
    return std::clamp(referencePath / std::max(path, kMinimumPath), 0.0f, 1.0f);
}

[[nodiscard]] inline float toneForRelativePath(float genericTone, float referencePath, float path) noexcept
{
    const auto pathRatio = gainForRelativePath(referencePath, path);
    return std::clamp(genericTone * std::sqrt(pathRatio), 0.03f, 1.0f);
}

[[nodiscard]] inline float relativeDelaySeconds(float path, float shortestPath) noexcept
{
    const auto pathDelta = std::max(0.0f, path - shortestPath);
    return std::min(pathDelta / kSpeedOfSoundMetersPerSecond, kMaximumRelativeDelaySeconds);
}

} // namespace clubcraft::spatial
