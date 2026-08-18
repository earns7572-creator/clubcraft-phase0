#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "RealtimePlan.h"
#include "SceneModel.h"

namespace clubcraft
{

enum class SceneCompileErrorCode
{
    none,
    tooManySpeakers,
    tooManySources,
    tooManyRoutes,
    tooManyRoutesForSource,
    duplicateSpeakerId,
    duplicateRouteId,
    speakerNotFound,
};

struct SceneCompileError
{
    SceneCompileErrorCode code = SceneCompileErrorCode::none;
    std::string message;

    [[nodiscard]] bool hasError() const noexcept { return code != SceneCompileErrorCode::none; }
};

/** Control-side result. The unordered_map never enters audio processing. */
struct CompiledScene
{
    RealtimeSceneSnapshot realtimeScene;
    std::unordered_map<std::string, SourceRoutePlan> sourcePlans;
};

/**
    Compiles persistable DynamicScene data into fixed-capacity realtime data.
    Speaker assignments persist inside this object so a Stable Speaker ID keeps
    its slot/generation across recompiles and a re-used slot gets a new generation.
*/
class SceneCompiler final
{
public:
    [[nodiscard]] SceneCompileError compile(const DynamicScene& scene,
                                            const std::vector<std::string>& activeSourceIds,
                                            std::uint64_t revision,
                                            CompiledScene& destination);

private:
    struct SlotAssignment
    {
        std::string stableId;
        std::uint32_t generation = 0;
    };

    [[nodiscard]] static float dbToLinear(float db) noexcept;
    [[nodiscard]] static SceneCompileError error(SceneCompileErrorCode code, const char* message);

    std::array<SlotAssignment, kMaxSpeakers> assignments {};
    std::array<SlotAssignment, kMaxRoutesGlobal> routeAssignments {};
};

} // namespace clubcraft
