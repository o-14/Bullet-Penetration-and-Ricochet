#pragma once

#include "core/ThicknessSolver.h"

#include <array>
#include <cstddef>

namespace RE
{
    class Actor;
    class BGSProjectile;
    class NiPoint3;
    class Projectile;
    class bhkNPCollisionObject;
}

namespace BPR::Engine
{
    struct BoundaryBatch
    {
        std::array<Core::BoundaryHit, Core::kMaximumBoundaryHits> hits{};
        std::size_t count{ 0 };
        std::size_t dropped{ 0 };
        bool completed{ false };
    };

    [[nodiscard]] std::uintptr_t CollisionBodyIdentity(
        const RE::bhkNPCollisionObject* collisionObject) noexcept;

    [[nodiscard]] BoundaryBatch QueryBoundaries(
        RE::Projectile& projectile,
        RE::Actor* shooter,
        RE::BGSProjectile& projectileBase,
        const RE::NiPoint3& start,
        const RE::NiPoint3& end,
        bool diagnostics) noexcept;
}
