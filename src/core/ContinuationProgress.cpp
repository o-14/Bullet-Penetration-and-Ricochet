#include "core/ContinuationProgress.h"

#include <cmath>

namespace BPR::Core
{
    std::optional<float> ForwardProgress(
        Vector3 launchPoint,
        Vector3 requestedDirection,
        Vector3 impactPoint) noexcept
    {
        const auto direction = Normalize(requestedDirection);
        if (!direction || !IsFinite(launchPoint) || !IsFinite(impactPoint)) {
            return std::nullopt;
        }
        const float progress = Dot(Subtract(impactPoint, launchPoint), *direction);
        return std::isfinite(progress) ? std::optional<float>{ progress } : std::nullopt;
    }

    bool HasValidForwardProgress(
        Vector3 launchPoint,
        Vector3 requestedDirection,
        Vector3 impactPoint,
        float backtrackTolerance) noexcept
    {
        if (!std::isfinite(backtrackTolerance) || backtrackTolerance < 0.0F) {
            return false;
        }
        const auto progress = ForwardProgress(launchPoint, requestedDirection, impactPoint);
        return progress && *progress >= -backtrackTolerance;
    }
}
