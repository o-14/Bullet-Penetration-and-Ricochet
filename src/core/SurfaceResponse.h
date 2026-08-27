#pragma once

#include "core/Vector3.h"

#include <cstdint>

namespace BPR::Core
{
    enum class SurfaceBehavior : std::uint8_t
    {
        kSoft,
        kFlexible,
        kFrangible,
        kRigid,
        kLiquid,
        kSuppressRebound
    };

    struct ProjectileProfile
    {
        float penetrationScale{ 1.0F };
        float glancingTolerance{ 1.0F };
        float reboundCostScale{ 1.0F };
        bool energyBeam{ false };
    };

    struct SurfaceProfile
    {
        float penetrationScale{ 0.82F };
        float glancingLimitDegrees{ 18.0F };
        float reboundCostScale{ 1.0F };
        SurfaceBehavior behavior{ SurfaceBehavior::kFlexible };
        bool conductive{ false };
    };

    struct ReboundSettings
    {
        bool enabled{ true };
        float chancePercent{ 50.0F };
        float headOnExclusionDegrees{ 70.0F };
        float baseEnergyCost{ 18.0F };
        float incidenceEnergyCost{ 24.0F };
        float repeatPenalty{ 0.30F };
        float variationDegrees{ 3.0F };
    };

    enum class ReboundMode : std::uint8_t
    {
        kGlancingPriority,
        kResidualEnergy
    };

    struct ReboundInput
    {
        Vector3 incoming;
        Vector3 surfaceNormal;
        ProjectileProfile projectile;
        SurfaceProfile surface;
        ReboundSettings settings;
        float originalDamage{ 0.0F };
        float currentFraction{ 1.0F };
        std::uint32_t priorRebounds{ 0 };
        std::uint64_t variationSeed{ 0 };
        ReboundMode mode{ ReboundMode::kGlancingPriority };
    };

    struct ReboundResult
    {
        bool accepted{ false };
        bool chanceRejected{ false };
        float chanceRollPercent{ 0.0F };
        float planeAngleDegrees{ 0.0F };
        float energyCostPercent{ 0.0F };
        float remainingFraction{ 0.0F };
        Vector3 direction;
    };

    [[nodiscard]] ReboundResult EvaluateRebound(const ReboundInput& input) noexcept;
}
