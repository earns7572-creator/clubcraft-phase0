#include "SourceRouteStereoRenderer.h"

#include <cassert>
#include <cmath>

namespace
{

[[nodiscard]] float renderPeak(std::uint16_t routeCount)
{
    clubcraft::SourceRouteStereoRenderer renderer;
    renderer.prepare(48000.0, 32);

    clubcraft::RealtimeSceneSnapshot scene;
    scene.revision = 9;
    scene.activeSpeakerCount = routeCount;
    scene.genericResponseTone = 1.0f;
    scene.listener = { 0.0f, 0.0f };
    for (std::size_t index = 0; index < routeCount; ++index)
    {
        scene.speakers[index] = {
            .active = true,
            .generation = 1,
            .type = clubcraft::SpeakerType::fullRange,
            .linearGain = 1.0f,
            .position = { 0.0f, 6.0f },
        };
    }

    clubcraft::SourceRoutePlan plan;
    plan.revision = 9;
    plan.routeCount = routeCount;
    for (std::size_t index = 0; index < routeCount; ++index)
    {
        plan.routes[index] = {
            .enabled = true,
            .speakerSlot = static_cast<std::uint16_t>(index),
            .speakerGeneration = 1,
            .mode = clubcraft::RouteMode::full,
            .inputMode = clubcraft::InputChannelMode::sumMono,
            .linearGain = 1.0f,
        };
    }

    juce::AudioBuffer<float> buffer(2, 32);
    buffer.clear();
    for (int index = 0; index < buffer.getNumSamples(); ++index)
    {
        buffer.setSample(0, index, 0.05f);
        buffer.setSample(1, index, 0.05f);
    }

    renderer.render(buffer, scene, plan);
    return std::abs(buffer.getSample(0, buffer.getNumSamples() - 1));
}

} // namespace

void runSourceRouteRendererTests()
{
    const auto oneRoutePeak = renderPeak(1);
    const auto twoRoutePeak = renderPeak(2);
    assert(oneRoutePeak > 0.0f);
    // New Dynamic Scene routes deliberately do not divide output by speaker count.
    assert(twoRoutePeak > oneRoutePeak * 1.8f);
}
