#include "core/ContinuationPlacement.h"

#include <algorithm>
#include <cmath>

namespace BPR::Core
{
    namespace
    {
        // Fallout's missile sweep needs its center placed beyond the impact by
        // more than the shape radius. This compatibility margin is retained
        // only for missile-style projectile bodies.
        constexpr float kSweptBodySeparation = 4.0F;
    }

    ContinuationPlacement PlaceContinuation(
        Vector3 impact,
        Vector3 outgoing,
        Vector3 surfaceNormal,
        float collisionRadius,
        float surfaceTolerance,
        ContinuationPlacementMode mode) noexcept
    {
        ContinuationPlacement result{ .origin = impact, .mode = mode };
        const auto direction = Normalize(outgoing);
        if (!direction || !IsFinite(impact)) {
            return result;
        }

        const float radius = std::isfinite(collisionRadius) ?
            std::clamp(collisionRadius, 0.0F, 64.0F) : 0.0F;
        const float tolerance = std::isfinite(surfaceTolerance) ?
            std::clamp(surfaceTolerance, 0.05F, 2.0F) : 0.35F;

        if (mode == ContinuationPlacementMode::kMissilePenetration) {
            result.forwardClearance = radius + kSweptBodySeparation;
            result.usedSweptBodyClearance = true;
            result.origin = Add(impact, Scale(*direction, result.forwardClearance));
            return result;
        }

        if (mode == ContinuationPlacementMode::kMissileRebound) {
            if (auto normal = Normalize(surfaceNormal)) {
                if (Dot(*direction, *normal) < 0.0F) {
                    *normal = Scale(*normal, -1.0F);
                }
                result.normalClearance = radius + 2.5F;
                result.forwardClearance = radius * 0.5F + 1.0F;
                result.usedSurfaceNormal = true;
                result.usedSweptBodyClearance = true;
                result.origin = Add(
                    impact,
                    Add(
                        Scale(*normal, result.normalClearance),
                        Scale(*direction, result.forwardClearance)));
                return result;
            }
            result.forwardClearance = radius + kSweptBodySeparation;
            result.usedSweptBodyClearance = true;
            result.origin = Add(impact, Scale(*direction, result.forwardClearance));
            return result;
        }

        result.forwardClearance = tolerance;

        if (auto normal = Normalize(surfaceNormal)) {
            if (Dot(*direction, *normal) < 0.0F) {
                *normal = Scale(*normal, -1.0F);
            }
            result.normalClearance = radius + tolerance;
            result.usedSurfaceNormal = true;
            result.origin = Add(
                impact,
                Add(
                    Scale(*normal, result.normalClearance),
                    Scale(*direction, result.forwardClearance)));
            return result;
        }

        result.forwardClearance = radius + tolerance * 2.0F;
        result.origin = Add(impact, Scale(*direction, result.forwardClearance));
        return result;
    }
}
