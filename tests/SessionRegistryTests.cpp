#include "SessionRegistry.h"
#include "SpatialMath.h"
#include "SpeakerType.h"
#include "SpeakerCharacterResponse.h"

#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <numbers>

void runSceneCompilerTests();
void runSourceRouteRendererTests();

namespace
{
constexpr float kTolerance = 0.0001f;

void assertNear(float actual, float expected)
{
    assert(std::abs(actual - expected) < kTolerance);
}

void testSpatialSceneSnapshotPublication()
{
    auto& registry = clubcraft::SessionRegistry::instance();
    registry.resetForTests();

    const auto handle = registry.acquireSession("test-club");
    assert(handle != nullptr);
    assert(!clubcraft::SessionRegistry::readSnapshot(handle).has_value());

    registry.publishSnapshot({
        .sessionId = "test-club",
        .revision = 7,
        .masterLevelDb = -6.0f,
        .speakerLevelDb = { 0.0f, -6.0f, -12.0f, -60.0f },
        .speakerPositions = {
            clubcraft::PlanarPosition { -4.0f, 7.0f },
            clubcraft::PlanarPosition { 4.0f, 7.0f },
            clubcraft::PlanarPosition { -4.0f, -7.0f },
            clubcraft::PlanarPosition { 4.0f, -7.0f },
        },
        .genericResponseTone = 0.35f,
        .listenerPosition = { 2.5f, -3.0f },
        .speakerTypes = {
            clubcraft::SpeakerType::sub,
            clubcraft::SpeakerType::woofer,
            clubcraft::SpeakerType::mid,
            clubcraft::SpeakerType::high,
        },
    });

    const auto published = clubcraft::SessionRegistry::readSnapshot(handle);
    assert(published.has_value());
    assert(published->revision == 7);
    assertNear(published->masterLinearGain, 0.5011872f);
    assertNear(published->speakerLinearGains[0], 1.0f);
    assertNear(published->speakerLinearGains[1], 0.5011872f);
    assertNear(published->speakerLinearGains[2], 0.2511886f);
    assertNear(published->speakerLinearGains[3], 0.001f);
    assertNear(published->speakerPositions[0].x, -4.0f);
    assertNear(published->speakerPositions[0].y, 7.0f);
    assertNear(published->speakerPositions[3].x, 4.0f);
    assertNear(published->speakerPositions[3].y, -7.0f);
    assertNear(published->genericResponseTone, 0.35f);
    assertNear(published->listenerPosition.x, 2.5f);
    assertNear(published->listenerPosition.y, -3.0f);
    assert(published->speakerTypes[0] == clubcraft::SpeakerType::sub);
    assert(published->speakerTypes[1] == clubcraft::SpeakerType::woofer);
    assert(published->speakerTypes[2] == clubcraft::SpeakerType::mid);
    assert(published->speakerTypes[3] == clubcraft::SpeakerType::high);
}

void testSpeakerTypeMapping()
{
    using namespace clubcraft;

    assert(speakerTypeFromParameterIndex(0) == SpeakerType::sub);
    assert(speakerTypeFromParameterIndex(1) == SpeakerType::woofer);
    assert(speakerTypeFromParameterIndex(2) == SpeakerType::fullRange);
    assert(speakerTypeFromParameterIndex(3) == SpeakerType::mid);
    assert(speakerTypeFromParameterIndex(4) == SpeakerType::high);
    assert(speakerTypeFromParameterIndex(-1) == SpeakerType::fullRange);
    assert(speakerTypeFromParameterIndex(999) == SpeakerType::fullRange);
    assert(speakerTypeToParameterIndex(SpeakerType::fullRange) == 2);
    assert(speakerTypeName(SpeakerType::high) == "HIGH");
}

float rmsForSpeakerType(clubcraft::SpeakerType type, float frequencyHz)
{
    constexpr auto sampleRate = 48000.0;
    constexpr auto blockSize = 256;
    constexpr auto warmupSamples = 4096;
    constexpr auto measuredSamples = 48000;

    clubcraft::SpeakerCharacterResponse response;
    response.prepare(sampleRate, blockSize);
    response.setType(type);

    float sumSquares = 0.0f;
    for (int sample = 0; sample < warmupSamples + measuredSamples; ++sample)
    {
        const auto phase = 2.0f * std::numbers::pi_v<float> * frequencyHz
            * static_cast<float>(sample) / static_cast<float>(sampleRate);
        const auto output = response.processSample(0.25f * std::sin(phase));
        if (sample >= warmupSamples)
            sumSquares += output * output;
    }

    return std::sqrt(sumSquares / static_cast<float>(measuredSamples));
}

void testSpeakerCharacterResponses()
{
    using namespace clubcraft;

    const auto subAt70Hz = rmsForSpeakerType(SpeakerType::sub, 70.0f);
    const auto subAt4kHz = rmsForSpeakerType(SpeakerType::sub, 4000.0f);
    assert(subAt70Hz > subAt4kHz * 10.0f);

    const auto highAt100Hz = rmsForSpeakerType(SpeakerType::high, 100.0f);
    const auto highAt6kHz = rmsForSpeakerType(SpeakerType::high, 6000.0f);
    assert(highAt6kHz > highAt100Hz * 10.0f);

    const auto fullRangeAt100Hz = rmsForSpeakerType(SpeakerType::fullRange, 100.0f);
    const auto fullRangeAt6kHz = rmsForSpeakerType(SpeakerType::fullRange, 6000.0f);
    assertNear(fullRangeAt100Hz, fullRangeAt6kHz);
}

void testFullSignalNormalization()
{
    clubcraft::LegacyRealtimeSceneSnapshot unityScene;
    unityScene.speakerLinearGains = { 1.0f, 1.0f, 1.0f, 1.0f };
    assertNear(unityScene.normalizedFullSignalGain(), 1.0f);

    clubcraft::LegacyRealtimeSceneSnapshot mixedScene;
    mixedScene.speakerLinearGains = { 1.0f, 0.5f, 0.25f, 0.0f };
    assertNear(mixedScene.normalizedFullSignalGain(), 0.4375f);
}

void testSpatialMath()
{
    using namespace clubcraft;

    assertNear(spatial::distance({ 0.0f, 0.0f }, { 3.0f, 4.0f }), 5.0f);

    const auto centre = spatial::constantPowerPan(0.0f);
    assertNear(centre.left, 0.70710678f);
    assertNear(centre.right, 0.70710678f);

    const auto left = spatial::constantPowerPan(-1.0f);
    assertNear(left.left, 1.0f);
    assertNear(left.right, 0.0f);

    const auto right = spatial::constantPowerPan(1.0f);
    assertNear(right.left, 0.0f);
    assertNear(right.right, 1.0f);

    const auto rightOfListener = spatial::panForSpeakerAndListener({ 6.0f, 0.0f }, { 0.0f, 0.0f });
    assertNear(rightOfListener.left, 0.0f);
    assertNear(rightOfListener.right, 1.0f);

    const auto directlyInFront = spatial::panForSpeakerAndListener({ 0.0f, 6.0f }, { 0.0f, 0.0f });
    assertNear(directlyInFront.left, 0.70710678f);
    assertNear(directlyInFront.right, 0.70710678f);

    const auto listenerMovedRightAndForward = spatial::panForSpeakerAndListener({ 6.0f, 0.0f }, { 4.0f, 4.0f });
    assert(listenerMovedRightAndForward.left > rightOfListener.left + 0.1f);
    assert(listenerMovedRightAndForward.right < rightOfListener.right - 0.05f);

    assertNear(spatial::gainForRelativePath(12.0f, 12.0f), 1.0f);
    assertNear(spatial::gainForRelativePath(12.0f, 24.0f), 0.5f);
    assertNear(spatial::relativeDelaySeconds(12.0f, 12.0f), 0.0f);
    assert(spatial::relativeDelaySeconds(24.0f, 12.0f) > 0.0f);
    assert(spatial::relativeDelaySeconds(1000.0f, 0.0f) <= spatial::kMaximumRelativeDelaySeconds);
}

void testDynamicSceneAndRoutePlanPublication()
{
    auto& registry = clubcraft::SessionRegistry::instance();
    registry.resetForTests();

    const auto publisher = registry.registerClubPublisher("dynamic-session", 101);
    assert(publisher.authoritative);
    assert(!publisher.conflict);

    const auto sourceResult = registry.registerSource({
        .sourceId = "kick",
        .sessionId = "dynamic-session",
        .displayName = "Kick",
        .heartbeat = 1,
        .runtimeInstanceToken = 201,
    });
    assert(sourceResult.accepted());

    clubcraft::RealtimeSceneSnapshot scene;
    scene.revision = 55;
    scene.activeSpeakerCount = 1;
    scene.speakers[0] = {
        .active = true,
        .generation = 3,
        .type = clubcraft::SpeakerType::sub,
        .linearGain = 0.5f,
        .position = { -4.0f, 3.0f },
    };

    assert(registry.publishRealtimeScene("dynamic-session", 101, scene));
    const auto sceneHandle = registry.acquireDynamicSession("dynamic-session");
    const auto publishedScene = clubcraft::SessionRegistry::readRealtimeScene(sceneHandle);
    assert(publishedScene.has_value());
    assert(publishedScene->revision == 55);
    assert(publishedScene->speakers[0].generation == 3);
    assertNear(publishedScene->speakers[0].linearGain, 0.5f);

    clubcraft::SourceRoutePlan plan;
    plan.revision = 55;
    plan.routeCount = 1;
    plan.routes[0] = {
        .enabled = true,
        .speakerSlot = 0,
        .speakerGeneration = 3,
        .mode = clubcraft::RouteMode::full,
        .inputMode = clubcraft::InputChannelMode::sumMono,
        .linearGain = 1.0f,
    };

    assert(registry.publishSourceRoutePlan("dynamic-session", 101, "kick", plan));
    const auto routeHandle = registry.acquireSourceRoute("dynamic-session", "kick");
    const auto publishedPlan = clubcraft::SessionRegistry::readSourceRoutePlan(routeHandle);
    assert(publishedPlan.has_value());
    assert(publishedPlan->revision == 55);
    assert(publishedPlan->routeCount == 1);
    assert(publishedPlan->routes[0].speakerGeneration == 3);

    const auto duplicateSource = registry.registerSource({
        .sourceId = "kick",
        .sessionId = "dynamic-session",
        .displayName = "Duplicated Kick",
        .heartbeat = 2,
        .runtimeInstanceToken = 202,
    });
    assert(duplicateSource.requiresRekey());

    const auto otherSessionSource = registry.registerSource({
        .sourceId = "kick",
        .sessionId = "another-session",
        .displayName = "Kick in another Session",
        .heartbeat = 3,
        .runtimeInstanceToken = 203,
    });
    assert(otherSessionSource.accepted());

    const auto conflictingClub = registry.registerClubPublisher("dynamic-session", 102);
    assert(!conflictingClub.authoritative);
    assert(conflictingClub.conflict);
    assert(registry.hasClubConflict("dynamic-session"));
    assert(!registry.publishRealtimeScene("dynamic-session", 102, scene));
}

void testSourceLifecycle()
{
    auto& registry = clubcraft::SessionRegistry::instance();
    registry.resetForTests();

    const auto sourceRegistration = registry.registerSource({
        .sourceId = "kick-source",
        .sessionId = "test-club",
        .displayName = "Kick",
        .position = { 2.0f, -3.0f },
        .heartbeat = 42,
    });
    assert(sourceRegistration.accepted());

    const auto source = registry.getSource("kick-source");
    assert(source.has_value());
    assert(source->displayName == "Kick");
    assertNear(source->position.x, 2.0f);
    assertNear(source->position.y, -3.0f);
    assert(source->heartbeat == 42);

    registry.unregisterSource("kick-source");
    assert(!registry.getSource("kick-source").has_value());
}
}

int main()
{
    testSpatialSceneSnapshotPublication();
    testSpeakerTypeMapping();
    testSpeakerCharacterResponses();
    testFullSignalNormalization();
    testSpatialMath();
    testDynamicSceneAndRoutePlanPublication();
    testSourceLifecycle();
    runSceneCompilerTests();
    runSourceRouteRendererTests();
    std::cout << "ClubCraftCoreTests passed\n";
    return 0;
}
