#pragma once

#include <array>
#include <cstdint>

#include "SceneTypes.h"
#include "SpeakerType.h"

namespace clubcraft
{

/** Value-only Speaker state safe to copy on the realtime path. */
struct RealtimeSpeaker
{
    bool active = false;
    std::uint32_t generation = 0;
    SpeakerType type = SpeakerType::fullRange;
    float linearGain = 1.0f;
    PlanarPosition position;
};

/**
    Numeric, source-local route. Stable IDs and control-side strings have been
    compiled away before this type reaches SessionRegistry's realtime path.
*/
struct CompiledRoute
{
    bool enabled = false;
    std::uint16_t speakerSlot = 0;
    std::uint32_t speakerGeneration = 0;
    RouteMode mode = RouteMode::full;
    InputChannelMode inputMode = InputChannelMode::sumMono;
    float linearGain = 1.0f;
    float bandLowHz = 20.0f;
    float bandHighHz = 20000.0f;
};

struct RealtimeSceneSnapshot
{
    std::uint64_t revision = 0;
    std::array<RealtimeSpeaker, kMaxSpeakers> speakers {};
    std::uint16_t activeSpeakerCount = 0;
    PlanarPosition listener;
    float masterLinearGain = 1.0f;
    float genericResponseTone = 1.0f;
};

struct SourceRoutePlan
{
    std::uint64_t revision = 0;
    std::uint16_t routeCount = 0;
    std::array<CompiledRoute, kMaxRoutesPerSource> routes {};
};

} // namespace clubcraft
