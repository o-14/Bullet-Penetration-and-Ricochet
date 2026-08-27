#pragma once

#include "core/Vector3.h"

#include <optional>

namespace BPR::Core
{
    inline constexpr float kContinuationBacktrackTolerance = 0.25F;

    [[nodiscard]] std::optional<float> ForwardProgress(
        Vector3 launchPoint,
        Vector3 requestedDirection,
        Vector3 impactPoint) noexcept;

    [[nodiscard]] bool HasValidForwardProgress(
        Vector3 launchPoint,
        Vector3 requestedDirection,
        Vector3 impactPoint,
        float backtrackTolerance = kContinuationBacktrackTolerance) noexcept;
}
