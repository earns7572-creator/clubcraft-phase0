#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace clubcraft
{

/**
    The musical role assigned to a virtual speaker.

    A type describes a deliberately broad tonal role. It is not a measurement
    of a named physical loudspeaker model, and all types are rendered through
    the same safe spatial path.
*/
enum class SpeakerType : std::uint8_t
{
    sub = 0,
    woofer,
    fullRange,
    mid,
    high,
};

inline constexpr std::size_t kSpeakerTypeCount = 5;

[[nodiscard]] constexpr int speakerTypeToParameterIndex(SpeakerType type) noexcept
{
    return static_cast<int>(type);
}

[[nodiscard]] constexpr SpeakerType speakerTypeFromParameterIndex(int index) noexcept
{
    switch (index)
    {
        case speakerTypeToParameterIndex(SpeakerType::sub):
            return SpeakerType::sub;
        case speakerTypeToParameterIndex(SpeakerType::woofer):
            return SpeakerType::woofer;
        case speakerTypeToParameterIndex(SpeakerType::mid):
            return SpeakerType::mid;
        case speakerTypeToParameterIndex(SpeakerType::high):
            return SpeakerType::high;
        case speakerTypeToParameterIndex(SpeakerType::fullRange):
        default:
            return SpeakerType::fullRange;
    }
}

[[nodiscard]] constexpr std::string_view speakerTypeName(SpeakerType type) noexcept
{
    switch (type)
    {
        case SpeakerType::sub:
            return "SUB";
        case SpeakerType::woofer:
            return "WOOFER";
        case SpeakerType::fullRange:
            return "FULL RANGE";
        case SpeakerType::mid:
            return "MID";
        case SpeakerType::high:
            return "HIGH";
    }

    return "FULL RANGE";
}

} // namespace clubcraft
