#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include <juce_audio_basics/juce_audio_basics.h>

#include "GenericSpeakerResponse.h"
#include "RealtimePlan.h"
#include "SpeakerCharacterResponse.h"
#include "SpatialMath.h"

namespace clubcraft
{

/**
    0.6.0 temporary Preview Renderer.

    It consumes one SOURCE's fixed-capacity RoutePlan and only performs the
    linear V1 Speaker Voice operations: FULL + SUM_MONO route gain, speaker
    type response, speaker level, and a listener-relative stereo preview. It
    intentionally does not perform BAND DSP, HRTF, discrete multichannel output,
    saturation, compression, or any cross-SOURCE aggregation.
*/
class SourceRouteStereoRenderer final
{
public:
    void prepare(double sampleRate, int maximumBlockSize)
    {
        for (auto& response : speakerCharacters)
            response.prepare(sampleRate, maximumBlockSize);
        for (auto& response : genericResponses)
            response.prepare(sampleRate, maximumBlockSize, 1);
        reset();
    }

    void reset() noexcept
    {
        for (auto& response : speakerCharacters)
            response.reset();
        for (auto& response : genericResponses)
            response.reset();
        routeSignatures.fill(0);
    }

    void render(juce::AudioBuffer<float>& buffer,
                const RealtimeSceneSnapshot& scene,
                const SourceRoutePlan& plan) noexcept
    {
        if (buffer.getNumChannels() < 2)
            return;

        const auto routeCount = std::min<std::size_t>(plan.routeCount, kMaxRoutesPerSource);
        if (routeCount == 0)
        {
            buffer.clear();
            return;
        }

        std::array<spatial::StereoGains, kMaxRoutesPerSource> pans {};
        std::array<float, kMaxRoutesPerSource> pathGains {};
        std::array<bool, kMaxRoutesPerSource> activeRoutes {};
        bool hasActiveRoute = false;

        for (std::size_t index = 0; index < routeCount; ++index)
        {
            const auto& route = plan.routes[index];
            if (!route.enabled || route.mode != RouteMode::full || route.inputMode != InputChannelMode::sumMono
                || route.speakerSlot >= kMaxSpeakers)
                continue;

            const auto& speaker = scene.speakers[route.speakerSlot];
            if (!speaker.active || speaker.generation != route.speakerGeneration)
                continue;

            const auto signature = (static_cast<std::uint64_t>(route.speakerSlot) << 32U)
                | static_cast<std::uint64_t>(route.speakerGeneration);
            if (routeSignatures[index] != signature)
            {
                speakerCharacters[index].reset();
                genericResponses[index].reset();
                routeSignatures[index] = signature;
            }

            speakerCharacters[index].setType(speaker.type);
            pans[index] = spatial::panForSpeakerAndListener(speaker.position, scene.listener);
            const auto pathDistance = std::max(1.0f, spatial::distance(speaker.position, scene.listener));
            pathGains[index] = spatial::gainForRelativePath(6.0f, pathDistance)
                * route.linearGain * speaker.linearGain;
            activeRoutes[index] = true;
            hasActiveRoute = true;
        }

        if (!hasActiveRoute)
        {
            buffer.clear();
            return;
        }

        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getWritePointer(1);
        const auto sampleCount = buffer.getNumSamples();
        for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
        {
            const auto sourceMono = 0.5f * (left[sampleIndex] + right[sampleIndex]);
            float outputLeft = 0.0f;
            float outputRight = 0.0f;

            for (std::size_t routeIndex = 0; routeIndex < routeCount; ++routeIndex)
            {
                if (!activeRoutes[routeIndex])
                    continue;

                auto voice = sourceMono * pathGains[routeIndex];
                voice = speakerCharacters[routeIndex].processSample(voice);
                voice = genericResponses[routeIndex].processSample(voice, scene.genericResponseTone);
                outputLeft += voice * pans[routeIndex].left;
                outputRight += voice * pans[routeIndex].right;
            }

            left[sampleIndex] = outputLeft * scene.masterLinearGain;
            right[sampleIndex] = outputRight * scene.masterLinearGain;
        }
    }

private:
    std::array<SpeakerCharacterResponse, kMaxRoutesPerSource> speakerCharacters;
    std::array<GenericSpeakerResponse, kMaxRoutesPerSource> genericResponses;
    std::array<std::uint64_t, kMaxRoutesPerSource> routeSignatures {};
};

} // namespace clubcraft
