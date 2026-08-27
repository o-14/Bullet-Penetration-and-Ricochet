#include "engine/ImpactHooks.h"

#include "runtime/ContinuationState.h"
#include "runtime/ImpactProcessor.h"
#include "runtime/RuntimeConfig.h"
#include "pch.h"

#include <atomic>
#include <limits>

namespace BPR::Engine
{
    namespace
    {
        template <class ProjectileType>
        struct ProcessImpactHook
        {
            using Function = bool (*)(ProjectileType*);
            inline static std::uintptr_t original{ 0 };

            static bool Dispatch(ProjectileType* projectile) noexcept
            {
                if (projectile) {
                    Runtime::ProcessProjectileImpact(
                        *projectile, Runtime::ImpactPhase::kNativeProcessing);
                }
                const auto function = reinterpret_cast<Function>(original);
                return function ? function(projectile) : false;
            }

            static void Install(REL::ID vtable)
            {
                REL::Relocation<std::uintptr_t> table{ vtable };
                original = table.write_vfunc(0xD0, Dispatch);
            }
        };

        bool DiagnosticsEnabled() noexcept
        {
            const Runtime::Configuration configuration = Runtime::AcquireConfiguration();
            return configuration && Runtime::GlobalSettings(*configuration).detailedLogging;
        }

        template <class ProjectileType>
        struct AddImpactHook
        {
            // Dear Modding CommonLibF4 declares AddImpact at this virtual slot
            // for every supported runtime. Penetration System OG 1.2's shipped
            // symbols provide historical confirmation. BPR calls the native
            // function first and processes only the resulting impact record.
            using Function = std::uint32_t (*)(
                ProjectileType*, const RE::Projectile::ImpactCreation&);
            inline static std::uintptr_t original{ 0 };

            static std::uint32_t Dispatch(
                ProjectileType* projectile,
                const RE::Projectile::ImpactCreation& creation) noexcept
            {
                const auto function = reinterpret_cast<Function>(original);
                const std::uint32_t result = function ?
                    function(projectile, creation) : std::numeric_limits<std::uint32_t>::max();
                if (projectile && result != std::numeric_limits<std::uint32_t>::max()) {
                    if (DiagnosticsEnabled()) {
                        REX::INFO(
                            "[BPR-DIAG] AddImpact observed handle={:08X} result={} impacts={}",
                            Runtime::ProjectileHandleValue(*projectile), result,
                            projectile->impacts.size());
                    }
                    Runtime::ProcessProjectileImpact(
                        *projectile, Runtime::ImpactPhase::kImpactAdded);
                }
                return result;
            }

            static void Install(REL::ID vtable)
            {
                REL::Relocation<std::uintptr_t> table{ vtable };
                original = table.write_vfunc(0xE4, Dispatch);
            }
        };

        std::atomic_bool g_installed{ false };
    }

    bool InstallImpactHooks() noexcept
    {
        if (g_installed.exchange(true)) {
            return true;
        }
        try {
            ProcessImpactHook<RE::Projectile>::Install(RE::Projectile::VTABLE[0]);
            ProcessImpactHook<RE::MissileProjectile>::Install(RE::MissileProjectile::VTABLE[0]);
            ProcessImpactHook<RE::BeamProjectile>::Install(RE::BeamProjectile::VTABLE[0]);
            ProcessImpactHook<RE::ArrowProjectile>::Install(RE::ArrowProjectile::VTABLE[0]);
            AddImpactHook<RE::Projectile>::Install(RE::Projectile::VTABLE[0]);
            AddImpactHook<RE::MissileProjectile>::Install(RE::MissileProjectile::VTABLE[0]);
            AddImpactHook<RE::BeamProjectile>::Install(RE::BeamProjectile::VTABLE[0]);
            AddImpactHook<RE::ArrowProjectile>::Install(RE::ArrowProjectile::VTABLE[0]);
            REX::INFO(
                "BPR ProcessImpacts and AddImpact hooks installed for projectile, missile, beam, and arrow classes");
            return true;
        } catch (const std::exception& exception) {
            REX::ERROR("BPR impact hook installation failed: {}", exception.what());
        } catch (...) {
            REX::ERROR("BPR impact hook installation failed with an unknown error");
        }
        g_installed.store(false);
        return false;
    }
}
