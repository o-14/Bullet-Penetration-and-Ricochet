#include "core/SurfaceResponse.h"

#include <algorithm>
#include <cmath>

namespace BPR::Core
{
    namespace
    {
        constexpr float kMinimumGameplayDamage = 0.001F;
        constexpr float kMinimumRemainingFraction = 0.0001F;
        // Material angles are authored against the default Ricochet Angle of
        // 70, which gives a 20-degree plane-angle window. Scaling by the
        // player's current window preserves the family ordering while making
        // the global control useful when material angles are enabled.
        constexpr float kMaterialAngleReferenceWindowDegrees = 20.0F;

        float BehaviorCost(SurfaceBehavior behavior) noexcept
        {
            switch (behavior) {
            case SurfaceBehavior::kFrangible:
                return 0.76F;
            case SurfaceBehavior::kRigid:
                return 0.92F;
            case SurfaceBehavior::kFlexible:
                return 1.18F;
            case SurfaceBehavior::kSoft:
                return 1.55F;
            case SurfaceBehavior::kLiquid:
            case SurfaceBehavior::kSuppressRebound:
            default:
                return 0.0F;
            }
        }
    }

    ReboundResult EvaluateRebound(const ReboundInput& input) noexcept
    {
        ReboundResult result;
        const bool optInMaterialSurface = input.settings.useMaterialAngles &&
            input.surface.materialRicochetEligible;
        const bool passiveSurface =
            input.surface.behavior == SurfaceBehavior::kLiquid ||
            input.surface.behavior == SurfaceBehavior::kSuppressRebound;
        if (!input.settings.enabled ||
            (passiveSurface && !optInMaterialSurface) ||
            (input.projectile.energyBeam && !input.surface.conductive)) {
            return result;
        }

        const auto planeAngle = PlaneAngleDegrees(input.incoming, input.surfaceNormal);
        if (!planeAngle) {
            return result;
        }
        result.planeAngleDegrees = *planeAngle;

        const float globalPlaneLimit = 90.0F -
            std::clamp(input.settings.headOnExclusionDegrees, 0.0F, 90.0F);
        if (*planeAngle > globalPlaneLimit) {
            return result;
        }

        float configuredSurfaceLimit = input.surface.glancingLimitDegrees;
        if (input.settings.useMaterialAngles) {
            const float materialScale = globalPlaneLimit / kMaterialAngleReferenceWindowDegrees;
            configuredSurfaceLimit = input.surface.materialRicochetAngleDegrees * materialScale;
        }
        const float surfacePlaneLimit = std::clamp(
            std::min(globalPlaneLimit,
                configuredSurfaceLimit * input.projectile.glancingTolerance),
            0.0F, 90.0F);
        const bool surfaceLimitApplies =
            input.mode == ReboundMode::kGlancingPriority || input.settings.useMaterialAngles;
        result.effectiveAngleLimitDegrees = surfaceLimitApplies ? surfacePlaneLimit : globalPlaneLimit;
        if (surfaceLimitApplies && *planeAngle > surfacePlaneLimit) {
            return result;
        }

        const float gameplayDamage = std::max(input.originalDamage, 0.0F);
        const float carriedFraction = std::clamp(input.currentFraction, 0.0F, 1.0F);
        if (!std::isfinite(gameplayDamage) || gameplayDamage <= kMinimumGameplayDamage ||
            !std::isfinite(carriedFraction) || carriedFraction <= kMinimumRemainingFraction) {
            return result;
        }

        const float incidence = std::clamp(*planeAngle / 90.0F, 0.0F, 1.0F);
        const float repeatMultiplier = 1.0F +
            std::max(input.settings.repeatPenalty, 0.0F) * static_cast<float>(input.priorRebounds);
        const float legacySurfaceCost = passiveSurface ?
            1.0F :
            std::max(input.surface.reboundCostScale, 0.0F) * BehaviorCost(input.surface.behavior);
        const float surfaceCost = input.settings.useMaterialLoss ?
            std::clamp(input.surface.materialRicochetLossScale, 0.75F, 1.25F) :
            legacySurfaceCost;
        const float responseCostPercent =
            (std::max(input.settings.baseEnergyCost, 0.0F) +
                std::max(input.settings.incidenceEnergyCost, 0.0F) * incidence * incidence) *
            surfaceCost *
            std::max(input.projectile.reboundCostScale, 0.0F) *
            repeatMultiplier;

        if (!std::isfinite(responseCostPercent) ||
            responseCostPercent <= 0.0F || responseCostPercent >= 100.0F) {
            return result;
        }

        const float chance = std::clamp(input.settings.chancePercent, 0.0F, 100.0F);
        if (chance <= 0.0F) {
            result.chanceRejected = true;
            return result;
        }
        if (chance < 100.0F) {
            result.chanceRollPercent = DeterministicUnitFloat(
                input.variationSeed ^ 0xD1B54A32D192ED03ULL) * 100.0F;
            if (result.chanceRollPercent >= chance) {
                result.chanceRejected = true;
                return result;
            }
        }

        const Vector3 reflected = ReflectedDirection(input.incoming, input.surfaceNormal);
        if (!Normalize(reflected)) {
            return result;
        }

        result.energyCostPercent = responseCostPercent;
        result.remainingFraction = std::clamp(
            carriedFraction * (1.0F - responseCostPercent / 100.0F), 0.0F, 1.0F);
        if (result.remainingFraction <= kMinimumRemainingFraction) {
            return result;
        }
        result.direction = DeterministicConeVariation(
            reflected, input.settings.variationDegrees, input.variationSeed);
        result.accepted = Normalize(result.direction).has_value();
        return result;
    }
}
