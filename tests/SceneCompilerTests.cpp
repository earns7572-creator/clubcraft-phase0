#include "SceneCompiler.h"
#include "SceneState.h"

#include <cassert>
#include <string>
#include <vector>

namespace
{

clubcraft::SpeakerConfig makeSpeaker(const std::string& id, float levelDb = 0.0f)
{
    return {
        .stableId = id,
        .name = id,
        .type = clubcraft::SpeakerType::fullRange,
        .position = { 0.0f, 0.0f },
        .levelDb = levelDb,
        .enabled = true,
    };
}

clubcraft::RouteConfig makeRoute(const std::string& id,
                                 const std::string& sourceId,
                                 const std::string& speakerId)
{
    return {
        .stableId = id,
        .sourceId = sourceId,
        .speakerStableId = speakerId,
        .mode = clubcraft::RouteMode::full,
        .inputMode = clubcraft::InputChannelMode::sumMono,
        .gainLinear = 1.0f,
        .enabled = true,
    };
}

void testCompileOneToManyRoutes()
{
    clubcraft::DynamicScene scene;
    scene.speakers = { makeSpeaker("sub"), makeSpeaker("full") };
    scene.routes = {
        makeRoute("kick-sub", "kick", "sub"),
        makeRoute("kick-full", "kick", "full"),
    };

    clubcraft::SceneCompiler compiler;
    clubcraft::CompiledScene compiled;
    const auto result = compiler.compile(scene, { "kick" }, 11, compiled);

    assert(!result.hasError());
    assert(compiled.realtimeScene.revision == 11);
    assert(compiled.realtimeScene.activeSpeakerCount == 2);
    assert(compiled.sourcePlans.contains("kick"));

    const auto& plan = compiled.sourcePlans.at("kick");
    assert(plan.revision == 11);
    assert(plan.routeCount == 2);
    assert(plan.routes[0].inputMode == clubcraft::InputChannelMode::sumMono);
    assert(plan.routes[0].linearGain == 1.0f);
    assert(plan.routes[0].speakerSlot != plan.routes[1].speakerSlot);
}

void testLegacyRoutesUseQuarterGain()
{
    clubcraft::DynamicScene scene;
    scene.legacyDefaultRouting = true;
    scene.legacyRouteGain = 0.25f;
    scene.speakers = {
        makeSpeaker("legacy-1"),
        makeSpeaker("legacy-2"),
        makeSpeaker("legacy-3"),
        makeSpeaker("legacy-4"),
    };

    clubcraft::SceneCompiler compiler;
    clubcraft::CompiledScene compiled;
    const auto result = compiler.compile(scene, { "kick" }, 20, compiled);

    assert(!result.hasError());
    const auto& plan = compiled.sourcePlans.at("kick");
    assert(plan.routeCount == 4);
    for (std::size_t index = 0; index < plan.routeCount; ++index)
        assert(plan.routes[index].linearGain == 0.25f);
}

void testGenerationChangesWhenSlotIsReused()
{
    clubcraft::SceneCompiler compiler;
    clubcraft::CompiledScene firstCompiled;
    clubcraft::DynamicScene firstScene;
    firstScene.speakers = { makeSpeaker("first") };
    firstScene.routes = { makeRoute("route-first", "source", "first") };
    assert(!compiler.compile(firstScene, { "source" }, 1, firstCompiled).hasError());

    const auto firstSlot = firstCompiled.sourcePlans.at("source").routes[0].speakerSlot;
    const auto firstGeneration = firstCompiled.sourcePlans.at("source").routes[0].speakerGeneration;

    clubcraft::CompiledScene removedCompiled;
    clubcraft::DynamicScene removedScene;
    assert(!compiler.compile(removedScene, { "source" }, 2, removedCompiled).hasError());

    clubcraft::CompiledScene secondCompiled;
    clubcraft::DynamicScene secondScene;
    secondScene.speakers = { makeSpeaker("second") };
    secondScene.routes = { makeRoute("route-second", "source", "second") };
    assert(!compiler.compile(secondScene, { "source" }, 3, secondCompiled).hasError());

    const auto& secondRoute = secondCompiled.sourcePlans.at("source").routes[0];
    assert(secondRoute.speakerSlot == firstSlot);
    assert(secondRoute.speakerGeneration > firstGeneration);
}

void testSceneStateRoundTrip()
{
    clubcraft::DynamicScene original;
    original.listener = { 3.0f, -2.0f };
    original.masterLevelDb = -4.0f;
    original.genericResponseTone = 0.6f;
    original.legacyDefaultRouting = true;
    original.legacyRouteGain = 0.25f;
    original.speakers = { makeSpeaker("speaker-a", -3.0f) };
    original.speakers[0].type = clubcraft::SpeakerType::woofer;
    original.routes = { makeRoute("route-a", "source-a", "speaker-a") };

    const auto tree = clubcraft::scene_state::toValueTree(original);
    const auto restored = clubcraft::scene_state::fromValueTree(tree);
    assert(restored.has_value());
    assert(restored->speakers.size() == 1);
    assert(restored->routes.size() == 1);
    assert(restored->speakers[0].stableId == "speaker-a");
    assert(restored->speakers[0].type == clubcraft::SpeakerType::woofer);
    assert(restored->routes[0].sourceId == "source-a");
    assert(restored->legacyDefaultRouting);
    assert(restored->legacyRouteGain == 0.25f);
}

void testCapacityAndReferenceErrors()
{
    clubcraft::SceneCompiler compiler;
    clubcraft::CompiledScene compiled;

    clubcraft::DynamicScene tooManySpeakers;
    for (std::size_t index = 0; index < clubcraft::kMaxSpeakers + 1; ++index)
        tooManySpeakers.speakers.push_back(makeSpeaker("speaker-" + std::to_string(index)));
    auto result = compiler.compile(tooManySpeakers, { "source" }, 1, compiled);
    assert(result.code == clubcraft::SceneCompileErrorCode::tooManySpeakers);

    clubcraft::DynamicScene tooManyGlobalRoutes;
    tooManyGlobalRoutes.speakers = { makeSpeaker("speaker") };
    for (std::size_t index = 0; index < clubcraft::kMaxRoutesGlobal + 1; ++index)
        tooManyGlobalRoutes.routes.push_back(makeRoute(
            "global-route-" + std::to_string(index), "source", "speaker"));
    result = compiler.compile(tooManyGlobalRoutes, { "source" }, 2, compiled);
    assert(result.code == clubcraft::SceneCompileErrorCode::tooManyRoutes);

    clubcraft::DynamicScene missingSpeaker;
    missingSpeaker.speakers = { makeSpeaker("known") };
    missingSpeaker.routes = { makeRoute("missing", "source", "unknown") };
    result = compiler.compile(missingSpeaker, { "source" }, 3, compiled);
    assert(result.code == clubcraft::SceneCompileErrorCode::speakerNotFound);

    clubcraft::DynamicScene tooManyRoutesForSource;
    tooManyRoutesForSource.speakers = { makeSpeaker("speaker") };
    for (std::size_t index = 0; index < clubcraft::kMaxRoutesPerSource + 1; ++index)
        tooManyRoutesForSource.routes.push_back(makeRoute(
            "route-" + std::to_string(index), "source", "speaker"));
    result = compiler.compile(tooManyRoutesForSource, { "source" }, 4, compiled);
    assert(result.code == clubcraft::SceneCompileErrorCode::tooManyRoutesForSource);
}

} // namespace

void runSceneCompilerTests()
{
    testCompileOneToManyRoutes();
    testLegacyRoutesUseQuarterGain();
    testGenerationChangesWhenSlotIsReused();
    testSceneStateRoundTrip();
    testCapacityAndReferenceErrors();
}
