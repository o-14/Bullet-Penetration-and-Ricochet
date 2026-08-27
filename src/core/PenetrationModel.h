#pragma once

#include "core/Vector3.h"

#include <cstdint>

namespace BPR::Core
{
    struct ReceiverSettings
    {
        bool enabled{ true };
        float exponent{ 1.0F };
        float minimum{ 0.25F };
        float maximum{ 4.0F };
    };

    [[nodiscard]] float MaximumDepth(
        float ammoBasePenetration,
        float projectileProfileMultiplier,
        float receiverMultiplier,
        float materialMultiplier) noexcept;

    [[nodiscard]] float ReceiverMultiplier(
        float baseWeaponDamage,
        float configuredWeaponDamage,
        const ReceiverSettings& settings) noexcept;

    [[nodiscard]] float RemainingFraction(
        float travelledThickness,
        float maximumDepth,
        float falloffExponent) noexcept;

    [[nodiscard]] Vector3 PenetrationExitDirection(
        Vector3 incoming,
        Vector3 exitNormal,
        float maximumVariationDegrees,
        float minimumOutwardAlignment,
        std::uint64_t seed) noexcept;

    [[nodiscard]] bool ReachedLimit(
        std::uint32_t count,
        std::uint32_t maximum) noexcept;
}
