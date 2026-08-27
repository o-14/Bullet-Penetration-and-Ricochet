#pragma once

#include "core/Vector3.h"

namespace BPR::Core
{
    enum class ContinuationPlacementMode
    {
        kSurfaceSeparated,
        kMissilePenetration,
        kMissileRebound
    };

    struct ContinuationPlacement
    {
        Vector3 origin;
        float normalClearance{ 0.0F };
        float forwardClearance{ 0.0F };
        bool usedSurfaceNormal{ false };
        bool usedSweptBodyClearance{ false };
        ContinuationPlacementMode mode{ ContinuationPlacementMode::kSurfaceSeparated };
    };

    [[nodiscard]] ContinuationPlacement PlaceContinuation(
        Vector3 impact,
        Vector3 outgoing,
        Vector3 surfaceNormal,
        float collisionRadius,
        float surfaceTolerance,
        ContinuationPlacementMode mode = ContinuationPlacementMode::kSurfaceSeparated) noexcept;
}
