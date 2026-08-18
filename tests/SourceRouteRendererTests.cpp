#include "SourceRouteStereoRenderer.h"
#include "TestCheck.h"

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
            .routeSlot = static_cast<std::uint16_t>(index),
            .routeGeneration = 1,
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

void fillStereo(juce::AudioBuffer<float>& buffer)
{
    for (int index = 0; index < buffer.getNumSamples(); ++index)
    {
        buffer.setSample(0, index, 0.05f);
        buffer.setSample(1, index, 0.05f);
    }
}

clubcraft::SourceRoutePlan makePlan(std::uint32_t generation)
{
    clubcraft::SourceRoutePlan plan;
    plan.revision = generation;
    plan.routeCount = 1;
    plan.routes[0] = {
        .enabled = true,
        .routeSlot = 7,
        .routeGeneration = generation,
        .speakerSlot = 0,
        .speakerGeneration = 1,
        .mode = clubcraft::RouteMode::full,
        .inputMode = clubcraft::InputChannelMode::sumMono,
        .linearGain = 1.0f,
    };
    return plan;
}

clubcraft::SourceRoutePlan makeSixteenRoutePlan(std::uint32_t generation)
{
    clubcraft::SourceRoutePlan plan;
    plan.revision = generation;
    plan.routeCount = static_cast<std::uint16_t>(clubcraft::kMaxRoutesPerSource);
    for (std::size_t index = 0; index < clubcraft::kMaxRoutesPerSource; ++index)
    {
        plan.routes[index] = {
            .enabled = true,
            .routeSlot = static_cast<std::uint16_t>(index),
            .routeGeneration = generation,
            .speakerSlot = static_cast<std::uint16_t>(index),
            .speakerGeneration = 1,
            .mode = clubcraft::RouteMode::full,
            .inputMode = clubcraft::InputChannelMode::sumMono,
            .linearGain = 1.0f,
        };
    }
    return plan;
}

clubcraft::RealtimeSceneSnapshot makeSixteenSpeakerScene()
{
    clubcraft::RealtimeSceneSnapshot scene;
    scene.activeSpeakerCount = static_cast<std::uint16_t>(clubcraft::kMaxSpeakers);
    scene.genericResponseTone = 1.0f;
    for (std::size_t index = 0; index < clubcraft::kMaxSpeakers; ++index)
    {
        scene.speakers[index] = {
            .active = true,
            .generation = 1,
            .type = clubcraft::SpeakerType::fullRange,
            .linearGain = 1.0f,
            .position = { 0.0f, 6.0f },
        };
    }
    return scene;
}

void testMaximumActiveAndRetiringVoicePool()
{
    clubcraft::SourceRouteStereoRenderer renderer;
    renderer.prepare(48000.0, 480);
    const auto scene = makeSixteenSpeakerScene();
    const auto firstPlan = makeSixteenRoutePlan(1);
    const auto secondPlan = makeSixteenRoutePlan(2);
    clubcraft::SourceRoutePlan emptyPlan;

    juce::AudioBuffer<float> fullBlock(2, 480);
    fillStereo(fullBlock);
    renderer.render(fullBlock, scene, firstPlan);
    fillStereo(fullBlock);
    renderer.render(fullBlock, scene, firstPlan);
    CHECK(renderer.activeVoiceCountForTests() == clubcraft::kMaxRoutesPerSource);

    juce::AudioBuffer<float> oneSample(2, 1);
    fillStereo(oneSample);
    renderer.render(oneSample, scene, emptyPlan);
    CHECK(renderer.activeVoiceCountForTests() == 0);
    CHECK(renderer.retiringVoiceCountForTests() == clubcraft::kMaxRoutesPerSource);

    fillStereo(oneSample);
    renderer.render(oneSample, scene, secondPlan);
    CHECK(renderer.activeVoiceCountForTests() == clubcraft::kMaxRoutesPerSource);
    CHECK(renderer.retiringVoiceCountForTests() == clubcraft::kMaxRoutesPerSource);
    CHECK(renderer.droppedRetiringVoiceCountForTests() == 0);

    fillStereo(fullBlock);
    renderer.render(fullBlock, scene, secondPlan);
    CHECK(renderer.retiringVoiceCountForTests() == 0);
}

void testRouteDeleteRetiresBeforeSameSlotGenerationReuse()
{
    clubcraft::SourceRouteStereoRenderer renderer;
    renderer.prepare(48000.0, 480);

    clubcraft::RealtimeSceneSnapshot scene;
    scene.activeSpeakerCount = 1;
    scene.genericResponseTone = 1.0f;
    scene.speakers[0] = {
        .active = true,
        .generation = 1,
        .type = clubcraft::SpeakerType::fullRange,
        .linearGain = 1.0f,
        .position = { 0.0f, 6.0f },
    };

    auto firstPlan = makePlan(1);
    juce::AudioBuffer<float> fullBlock(2, 480);
    fillStereo(fullBlock);
    renderer.render(fullBlock, scene, firstPlan);
    fillStereo(fullBlock);
    renderer.render(fullBlock, scene, firstPlan);
    CHECK(renderer.activeVoiceCountForTests() == 1);

    clubcraft::SourceRoutePlan emptyPlan;
    juce::AudioBuffer<float> halfFade(2, 240);
    fillStereo(halfFade);
    renderer.render(halfFade, scene, emptyPlan);
    CHECK(renderer.activeVoiceCountForTests() == 0);
    CHECK(renderer.retiringVoiceCountForTests() == 1);

    auto replacementPlan = makePlan(2);
    juce::AudioBuffer<float> oneSample(2, 1);
    fillStereo(oneSample);
    renderer.render(oneSample, scene, replacementPlan);
    CHECK(renderer.activeVoiceCountForTests() == 1);
    CHECK(renderer.retiringVoiceCountForTests() == 1);

    fillStereo(fullBlock);
    renderer.render(fullBlock, scene, replacementPlan);
    CHECK(renderer.activeVoiceCountForTests() == 1);
    CHECK(renderer.retiringVoiceCountForTests() == 0);
}

} // namespace

void runSourceRouteRendererTests()
{
    const auto oneRoutePeak = renderPeak(1);
    const auto twoRoutePeak = renderPeak(2);
    CHECK(oneRoutePeak > 0.0f);
    // New Dynamic Scene routes deliberately do not divide output by speaker count.
    CHECK(twoRoutePeak > oneRoutePeak * 1.8f);
    testRouteDeleteRetiresBeforeSameSlotGenerationReuse();
    testMaximumActiveAndRetiringVoicePool();
}
