#pragma once

#include "core/Vector3.h"

namespace RE
{
    class BGSProjectile;
    class Projectile;
}

namespace BPR::Engine
{
    [[nodiscard]] bool UsesMissileMotion(const RE::BGSProjectile& base) noexcept;

    [[nodiscard]] Core::Vector3 CurrentTravelDirection(
        const RE::Projectile& projectile,
        const RE::BGSProjectile& base) noexcept;
}
