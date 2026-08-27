#pragma once

#include "config/IniDocument.h"
#include "core/ImpactPolicy.h"
#include "core/PenetrationModel.h"
#include "core/SurfaceResponse.h"
#include "core/ThicknessSolver.h"

#include <string>
#include <optional>
#include <string_view>

namespace BPR::Config
{
    struct RuntimeSettings
    {
        bool detailedLogging{ false };
        bool suppressProjectileTrails{ true };
        bool preventRepeatActor{ true };
        float damageFalloffExponent{ 4.0F };
        float penetrationVariationDegrees{ 0.0F };
        std::string defaultProjectileProfile{ "standard" };
        std::string defaultSurfaceFamily{ "general" };
        float defaultAmmoDepth{ 10.0F };
        Core::ReceiverSettings receiver;
        Core::ReboundSettings rebound;
        Core::ThicknessSettings thickness;
        Core::ImpactPolicySettings impactPolicy;
    };

    struct DataSettings
    {
        RuntimeSettings runtime;
        FoldedMap<Core::ProjectileProfile> projectileProfiles;
        FoldedMap<Core::SurfaceProfile> surfaceFamilies;
        FoldedMap<std::string> surfaceRecordAssignments;
        FoldedMap<std::string> surfaceAssignments;
        FoldedMap<std::string> surfaceAliases;
        FoldedMap<std::string> surfacePatterns;
    };

    struct SurfacePatternMatch
    {
        std::string_view pattern;
        std::string_view family;
    };

    [[nodiscard]] DataSettings BuiltInDefaults();
    void ApplyDocument(DataSettings& settings, const IniDocument& document);
    [[nodiscard]] std::optional<SurfacePatternMatch> MatchSurfacePattern(
        const DataSettings& settings,
        std::string_view identifier) noexcept;
}
