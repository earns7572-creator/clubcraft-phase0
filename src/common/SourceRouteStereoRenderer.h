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
    SOURCE-local realtime renderer for the 0.7 RoutePlan.

    DSP state is keyed by persistent routeSlot + routeGeneration, never by the
    current RoutePlan array index. A fixed pool of 16 active + 16 retiring
    Voices is prepared before playback, so plan changes and Route delete fades
    never allocate or wait in processBlock().
*/
class SourceRouteStereoRenderer final
{
public:
    static constexpr std::size_t kMaxActiveVoices = kMaxRoutesPerSource;
    static constexpr std::size_t kMaxRetiringVoices = kMaxRoutesPerSource;
    static constexpr std::size_t kVoicePoolSize = kMaxActiveVoices + kMaxRetiringVoices;

    void prepare(double newSampleRate, int maximumBlockSize)
    {
        sampleRate = std::max(1.0, newSampleRate);
        for (auto& voice : voices)
            voice.prepare(sampleRate, maximumBlockSize);
        reset();
    }

    void reset() noexcept
    {
        for (auto& voice : voices)
            voice.release();
        nextRetirementOrdinal = 1;
        droppedRetiringVoiceCount = 0;
    }

    [[nodiscard]] std::size_t activeVoiceCountForTests() const noexcept
    {
        return countVoices(VoiceState::active);
    }

    [[nodiscard]] std::size_t retiringVoiceCountForTests() const noexcept
    {
        return countVoices(VoiceState::retiring);
    }

    [[nodiscard]] std::uint32_t droppedRetiringVoiceCountForTests() const noexcept
    {
        return droppedRetiringVoiceCount;
    }

    void render(juce::AudioBuffer<float>& buffer,
                const RealtimeSceneSnapshot& scene,
                const SourceRoutePlan& plan) noexcept
    {
        if (buffer.getNumChannels() < 2)
            return;

        const auto routeCount = std::min<std::size_t>(plan.routeCount, kMaxRoutesPerSource);

        // A Route that has disappeared from the new Plan becomes a retiring
        // Voice. It continues its existing filter/smoother state at target 0
        // for 10ms rather than being cut or reset at the plan boundary.
        for (auto& voice : voices)
        {
            if (voice.state == VoiceState::active && !planContainsIdentity(plan, routeCount, voice))
                retire(voice);
        }

        for (std::size_t index = 0; index < routeCount; ++index)
        {
            const auto& route = plan.routes[index];
            if (route.routeSlot >= kMaxRoutesGlobal || route.routeGeneration == 0)
                continue;

            Voice* voice = findVoice(VoiceState::active, route.routeSlot, route.routeGeneration);
            const auto routeCanRender = isRenderable(route, scene);
            if (voice == nullptr && !routeCanRender)
                continue;

            if (voice == nullptr)
            {
                voice = acquireFreeVoice();
                if (voice == nullptr)
                    continue; // Defensive: a 16-route Plan cannot exhaust 32 prepared Voices.
                voice->activate(route.routeSlot, route.routeGeneration);
            }

            if (!routeCanRender)
            {
                voice->setGainTarget(0.0f, 0.010f, sampleRate);
                continue;
            }

            const auto& speaker = scene.speakers[route.speakerSlot];
            voice->speakerCharacter.setType(speaker.type);
            const auto pan = spatial::panForSpeakerAndListener(speaker.position, scene.listener);
            const auto pathDistance = std::max(1.0f, spatial::distance(speaker.position, scene.listener));
            const auto targetGain = route.enabled
                ? spatial::gainForRelativePath(6.0f, pathDistance) * route.linearGain * speaker.linearGain
                : 0.0f;
            voice->setGainTarget(targetGain, route.enabled ? 0.020f : 0.010f, sampleRate);
            voice->setPanTarget(pan, 0.020f, sampleRate);
        }

        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getWritePointer(1);
        const auto sampleCount = buffer.getNumSamples();
        for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
        {
            const auto sourceMono = 0.5f * (left[sampleIndex] + right[sampleIndex]);
            float outputLeft = 0.0f;
            float outputRight = 0.0f;

            for (auto& voice : voices)
            {
                if (voice.state == VoiceState::free)
                    continue;

                voice.advanceSmoothers();
                auto sample = sourceMono * voice.currentGain;
                sample = voice.speakerCharacter.processSample(sample);
                sample = voice.genericResponse.processSample(sample, scene.genericResponseTone);
                outputLeft += sample * voice.currentPan.left;
                outputRight += sample * voice.currentPan.right;

                if (voice.state == VoiceState::retiring && voice.currentGain <= kSilentGain)
                    voice.release();
            }

            left[sampleIndex] = outputLeft * scene.masterLinearGain;
            right[sampleIndex] = outputRight * scene.masterLinearGain;
        }
    }

private:
    enum class VoiceState : std::uint8_t
    {
        free,
        active,
        retiring,
    };

