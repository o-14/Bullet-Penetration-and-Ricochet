#pragma once

#include <cstdint>

namespace BPR::Core
{
    // BGSProjectileData::flags combines the PROJ record's low-word flags with
    // its high-word projectile type. Fallout assigns Type=Beam to conventional
    // hitscan rounds as well as lasers. Its ballistic beam records also set the
    // Alt Trigger flag, while the energy-beam records do not. Exact ammo INI
    // profiles are resolved before this record-based fallback is consulted.
    inline constexpr std::uint32_t kProjectileRecordAltTrigger = 0x00000004U;
    inline constexpr std::uint32_t kProjectileRecordTypeMask = 0xFFFF0000U;
    inline constexpr std::uint32_t kProjectileRecordTypeBeam = 0x00040000U;

    [[nodiscard]] constexpr bool UsesEnergyBeamProfile(std::uint32_t flags) noexcept
    {
        return (flags & kProjectileRecordTypeMask) == kProjectileRecordTypeBeam &&
               (flags & kProjectileRecordAltTrigger) == 0;
    }
}
