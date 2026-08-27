#pragma once

#include <cstdint>

namespace BPR::Core
{
    enum class ShooterClass : std::uint8_t
    {
        kUnknown,
        kPlayer,
        kNPC
    };

    enum class ImpactOwnerClass : std::uint8_t
    {
        kUnknown,
        kWorld,
        kProp,
        kActor
    };

    struct ShooterControlSettings
    {
        bool enablePenetration{ true };
        bool enableRicochet{ true };
        std::uint32_t maxPenetrations{ 0 };
        std::uint32_t maxRicochets{ 2 };
    };

    struct ImpactPolicySettings
    {
        ShooterControlSettings player;
        ShooterControlSettings npc;
        ShooterControlSettings unknown;
        bool allowPropRicochets{ true };
    };

    [[nodiscard]] const ShooterControlSettings& ControlsFor(
        ShooterClass shooter,
        const ImpactPolicySettings& settings) noexcept;

    [[nodiscard]] bool AllowsPenetration(
        ShooterClass shooter,
        const ImpactPolicySettings& settings) noexcept;

    [[nodiscard]] bool AllowsRicochet(
        ShooterClass shooter,
        ImpactOwnerClass owner,
        bool ricochetEnabled,
        const ImpactPolicySettings& settings) noexcept;
}
