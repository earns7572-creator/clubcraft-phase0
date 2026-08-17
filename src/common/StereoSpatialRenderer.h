#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include "GenericSpeakerResponse.h"
#include "SpatialMath.h"
#include "SpeakerCharacterResponse.h"

namespace clubcraft
{

/**
    Renders one SOURCE as four virtual speaker paths into a stereo listener bus.

    The renderer owns all scratch memory, delay state and filters. Call prepare()
    outside the audio callback. render() performs no allocation or locking.
*/
class StereoSpatialRenderer final
{
public:
    void prepare(double newSampleRate, int maximumBlockSize)
    {
        sampleRate = newSampleRate;
        maxBlockSize = maximumBlockSize;
        const auto delaySamples = static_cast<std::size_t>(std::ceil(sampleRate * spatial::kMaximumRelativeDelaySeconds))
            + static_cast<std::size_t>(maximumBlockSize) + 2U;
        monoDelayLine.assign(delaySamples, 0.0f);
        writeIndex = 0;

        for (auto& response : speakerResponses)
            response.prepare(sampleRate, maximumBlockSize, 1);
        for (auto& character : speakerCharacters)
            character.prepare(sampleRate, maximumBlockSize);
    }

    void reset()
    {
        std::fill(monoDelayLine.begin(), monoDelayLine.end(), 0.0f);
        writeIndex = 0;
        for (auto& response : speakerResponses)
            response.reset();
        for (auto& character : speakerCharacters)
            character.reset();
    }

    void render(juce::AudioBuffer<float>& buffer,
                const RealtimeSceneSnapshot& scene,
                PlanarPosition sourcePosition) noexcept
    {
        if (buffer.getNumChannels() < 2 || monoDelayLine.empty())
            return;

        std::array<SpeakerPath, kSpeakerCount> paths;
        constexpr PlanarPosition listener { 0.0f, 0.0f };

        float maximumAbsX = 0.25f;
        float referencePath = 0.0f;
        for (std::size_t index = 0; index < kSpeakerCount; ++index)
        {
            maximumAbsX = std::max(maximumAbsX, std::abs(scene.speakerPositions[index].x));
            referencePath += 2.0f * spatial::distance(scene.speakerPositions[index], listener);
        }
        referencePath /= static_cast<float>(kSpeakerCount);

        float shortestPath = std::numeric_limits<float>::max();
        for (std::size_t index = 0; index < kSpeakerCount; ++index)
        {
            auto& path = paths[index];
            const auto sourceToSpeaker = spatial::distance(sourcePosition, scene.speakerPositions[index]);
            const auto speakerToListener = spatial::distance(scene.speakerPositions[index], listener);
            path.totalDistance = sourceToSpeaker + speakerToListener;
            shortestPath = std::min(shortestPath, path.totalDistance);
            path.gain = scene.speakerLinearGains[index]
                * spatial::gainForRelativePath(referencePath, path.totalDistance)
                / static_cast<float>(kSpeakerCount);
            path.responseTone = spatial::toneForRelativePath(
                scene.genericResponseTone, referencePath, path.totalDistance);
            path.pan = spatial::constantPowerPan(scene.speakerPositions[index].x / maximumAbsX);
            path.type = scene.speakerTypes[index];
            speakerCharacters[index].setType(path.type);
        }

        for (auto& path : paths)
        {
            const auto delaySeconds = spatial::relativeDelaySeconds(path.totalDistance, shortestPath);
            path.delaySamples = std::min(
                static_cast<std::size_t>(std::round(delaySeconds * static_cast<float>(sampleRate))),
                monoDelayLine.size() - 1U);
        }

        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getWritePointer(1);
        const auto samples = buffer.getNumSamples();

        for (int sample = 0; sample < samples; ++sample)
        {
            const auto fullSignal = 0.5f * (left[sample] + right[sample]);
            monoDelayLine[writeIndex] = fullSignal;

            float stereoLeft = 0.0f;
            float stereoRight = 0.0f;
            for (std::size_t index = 0; index < kSpeakerCount; ++index)
            {
                const auto& path = paths[index];
                const auto readIndex = (writeIndex + monoDelayLine.size() - path.delaySamples) % monoDelayLine.size();
                const auto characterSample = speakerCharacters[index].processSample(
                    monoDelayLine[readIndex]);
                const auto speakerSample = speakerResponses[index].processSample(
                    characterSample, path.responseTone) * path.gain;
                stereoLeft += speakerSample * path.pan.left;
                stereoRight += speakerSample * path.pan.right;
            }

            left[sample] = stereoLeft;
            right[sample] = stereoRight;
            writeIndex = (writeIndex + 1U) % monoDelayLine.size();
        }
    }

private:
    struct SpeakerPath
    {
        float totalDistance = 0.0f;
        float gain = 0.0f;
        float responseTone = 1.0f;
        spatial::StereoGains pan;
        SpeakerType type = SpeakerType::fullRange;
        std::size_t delaySamples = 0;
    };

    double sampleRate = 48000.0;
    int maxBlockSize = 0;
    std::vector<float> monoDelayLine;
    std::size_t writeIndex = 0;
    std::array<GenericSpeakerResponse, kSpeakerCount> speakerResponses;
    std::array<SpeakerCharacterResponse, kSpeakerCount> speakerCharacters;
};

} // namespace clubcraft
