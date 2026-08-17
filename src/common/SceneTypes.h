#pragma once

#include <cstddef>
#include <cstdint>

namespace clubcraft
{

// V1 capacity limits. Control-side code rejects over-capacity Scenes rather
// than silently dropping routes; realtime code never allocates to grow them.
inline constexpr std::size_t kMaxSpeakers = 16;
inline constexpr std::size_t kMaxSources = 128;
inline constexpr std::size_t kMaxRoutesGlobal = 512;
inline constexpr std::size_t kMaxRoutesPerSource = 16;

struct PlanarPosition
{
    float x = 0.0f;
    float y = 0.0f;
};

enum class RouteMode : std::uint8_t
{
    full = 0,
    band,
};

enum class InputChannelMode : std::uint8_t
{
    sumMono = 0,
    left,
    right,
};

} // namespace clubcraft
