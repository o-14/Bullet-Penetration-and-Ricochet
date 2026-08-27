#include "engine/ProjectileVisuals.h"

#include "runtime/ContinuationState.h"
#include "runtime/RuntimeConfig.h"
#include "pch.h"

#include <atomic>
#include <optional>

namespace BPR::Engine
{
    namespace
    {
        constexpr std::uint32_t kMaximumVisualDiagnostics = 256;
        std::optional<RE::BSFixedString> g_tracerNode;
        std::optional<RE::BSFixedString> g_vaporNode;
        std::atomic_bool g_installed{ false };
        std::atomic_uint32_t g_visualDiagnostics{ 0 };

        struct VisualSettings
        {
            bool suppress{ false };
            bool diagnostics{ false };
        };

        VisualSettings Settings() noexcept
        {
            const Runtime::Configuration configuration = Runtime::AcquireConfiguration();
            if (!configuration) {
                return {};
            }
            const Config::RuntimeSettings& settings = Runtime::GlobalSettings(*configuration);
            return { settings.suppressProjectileTrails, settings.detailedLogging };
        }

        RE::BGSProjectile* BaseProjectile(RE::Projectile& projectile) noexcept
        {
            RE::TESBoundObject* base = projectile.GetObjectReference();
            return base ? base->As<RE::BGSProjectile>() : nullptr;
        }

        bool ExplosiveVisual(RE::Projectile& projectile) noexcept
        {
            const RE::BGSProjectile* base = BaseProjectile(projectile);
            return projectile.explosion != nullptr || (base && base->data.explosionType != nullptr);
        }

        void Suppress(RE::Projectile& projectile, RE::NiAVObject* root, const char* phase) noexcept
        {
            const VisualSettings settings = Settings();
            if (!root || !settings.suppress || !g_tracerNode || !g_vaporNode) {
                return;
            }

            RE::NiAVObject* tracer = root->GetObjectByName(*g_tracerNode);
            RE::NiAVObject* vapor = root->GetObjectByName(*g_vaporNode);
            if (tracer) {
                tracer->SetAppCulled(true);
            }
            if (vapor) {
                vapor->SetAppCulled(true);
            }

            // BeamEnd is an endpoint marker, not the full tracer geometry. When
            // either stock marker is present, cull the complete non-explosive
            // projectile effect root so its procedural beam cannot remain visible.
            // Explosive projectile roots may contain a visible missile model, so
            // only their named trail components are suppressed.
            const bool markerPresent = tracer != nullptr || vapor != nullptr;
            const bool explosive = ExplosiveVisual(projectile);
            const bool rootCulled = markerPresent && !explosive;
            if (rootCulled) {
                root->SetAppCulled(true);
            }

            if (settings.diagnostics) {
                const std::uint32_t sequence =
                    g_visualDiagnostics.fetch_add(1, std::memory_order_relaxed);
                if (sequence < kMaximumVisualDiagnostics) {
                    REX::INFO(
                        "[BPR-DIAG] projectile visuals phase={} handle={:08X} tracerMarker={} vaporMarker={} rootCulled={} explosive={} controllerMutation=false",
                        phase, Runtime::ProjectileHandleValue(projectile), tracer != nullptr,
                        vapor != nullptr, rootCulled, explosive);
                } else if (sequence == kMaximumVisualDiagnostics) {
                    REX::INFO("[BPR-DIAG] projectile visual diagnostics capped at {} events",
                        kMaximumVisualDiagnostics);
                }
            }
        }

        template <class ProjectileType>
        struct PostLoadHook
        {
            using Function = void (*)(ProjectileType*, RE::NiAVObject*);
            inline static std::uintptr_t original{ 0 };

            static void Dispatch(ProjectileType* projectile, RE::NiAVObject* object) noexcept
            {
                const auto function = reinterpret_cast<Function>(original);
                if (function) {
                    function(projectile, object);
                }
                if (projectile) {
                    Suppress(*projectile, object ? object : projectile->Get3D(), "post-load");
                }
            }

            static void Install(REL::ID vtable)
            {
                REL::Relocation<std::uintptr_t> table{ vtable };
                original = table.write_vfunc(0xCE, Dispatch);
            }
        };
    }

    bool InstallProjectileVisualHooks() noexcept
    {
        if (g_installed.exchange(true)) {
            return true;
        }
        try {
            g_tracerNode.emplace("BeamEnd");
            g_vaporNode.emplace("Lightning02SmokeBullets");
            PostLoadHook<RE::Projectile>::Install(RE::Projectile::VTABLE[0]);
            PostLoadHook<RE::MissileProjectile>::Install(RE::MissileProjectile::VTABLE[0]);
            PostLoadHook<RE::BeamProjectile>::Install(RE::BeamProjectile::VTABLE[0]);
            PostLoadHook<RE::ArrowProjectile>::Install(RE::ArrowProjectile::VTABLE[0]);
            REX::INFO("BPR projectile tracer and vapor visibility hooks installed");
            return true;
        } catch (const std::exception& exception) {
            REX::WARN("BPR projectile visual hooks unavailable: {}", exception.what());
        } catch (...) {
            REX::WARN("BPR projectile visual hooks unavailable");
        }
        g_installed.store(false);
        return false;
    }

}