    struct Voice final
    {
        void prepare(double sampleRate, int maximumBlockSize)
        {
            speakerCharacter.prepare(sampleRate, maximumBlockSize);
            genericResponse.prepare(sampleRate, maximumBlockSize, 1);
            release();
        }

        void activate(std::uint16_t newRouteSlot, std::uint32_t newRouteGeneration) noexcept
        {
            state = VoiceState::active;
            routeSlot = newRouteSlot;
            routeGeneration = newRouteGeneration;
            retirementOrdinal = 0;
            currentGain = 0.0f;
            targetGain = 0.0f;
            gainStep = 1.0f;
            currentPan = {};
            targetPan = {};
            panLeftStep = 1.0f;
            panRightStep = 1.0f;
            speakerCharacter.reset();
            genericResponse.reset();
        }

        void release() noexcept
        {
            state = VoiceState::free;
            routeSlot = 0;
            routeGeneration = 0;
            retirementOrdinal = 0;
            currentGain = 0.0f;
            targetGain = 0.0f;
            gainStep = 1.0f;
            currentPan = {};
            targetPan = {};
            panLeftStep = 1.0f;
            panRightStep = 1.0f;
            speakerCharacter.reset();
            genericResponse.reset();
        }

        void setGainTarget(float target, float seconds, double sampleRate) noexcept
        {
            const auto clampedTarget = std::max(0.0f, target);
            if (std::abs(targetGain - clampedTarget) <= 1.0e-7f)
                return;
            targetGain = clampedTarget;
            gainStep = std::abs(targetGain - currentGain) / std::max(1.0, seconds * sampleRate);
        }

        void setPanTarget(spatial::StereoGains target, float seconds, double sampleRate) noexcept
        {
            if (std::abs(targetPan.left - target.left) <= 1.0e-7f
                && std::abs(targetPan.right - target.right) <= 1.0e-7f)
                return;
            targetPan = target;
            panLeftStep = std::abs(targetPan.left - currentPan.left) / std::max(1.0, seconds * sampleRate);
            panRightStep = std::abs(targetPan.right - currentPan.right) / std::max(1.0, seconds * sampleRate);
        }

        void advanceSmoothers() noexcept
        {
            const auto moveToward = [](float current, float target, float step)
            {
                if (current < target)
                    return std::min(target, current + step);
                return std::max(target, current - step);
            };
            currentGain = moveToward(currentGain, targetGain, gainStep);
            currentPan.left = moveToward(currentPan.left, targetPan.left, panLeftStep);
            currentPan.right = moveToward(currentPan.right, targetPan.right, panRightStep);
        }

        VoiceState state = VoiceState::free;
        std::uint16_t routeSlot = 0;
        std::uint32_t routeGeneration = 0;
        std::uint64_t retirementOrdinal = 0;
        float currentGain = 0.0f;
        float targetGain = 0.0f;
        float gainStep = 1.0f;
        spatial::StereoGains currentPan {};
        spatial::StereoGains targetPan {};
        float panLeftStep = 1.0f;
        float panRightStep = 1.0f;
        SpeakerCharacterResponse speakerCharacter;
        GenericSpeakerResponse genericResponse;
    };

