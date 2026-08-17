#include "SceneCompiler.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace clubcraft
{

namespace
{
constexpr std::size_t kLegacySpeakerRouteCount = 4;

[[nodiscard]] bool contains(const std::vector<std::string>& values, const std::string& value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}
}

SceneCompileError SceneCompiler::compile(const DynamicScene& scene,
                                         const std::vector<std::string>& activeSourceIds,
                                         std::uint64_t revision,
                                         CompiledScene& destination)
{
    if (scene.speakers.size() > kMaxSpeakers)
        return error(SceneCompileErrorCode::tooManySpeakers, "The scene exceeds MAX_SPEAKERS.");

    if (activeSourceIds.size() > kMaxSources)
        return error(SceneCompileErrorCode::tooManySources, "The session exceeds MAX_SOURCES.");

    if (scene.routes.size() > kMaxRoutesGlobal)
        return error(SceneCompileErrorCode::tooManyRoutes, "The scene exceeds MAX_ROUTES_GLOBAL.");

    std::unordered_set<std::string> speakerIds;
    for (const auto& speaker : scene.speakers)
    {
        if (!speakerIds.insert(speaker.stableId).second)
            return error(SceneCompileErrorCode::duplicateSpeakerId, "Speaker Stable IDs must be unique.");
    }

    std::unordered_set<std::string> routeIds;
    for (const auto& route : scene.routes)
    {
        if (!routeIds.insert(route.stableId).second)
            return error(SceneCompileErrorCode::duplicateRouteId, "Route Stable IDs must be unique.");
    }

    // Remove only stale control-side identifiers. Generation is retained so a
    // future speaker which reuses the slot receives a different generation.
    for (auto& assignment : assignments)
    {
        if (!assignment.stableId.empty() && !speakerIds.contains(assignment.stableId))
            assignment.stableId.clear();
    }

    std::unordered_map<std::string, std::size_t> slotForSpeaker;
    for (std::size_t speakerIndex = 0; speakerIndex < scene.speakers.size(); ++speakerIndex)
    {
        const auto& speaker = scene.speakers[speakerIndex];
        auto slotIt = std::find_if(assignments.begin(), assignments.end(), [&speaker] (const SlotAssignment& assignment)
        {
            return assignment.stableId == speaker.stableId;
        });

        if (slotIt == assignments.end())
        {
            slotIt = std::find_if(assignments.begin(), assignments.end(), [] (const SlotAssignment& assignment)
            {
                return assignment.stableId.empty();
            });

            // The capacity was verified from scene.speakers above. This is a
            // defensive guard if compiler state is ever corrupted.
            if (slotIt == assignments.end())
                return error(SceneCompileErrorCode::tooManySpeakers, "No free realtime speaker slot is available.");

            slotIt->stableId = speaker.stableId;
            slotIt->generation = std::max<std::uint32_t>(1, slotIt->generation + 1);
        }

        const auto slot = static_cast<std::size_t>(std::distance(assignments.begin(), slotIt));
        slotForSpeaker.emplace(speaker.stableId, slot);
    }

    CompiledScene compiled;
    compiled.realtimeScene.revision = revision;
    compiled.realtimeScene.listener = scene.listener;
    compiled.realtimeScene.masterLinearGain = dbToLinear(scene.masterLevelDb);
    compiled.realtimeScene.genericResponseTone = scene.genericResponseTone;

    for (const auto& speaker : scene.speakers)
    {
        const auto slot = slotForSpeaker.at(speaker.stableId);
        auto& realtimeSpeaker = compiled.realtimeScene.speakers[slot];
        realtimeSpeaker.active = speaker.enabled;
        realtimeSpeaker.generation = assignments[slot].generation;
        realtimeSpeaker.type = speaker.type;
        realtimeSpeaker.linearGain = dbToLinear(speaker.levelDb);
        realtimeSpeaker.position = speaker.position;
        if (speaker.enabled)
            ++compiled.realtimeScene.activeSpeakerCount;
    }

    std::vector<std::string> uniqueSources;
    uniqueSources.reserve(activeSourceIds.size());
    for (const auto& sourceId : activeSourceIds)
    {
        if (!contains(uniqueSources, sourceId))
            uniqueSources.push_back(sourceId);
    }

    for (const auto& sourceId : uniqueSources)
    {
        SourceRoutePlan plan;
        plan.revision = revision;
        compiled.sourcePlans.emplace(sourceId, plan);
    }

    const auto appendRoute = [&compiled] (const std::string& sourceId, const CompiledRoute& route) -> SceneCompileError
    {
        const auto planIt = compiled.sourcePlans.find(sourceId);
        if (planIt == compiled.sourcePlans.end())
            return {};

        auto& plan = planIt->second;
        if (plan.routeCount >= kMaxRoutesPerSource)
            return SceneCompiler::error(SceneCompileErrorCode::tooManyRoutesForSource,
                                        "A SOURCE exceeds MAX_ROUTES_PER_SOURCE.");

        plan.routes[plan.routeCount++] = route;
        return {};
    };

    if (scene.legacyDefaultRouting)
    {
        const auto legacySpeakerCount = std::min(kLegacySpeakerRouteCount, scene.speakers.size());
        for (const auto& sourceId : uniqueSources)
        {
            for (std::size_t index = 0; index < legacySpeakerCount; ++index)
            {
                const auto& speaker = scene.speakers[index];
                const auto slot = slotForSpeaker.at(speaker.stableId);
                const auto result = appendRoute(sourceId, {
                    .enabled = true,
                    .speakerSlot = static_cast<std::uint16_t>(slot),
                    .speakerGeneration = assignments[slot].generation,
                    .mode = RouteMode::full,
                    .inputMode = InputChannelMode::sumMono,
                    .linearGain = scene.legacyRouteGain,
                });
                if (result.hasError())
                    return result;
            }
        }
    }
    else
    {
        for (const auto& route : scene.routes)
        {
            const auto slotIt = slotForSpeaker.find(route.speakerStableId);
            if (slotIt == slotForSpeaker.end())
                return error(SceneCompileErrorCode::speakerNotFound, "A Route references an unknown Speaker Stable ID.");

            const auto slot = slotIt->second;
            const auto result = appendRoute(route.sourceId, {
                .enabled = route.enabled,
                .speakerSlot = static_cast<std::uint16_t>(slot),
                .speakerGeneration = assignments[slot].generation,
                .mode = route.mode,
                .inputMode = route.inputMode,
                .linearGain = route.gainLinear,
                .bandLowHz = route.bandLowHz,
                .bandHighHz = route.bandHighHz,
            });
            if (result.hasError())
                return result;
        }
    }

    destination = std::move(compiled);
    return {};
}

float SceneCompiler::dbToLinear(float db) noexcept
{
    return std::pow(10.0f, db / 20.0f);
}

SceneCompileError SceneCompiler::error(SceneCompileErrorCode code, const char* message)
{
    return { code, message };
}

} // namespace clubcraft
