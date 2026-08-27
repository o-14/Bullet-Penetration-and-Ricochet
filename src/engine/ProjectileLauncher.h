#pragma once

#include "core/Vector3.h"
#include "runtime/ContinuationState.h"

#include <cstdint>

namespace RE
{
    class Projectile;
}

namespace BPR::Engine
{
    enum class ContinuationKind : std::uint8_t
    {
        kPenetration,
        kRebound
    };

    [[nodiscard]] bool LaunchContinuation(
        RE::Projectile& source,
        Core::Vector3 origin,
        Core::Vector3 direction,
        Core::Vector3 surfaceNormal,
        float relativePowerScale,
        float remainingFraction,
        float surfaceTolerance,
        const Runtime::ChainState& state,
        ContinuationKind kind,
        bool diagnostics) noexcept;
}
