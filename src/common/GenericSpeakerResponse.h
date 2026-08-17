#pragma once

#include <algorithm>

#include <juce_dsp/juce_dsp.h>

namespace clubcraft
{

/**
    Generic, stable speaker coloration for Phase 1.

    Response Tone is normalized to 0.0–1.0 and maps to a conservative low-pass
    cutoff from 1.2 kHz (dark) to 18 kHz (bright). The filter is deliberately
    generic: it creates a useful, automatable response path without claiming a
    measurement-derived model of a particular loudspeaker.
*/
class GenericSpeakerResponse final
{
public:
    void prepare(double sampleRate, int maximumBlockSize, int channelCount)
    {
        juce::dsp::ProcessSpec spec {
            sampleRate,
            static_cast<juce::uint32>(maximumBlockSize),
            static_cast<juce::uint32>(channelCount),
        };

        filter.prepare(spec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        filter.setCutoffFrequency(cutoffForTone(1.0f));
    }

    void reset()
    {
        filter.reset();
    }

    void process(juce::AudioBuffer<float>& buffer, float responseTone) noexcept
    {
        filter.setCutoffFrequency(cutoffForTone(responseTone));
        juce::dsp::AudioBlock<float> block { buffer };
        juce::dsp::ProcessContextReplacing<float> context { block };
        filter.process(context);
    }

    [[nodiscard]] static float cutoffForTone(float responseTone) noexcept
    {
        constexpr float kDarkCutoffHz = 1200.0f;
        constexpr float kBrightCutoffHz = 18000.0f;
        const auto clampedTone = std::clamp(responseTone, 0.0f, 1.0f);
        return kDarkCutoffHz + (kBrightCutoffHz - kDarkCutoffHz) * clampedTone;
    }

private:
    juce::dsp::StateVariableTPTFilter<float> filter;
};

} // namespace clubcraft
