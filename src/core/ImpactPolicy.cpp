#include "core/ImpactPolicy.h"

namespace BPR::Core
{
    const ShooterControlSettings& ControlsFor(
        ShooterClass shooter,
        const ImpactPolicySettings& settings) noexcept
    {
        switch (shooter) {
        case ShooterClass::kPlayer:
            return settings.player;
        case ShooterClass::kNPC:
            return settings.npc;
        case ShooterClass::kUnknown:
        default:
            return settings.unknown;
        }
    }

    bool AllowsPenetration(
        ShooterClass shooter,
        const ImpactPolicySettings& settings) noexcept
    {
        return ControlsFor(shooter, settings).enablePenetration;
    }

    bool AllowsRicochet(
        ShooterClass shooter,
        ImpactOwnerClass owner,
        bool ricochetEnabled,
        const ImpactPolicySettings& settings) noexcept
    {
        if (!ricochetEnabled || !ControlsFor(shooter, settings).enableRicochet) {
            return false;
        }
        return owner != ImpactOwnerClass::kProp || settings.allowPropRicochets;
    }
}