    [[nodiscard]] static bool isRenderable(const CompiledRoute& route,
                                           const RealtimeSceneSnapshot& scene) noexcept
    {
        if (route.mode != RouteMode::full || route.inputMode != InputChannelMode::sumMono
            || route.speakerSlot >= kMaxSpeakers)
            return false;

        const auto& speaker = scene.speakers[route.speakerSlot];
        return speaker.active && speaker.generation == route.speakerGeneration;
    }

    [[nodiscard]] static bool planContainsIdentity(const SourceRoutePlan& plan,
                                                    std::size_t routeCount,
                                                    const Voice& voice) noexcept
    {
        for (std::size_t index = 0; index < routeCount; ++index)
        {
            const auto& route = plan.routes[index];
            if (route.routeSlot == voice.routeSlot && route.routeGeneration == voice.routeGeneration)
                return true;
        }
        return false;
    }

    [[nodiscard]] Voice* findVoice(VoiceState state,
                                   std::uint16_t routeSlot,
                                   std::uint32_t routeGeneration) noexcept
    {
        const auto found = std::find_if(voices.begin(), voices.end(), [=](const Voice& voice)
        {
            return voice.state == state && voice.routeSlot == routeSlot
                && voice.routeGeneration == routeGeneration;
        });
        return found == voices.end() ? nullptr : &*found;
    }

    [[nodiscard]] Voice* acquireFreeVoice() noexcept
    {
        if (const auto freeVoice = std::find_if(voices.begin(), voices.end(), [](const Voice& voice)
            { return voice.state == VoiceState::free; }); freeVoice != voices.end())
            return &*freeVoice;

        // A new retirement while 16 previous retirements are still fading can
        // fill all 32 Voice slots. Finish the oldest retiring Voice first;
        // this is diagnostic-visible but never allocates or waits.
        const auto oldest = std::min_element(voices.begin(), voices.end(), [](const Voice& lhs, const Voice& rhs)
        {
            const auto lhsOrdinal = lhs.state == VoiceState::retiring ? lhs.retirementOrdinal : UINT64_MAX;
            const auto rhsOrdinal = rhs.state == VoiceState::retiring ? rhs.retirementOrdinal : UINT64_MAX;
            return lhsOrdinal < rhsOrdinal;
        });
        if (oldest == voices.end() || oldest->state != VoiceState::retiring)
            return nullptr;

        oldest->release();
        ++droppedRetiringVoiceCount;
        return &*oldest;
    }

    void retire(Voice& voice) noexcept
    {
        if (voice.state != VoiceState::active)
            return;

        if (countVoices(VoiceState::retiring) >= kMaxRetiringVoices)
        {
            const auto oldest = std::min_element(voices.begin(), voices.end(), [](const Voice& lhs, const Voice& rhs)
            {
                const auto lhsOrdinal = lhs.state == VoiceState::retiring ? lhs.retirementOrdinal : UINT64_MAX;
                const auto rhsOrdinal = rhs.state == VoiceState::retiring ? rhs.retirementOrdinal : UINT64_MAX;
                return lhsOrdinal < rhsOrdinal;
            });
            if (oldest != voices.end() && oldest->state == VoiceState::retiring)
            {
                oldest->release();
                ++droppedRetiringVoiceCount;
            }
        }

        voice.state = VoiceState::retiring;
        voice.retirementOrdinal = nextRetirementOrdinal++;
        voice.setGainTarget(0.0f, 0.010f, sampleRate);
    }

    [[nodiscard]] std::size_t countVoices(VoiceState state) const noexcept
    {
        return static_cast<std::size_t>(std::count_if(voices.begin(), voices.end(), [state](const Voice& voice)
        {
            return voice.state == state;
        }));
    }

    static constexpr float kSilentGain = 1.0e-5f;
    std::array<Voice, kVoicePoolSize> voices {};
    double sampleRate = 44100.0;
    std::uint64_t nextRetirementOrdinal = 1;
    std::uint32_t droppedRetiringVoiceCount = 0;
};

} // namespace clubcraft
