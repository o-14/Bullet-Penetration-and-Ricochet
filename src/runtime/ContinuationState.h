#pragma once

#include "core/ImpactPolicy.h"
#include "core/Vector3.h"

#include "RE/B/BSPointerHandle.h"

#include <cstdint>

namespace RE
{
    class Projectile;
}

namespace BPR::Runtime
{
    struct ChainState
    {
        std::uint64_t chainID{ 0 };
        std::uint64_t lastImpactToken{ 0 };
        std::uintptr_t lastActor{ 0 };
        std::uint32_t penetrationCount{ 0 };
        std::uint32_t reboundCount{ 0 };
        float remainingFraction{ 1.0F };
        float remainingRange{ -1.0F };
        Core::Vector3 launchPoint;
        Core::Vector3 requestedDirection;
        Core::ShooterClass shooterClass{ Core::ShooterClass::kUnknown };
        bool validateForwardProgress{ false };
    };

    [[nodiscard]] std::uint32_t ProjectileHandleValue(RE::Projectile& projectile) noexcept;
    [[nodiscard]] bool ClaimImpact(
        std::uint32_t projectileHandle,
        std::uint64_t impactToken,
        ChainState& output) noexcept;
    void RegisterContinuation(
        std::uint32_t continuationHandle,
        const ChainState& state,
        RE::ObjectRefHandle deferredShooter) noexcept;
    [[nodiscard]] bool ApplyDeferredShooter(
        std::uint32_t projectileHandle,
        RE::Projectile& projectile) noexcept;
    void EraseProjectile(std::uint32_t projectileHandle) noexcept;
    void ClearAllChains() noexcept;
}
