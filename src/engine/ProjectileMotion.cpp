#include "engine/ProjectileMotion.h"

#include "pch.h"

#include <cmath>

namespace BPR::Engine
{
    namespace
    {
        Core::Vector3 ToCore(const RE::NiPoint3& value) noexcept
        {
            return { value.x, value.y, value.z };
        }

    }

    bool UsesMissileMotion(const RE::BGSProjectile& base) noexcept
    {
        constexpr auto flag = static_cast<std::uint32_t>(
            RE::BGSProjectile::BGSProjectileFlags::kMotionMissile);
        return (base.data.flags & flag) != 0;
    }

    Core::Vector3 CurrentTravelDirection(
        const RE::Projectile& projectile,
        const RE::BGSProjectile& base) noexcept
    {
        if (UsesMissileMotion(base)) {
            if (const auto velocity = Core::Normalize(ToCore(projectile.velocity))) {
                return *velocity;
            }
        }
        if (const auto movement = Core::Normalize(ToCore(projectile.movementDirection))) {
            return *movement;
        }
        if (const auto velocity = Core::Normalize(ToCore(projectile.velocity))) {
            return *velocity;
        }

        const float pitch = projectile.data.angle.x;
        const float yaw = projectile.data.angle.z;
        return Core::Normalize({
            std::cos(pitch) * std::sin(yaw),
            std::cos(pitch) * std::cos(yaw),
            -std::sin(pitch)
        }).value_or(Core::Vector3{});
    }

}
