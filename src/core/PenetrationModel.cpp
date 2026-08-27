#include "core/PenetrationModel.h"

#include <algorithm>
#include <cmath>

namespace BPR::Core
{
    // BPR's ammo-, projectile-, receiver-, and material-based depth model.
    float MaximumDepth(
        float ammoBasePenetration,
        float projectileProfileMultiplier,
        float receiverMultiplier,
        float materialMultiplier) noexcept
    {
        const float value = ammoBasePenetration * projectileProfileMultiplier *
            receiverMultiplier * materialMultiplier;
        return std::isfinite(value) && value > 0.0F ? value : 0.0F;
    }

    // Directly retained BPR-authored receiver-instance calculation.
    float ReceiverMultiplier(
        float baseWeaponDamage,
        float configuredWeaponDamage,
        const ReceiverSettings& settings) noexcept
    {
        if (!settings.enabled || settings.exponent <= 0.0F ||
            !std::isfinite(baseWeaponDamage) || !std::isfinite(configuredWeaponDamage) ||
            baseWeaponDamage <= 0.0F || configuredWeaponDamage <= 0.0F) {
            return 1.0F;
        }

        const float lower = std::max(std::min(settings.minimum, settings.maximum), 0.01F);
        const float upper = std::max(std::max(settings.minimum, settings.maximum), lower);
        const float ratio = configuredWeaponDamage / baseWeaponDamage;
        const float value = std::pow(ratio, std::clamp(settings.exponent, 0.0F, 4.0F));
        return std::isfinite(value) ? std::clamp(value, lower, upper) : 1.0F;
    }

    // Directly retained BPR-authored nonlinear cumulative falloff.
    float RemainingFraction(
        float travelledThickness,
        float maximumDepth,
        float falloffExponent) noexcept
    {
        if (!std::isfinite(travelledThickness) || !std::isfinite(maximumDepth) ||
            !std::isfinite(falloffExponent) || maximumDepth <= 0.0F || falloffExponent <= 0.0F) {
            return 0.0F;
        }
        const float ratio = std::clamp(travelledThickness / maximumDepth, 0.0F, 1.0F);
        const float retained = 1.0F - std::pow(ratio, falloffExponent);
        return std::isfinite(retained) ? std::clamp(retained, 0.0F, 1.0F) : 0.0F;
    }

    Vector3 PenetrationExitDirection(
        Vector3 incoming,
        Vector3 exitNormal,
        float maximumVariationDegrees,
        float minimumOutwardAlignment,
        std::uint64_t seed) noexcept
    {
        const auto forward = Normalize(incoming);
        if (!forward) {
            return {};
        }
        if (!std::isfinite(maximumVariationDegrees) || maximumVariationDegrees <= 0.0F) {
            return *forward;
        }
        const auto outward = Normalize(exitNormal);
        if (!outward) {
            return *forward;
        }
        const Vector3 candidate = DeterministicConeVariation(
            *forward, std::clamp(maximumVariationDegrees, 0.0F, 12.0F), seed);
        const float requiredAlignment = std::clamp(minimumOutwardAlignment, 0.0F, 1.0F);
        return Dot(candidate, *outward) > requiredAlignment ? candidate : *forward;
    }

    bool ReachedLimit(std::uint32_t count, std::uint32_t maximum) noexcept
    {
        return maximum != 0 && count >= maximum;
    }
}
