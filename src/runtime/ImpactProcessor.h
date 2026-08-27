#pragma once

namespace RE
{
    class Projectile;
}

namespace BPR::Runtime
{
    enum class ImpactPhase : std::uint8_t
    {
        kImpactAdded,
        kNativeProcessing
    };

    void ProcessProjectileImpact(RE::Projectile& projectile, ImpactPhase phase) noexcept;
}
