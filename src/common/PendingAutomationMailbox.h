#pragma once

#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace clubcraft
{
// The legacy APVTS bridge only exposes four historical speakers plus the
// scene-wide controls below.  This fixed enum makes parameterChanged() write
// one atomic word without touching DynamicScene or any control-side container.
enum class LegacyAutomationField : std::uint8_t
{
    masterLevel = 0,
    genericResponseTone,
    listenerPositionX,
    listenerPositionY,
    speakerLevel1,
    speakerLevel2,
    speakerLevel3,
    speakerLevel4,
    speakerType1,
    speakerType2,
    speakerType3,
    speakerType4,
    speakerPositionX1,
    speakerPositionX2,
    speakerPositionX3,
    speakerPositionX4,
    speakerPositionY1,
    speakerPositionY2,
    speakerPositionY3,
    speakerPositionY4,
    count,
};

inline constexpr std::size_t kLegacyAutomationFieldCount =
    static_cast<std::size_t>(LegacyAutomationField::count);

[[nodiscard]] constexpr std::size_t toAutomationIndex(LegacyAutomationField field) noexcept
{
    return static_cast<std::size_t>(field);
}

// A snapshot entry retains the exact packed word that was observed.  The word
// is used as the acknowledgement compare-and-swap value after Scene commit.
struct PendingAutomationEntry final
{
    float value = 0.0f;
    std::uint32_t revision = 0;
    std::uint64_t packed = 0;

    [[nodiscard]] bool isPending() const noexcept { return revision != 0; }
};

using PendingAutomationSnapshot =
    std::array<PendingAutomationEntry, kLegacyAutomationFieldCount>;

// Each field is a single atomic 64-bit word: [revision:32][float bits:32].
// A non-zero revision is both the dirty marker and the acknowledgement token.
// Unlike a destructive exchange, snapshot() never clears data.  A producer
// that writes after snapshot() changes the packed word, so acknowledge() will
// fail for the stale snapshot and preserve the newer automation value.
class PendingAutomationMailbox final
{
public:
    void store(LegacyAutomationField field, float value) noexcept
    {
        auto revision = nextRevision.fetch_add(1, std::memory_order_relaxed) + 1U;
        if (revision == 0U)
            revision = nextRevision.fetch_add(1, std::memory_order_relaxed) + 1U;

        const auto packed = (static_cast<std::uint64_t>(revision) << 32U)
            | static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(value));
        slots[toAutomationIndex(field)].store(packed, std::memory_order_release);
    }

    [[nodiscard]] PendingAutomationSnapshot snapshot() const noexcept
    {
        PendingAutomationSnapshot result {};
        for (std::size_t index = 0; index < slots.size(); ++index)
        {
            const auto packed = slots[index].load(std::memory_order_acquire);
            result[index] = {
                .value = std::bit_cast<float>(static_cast<std::uint32_t>(packed)),
                .revision = static_cast<std::uint32_t>(packed >> 32U),
                .packed = packed,
            };
        }
        return result;
    }

    [[nodiscard]] bool hasPending(const PendingAutomationSnapshot& values) const noexcept
    {
        for (const auto& value : values)
            if (value.isPending())
                return true;
        return false;
    }

    // Must be called only after the candidate Scene CAS commit succeeds.
    // A failed CAS means a newer callback write arrived, therefore it remains
    // pending for the next Timer / AsyncUpdater pass.
    void acknowledge(const PendingAutomationSnapshot& values) noexcept
    {
        for (std::size_t index = 0; index < slots.size(); ++index)
        {
            if (!values[index].isPending())
                continue;

            auto expected = values[index].packed;
            static_cast<void>(slots[index].compare_exchange_strong(
                expected, 0U, std::memory_order_acq_rel, std::memory_order_acquire));
        }
    }

    void clear() noexcept
    {
        for (auto& slot : slots)
            slot.store(0U, std::memory_order_release);
    }

private:
    std::array<std::atomic<std::uint64_t>, kLegacyAutomationFieldCount> slots {};
    std::atomic<std::uint32_t> nextRevision { 0U };
};
} // namespace clubcraft
