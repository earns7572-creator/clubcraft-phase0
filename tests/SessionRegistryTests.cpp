#include "SessionRegistry.h"
#include "SpatialMath.h"

#include <array>
#include <cassert>
#include <cmath>
#include <iostream>

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
}

void testFullSignalNormalization()
{
    clubcraft::RealtimeSceneSnapshot unityScene;
    unityScene.speakerLinearGains = { 1.0f, 1.0f, 1.0f, 1.0f };
    assertNear(unityScene.normalizedFullSignalGain(), 1.0f);

    clubcraft::RealtimeSceneSnapshot mixedScene;
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

    assertNear(spatial::gainForRelativePath(12.0f, 12.0f), 1.0f);
    assertNear(spatial::gainForRelativePath(12.0f, 24.0f), 0.5f);
    assertNear(spatial::relativeDelaySeconds(12.0f, 12.0f), 0.0f);
    assert(spatial::relativeDelaySeconds(24.0f, 12.0f) > 0.0f);
    assert(spatial::relativeDelaySeconds(1000.0f, 0.0f) <= spatial::kMaximumRelativeDelaySeconds);
}

void testSourceLifecycle()
{
    auto& registry = clubcraft::SessionRegistry::instance();
    registry.resetForTests();

    registry.registerSource({
        .sourceId = "kick-source",
        .sessionId = "test-club",
        .displayName = "Kick",
        .position = { 2.0f, -3.0f },
        .heartbeat = 42,
    });

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
    testFullSignalNormalization();
    testSpatialMath();
    testSourceLifecycle();
    std::cout << "ClubCraftCoreTests passed\n";
    return 0;
}
