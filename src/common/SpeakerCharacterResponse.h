#pragma once

#include <juce_dsp/juce_dsp.h>

#include "SpeakerType.h"

namespace clubcraft
{

/**
    A conservative tonal role for one virtual speaker.

    The response intentionally uses broad crossover-like ranges rather than
    steep, model-specific EQ curves. It gives the user a musical Speaker Type
    control while avoiding claims that these are measurements of real hardware.
    prepare() allocates all DSP state. setType() and processSample() are safe
    to call from the audio callback and do not allocate or lock.
*/
class SpeakerCharacterResponse final
{
public:
    void prepare(double sampleRate, int maximumBlockSize)
    {
        const juce::dsp::ProcessSpec spec {
            sampleRate,
            static_cast<juce::uint32>(maximumBlockSize),
            1U,
        };

        highPass.prepare(spec);
        lowPass.prepare(spec);
        highPass.setType(juce::dsp::StateVariableTPTFilterType::highpass);
        lowPass.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        configureFiltersForType(type);
        reset();
    }

    void reset() noexcept
    {
        highPass.reset();
        lowPass.reset();
    }

    void setType(SpeakerType newType) noexcept
    {
        if (type == newType)
            return;

        type = newType;
        configureFiltersForType(type);
        reset();
    }

    [[nodiscard]] SpeakerType getType() const noexcept
    {
        return type;
    }

    [[nodiscard]] float processSample(float input) noexcept
    {
        auto sample = input;
        if (usesHighPass)
            sample = highPass.processSample(0, sample);
        if (usesLowPass)
            sample = lowPass.processSample(0, sample);
        return sample;
    }

private:
    void configureFiltersForType(SpeakerType newType) noexcept
    {
        usesHighPass = false;
        usesLowPass = false;

        switch (newType)
        {
            case SpeakerType::sub:
                // Removes infrasonic energy, then retains the weight range.
                highPass.setCutoffFrequency(28.0f);
                lowPass.setCutoffFrequency(110.0f);
                usesHighPass = true;
                usesLowPass = true;
                break;

            case SpeakerType::woofer:
                highPass.setCutoffFrequency(55.0f);
                lowPass.setCutoffFrequency(420.0f);
                usesHighPass = true;
                usesLowPass = true;
                break;

            case SpeakerType::mid:
                highPass.setCutoffFrequency(220.0f);
                lowPass.setCutoffFrequency(4600.0f);
                usesHighPass = true;
                usesLowPass = true;
                break;

            case SpeakerType::high:
                highPass.setCutoffFrequency(2400.0f);
                usesHighPass = true;
                break;

            case SpeakerType::fullRange:
                // Preserve Phase 2 sound and existing session behaviour.
                break;
        }
    }

    SpeakerType type = SpeakerType::fullRange;
    bool usesHighPass = false;
    bool usesLowPass = false;
    juce::dsp::StateVariableTPTFilter<float> highPass;
    juce::dsp::StateVariableTPTFilter<float> lowPass;
};

} // namespace clubcraft
