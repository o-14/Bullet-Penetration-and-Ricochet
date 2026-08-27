#pragma once

#include "config/Settings.h"

#include <memory>
#include <string_view>

namespace RE
{
    class BGSMaterialType;
    class BGSProjectile;
    class TESAmmo;
}

namespace BPR::Runtime
{
    struct ResolvedSurface
    {
        Core::SurfaceProfile profile;
        std::string_view runtimeName;
        std::string_view familyName;
        bool fallback{ true };
        bool inferred{ false };
    };

    struct ConfigurationSnapshot;
    using Configuration = std::shared_ptr<const ConfigurationSnapshot>;

    void ReloadConfiguration() noexcept;
    [[nodiscard]] Configuration AcquireConfiguration() noexcept;
    [[nodiscard]] const Config::RuntimeSettings& GlobalSettings(
        const ConfigurationSnapshot& configuration) noexcept;
    [[nodiscard]] Core::ProjectileProfile AmmoProfile(
        const ConfigurationSnapshot& configuration,
        const RE::TESAmmo* ammo,
        const RE::BGSProjectile* projectileBase) noexcept;
    [[nodiscard]] float AmmoDepth(
        const ConfigurationSnapshot& configuration,
        const RE::TESAmmo* ammo) noexcept;
    [[nodiscard]] ResolvedSurface Surface(
        const ConfigurationSnapshot& configuration,
        const RE::BGSMaterialType* material) noexcept;
}
