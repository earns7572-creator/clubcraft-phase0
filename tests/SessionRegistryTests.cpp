#include "SessionRegistry.h"

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

void testSnapshotPublication()
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
        .primarySpeakerLevelDb = -3.0f,
    });

    const auto published = clubcraft::SessionRegistry::readSnapshot(handle);
    assert(published.has_value());
    assert(published->revision == 7);
    assertNear(published->masterLinearGain, 0.5011872f);
    assertNear(published->primarySpeakerLinearGain, 0.7079458f);
}

void testSourceLifecycle()
{
    auto& registry = clubcraft::SessionRegistry::instance();
    registry.resetForTests();

    registry.registerSource({
        .sourceId = "kick-source",
        .sessionId = "test-club",
        .displayName = "Kick",
        .heartbeat = 42,
    });

    const auto source = registry.getSource("kick-source");
    assert(source.has_value());
    assert(source->displayName == "Kick");
    assert(source->heartbeat == 42);

    registry.unregisterSource("kick-source");
    assert(!registry.getSource("kick-source").has_value());
}
}

int main()
{
    testSnapshotPublication();
    testSourceLifecycle();
    std::cout << "ClubCraftCoreTests passed\n";
    return 0;
}
