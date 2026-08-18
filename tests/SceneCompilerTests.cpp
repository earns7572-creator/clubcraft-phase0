#include "SceneCompiler.h"
#include "PendingAutomationMailbox.h"
#include "SceneState.h"
#include "TestCheck.h"

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

    CHECK(!result.hasError());
    CHECK(compiled.realtimeScene.revision == 11);
    CHECK(compiled.realtimeScene.activeSpeakerCount == 2);
    CHECK(compiled.sourcePlans.contains("kick"));

    const auto& plan = compiled.sourcePlans.at("kick");
    CHECK(plan.revision == 11);
    CHECK(plan.routeCount == 2);
    CHECK(plan.routes[0].inputMode == clubcraft::InputChannelMode::sumMono);
    CHECK(plan.routes[0].linearGain == 1.0f);
    CHECK(plan.routes[0].speakerSlot != plan.routes[1].speakerSlot);
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

    CHECK(!result.hasError());
    const auto& plan = compiled.sourcePlans.at("kick");
    CHECK(plan.routeCount == 4);
    for (std::size_t index = 0; index < plan.routeCount; ++index)
        CHECK(plan.routes[index].linearGain == 0.25f);
}

void testGenerationChangesWhenSlotIsReused()
{
    clubcraft::SceneCompiler compiler;
    clubcraft::CompiledScene firstCompiled;
    clubcraft::DynamicScene firstScene;
    firstScene.speakers = { makeSpeaker("first") };
    firstScene.routes = { makeRoute("route-first", "source", "first") };
    CHECK(!compiler.compile(firstScene, { "source" }, 1, firstCompiled).hasError());

    const auto firstSlot = firstCompiled.sourcePlans.at("source").routes[0].speakerSlot;
    const auto firstGeneration = firstCompiled.sourcePlans.at("source").routes[0].speakerGeneration;

    clubcraft::CompiledScene removedCompiled;
    clubcraft::DynamicScene removedScene;
    CHECK(!compiler.compile(removedScene, { "source" }, 2, removedCompiled).hasError());

    clubcraft::CompiledScene secondCompiled;
    clubcraft::DynamicScene secondScene;
    secondScene.speakers = { makeSpeaker("second") };
    secondScene.routes = { makeRoute("route-second", "source", "second") };
    CHECK(!compiler.compile(secondScene, { "source" }, 3, secondCompiled).hasError());

    const auto& secondRoute = secondCompiled.sourcePlans.at("source").routes[0];
    CHECK(secondRoute.speakerSlot == firstSlot);
    CHECK(secondRoute.speakerGeneration > firstGeneration);
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
    CHECK(restored.has_value());
    CHECK(restored->speakers.size() == 1);
    CHECK(restored->routes.size() == 1);
    CHECK(restored->speakers[0].stableId == "speaker-a");
    CHECK(restored->speakers[0].type == clubcraft::SpeakerType::woofer);
    CHECK(restored->routes[0].sourceId == "source-a");
    CHECK(restored->legacyDefaultRouting);
    CHECK(restored->legacyRouteGain == 0.25f);
}

void testPendingAutomationAckAfterCommit()
{
    using Field = clubcraft::LegacyAutomationField;
    clubcraft::PendingAutomationMailbox mailbox;

    mailbox.store(Field::speakerLevel1, -6.0f);
    const auto timerSnapshot = mailbox.snapshot();
    CHECK(timerSnapshot[clubcraft::toAutomationIndex(Field::speakerLevel1)].isPending());
    CHECK(timerSnapshot[clubcraft::toAutomationIndex(Field::speakerLevel1)].value == -6.0f);

    // A state-save interleave sees the value even though Timer has already
    // snapshotted it. The value is not consumed until Scene commit succeeds.
    const auto stateSaveSnapshot = mailbox.snapshot();
    CHECK(stateSaveSnapshot[clubcraft::toAutomationIndex(Field::speakerLevel1)].value == -6.0f);
    mailbox.acknowledge(timerSnapshot);
    CHECK(!mailbox.snapshot()[clubcraft::toAutomationIndex(Field::speakerLevel1)].isPending());

    mailbox.store(Field::speakerLevel1, -3.0f);
    const auto staleTimerSnapshot = mailbox.snapshot();
    mailbox.store(Field::speakerLevel1, -1.0f);
    mailbox.acknowledge(staleTimerSnapshot);
    const auto afterStaleAck = mailbox.snapshot();
    CHECK(afterStaleAck[clubcraft::toAutomationIndex(Field::speakerLevel1)].isPending());
    CHECK(afterStaleAck[clubcraft::toAutomationIndex(Field::speakerLevel1)].value == -1.0f);
}

void testCapacityAndReferenceErrors()
{
    clubcraft::SceneCompiler compiler;
    clubcraft::CompiledScene compiled;

    clubcraft::DynamicScene tooManySpeakers;
    for (std::size_t index = 0; index < clubcraft::kMaxSpeakers + 1; ++index)
        tooManySpeakers.speakers.push_back(makeSpeaker("speaker-" + std::to_string(index)));
    auto result = compiler.compile(tooManySpeakers, { "source" }, 1, compiled);
    CHECK(result.code == clubcraft::SceneCompileErrorCode::tooManySpeakers);

    clubcraft::DynamicScene tooManyGlobalRoutes;
    tooManyGlobalRoutes.speakers = { makeSpeaker("speaker") };
    for (std::size_t index = 0; index < clubcraft::kMaxRoutesGlobal + 1; ++index)
        tooManyGlobalRoutes.routes.push_back(makeRoute(
            "global-route-" + std::to_string(index), "source", "speaker"));
    result = compiler.compile(tooManyGlobalRoutes, { "source" }, 2, compiled);
    CHECK(result.code == clubcraft::SceneCompileErrorCode::tooManyRoutes);

    clubcraft::DynamicScene missingSpeaker;
    missingSpeaker.speakers = { makeSpeaker("known") };
    missingSpeaker.routes = { makeRoute("missing", "source", "unknown") };
    result = compiler.compile(missingSpeaker, { "source" }, 3, compiled);
    CHECK(result.code == clubcraft::SceneCompileErrorCode::speakerNotFound);

    clubcraft::DynamicScene tooManyRoutesForSource;
    tooManyRoutesForSource.speakers = { makeSpeaker("speaker") };
    for (std::size_t index = 0; index < clubcraft::kMaxRoutesPerSource + 1; ++index)
        tooManyRoutesForSource.routes.push_back(makeRoute(
            "route-" + std::to_string(index), "source", "speaker"));
    result = compiler.compile(tooManyRoutesForSource, { "source" }, 4, compiled);
    CHECK(result.code == clubcraft::SceneCompileErrorCode::tooManyRoutesForSource);
}

} // namespace

void runSceneCompilerTests()
{
    testCompileOneToManyRoutes();
    testLegacyRoutesUseQuarterGain();
    testGenerationChangesWhenSlotIsReused();
    testSceneStateRoundTrip();
    testPendingAutomationAckAfterCommit();
    testCapacityAndReferenceErrors();
}
