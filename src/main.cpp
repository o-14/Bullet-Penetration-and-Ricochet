#include "engine/ImpactHooks.h"
#include "engine/ProjectileVisuals.h"
#include "runtime/ContinuationState.h"
#include "runtime/MCMReload.h"
#include "runtime/RuntimeConfig.h"

namespace Main
{
    namespace
    {
        bool g_initialized = false;

        bool SupportedRuntime(REL::Version version) noexcept
        {
            return version == F4SE::RUNTIME_1_10_163 ||
                version == F4SE::RUNTIME_1_11_191 ||
                version == F4SE::RUNTIME_1_11_221;
        }

        void ResetState()
        {
            BPR::Runtime::ReloadConfiguration();
            BPR::Runtime::CaptureMCMState();
            BPR::Runtime::ClearAllChains();
        }

        void OnMessage(F4SE::MessagingInterface::Message* message)
        {
            if (!message) {
                return;
            }
            switch (message->type) {
            case F4SE::MessagingInterface::kGameDataReady:
                ResetState();
                BPR::Runtime::InstallMCMReload();
                (void)BPR::Engine::InstallProjectileVisualHooks();
                (void)BPR::Engine::InstallImpactHooks();
                break;
            case F4SE::MessagingInterface::kGameLoaded:
            case F4SE::MessagingInterface::kPostLoadGame:
            case F4SE::MessagingInterface::kNewGame:
                ResetState();
                break;
            default:
                break;
            }
        }
    }

    bool InitPlugin(const F4SE::LoadInterface* f4se)
    {
        if (g_initialized) {
            return true;
        }
        static std::once_flag once;
        std::call_once(once, [&]() {
            F4SE::Init(f4se);
            const F4SE::MessagingInterface* messaging = F4SE::GetMessagingInterface();
            if (!messaging || !messaging->RegisterListener(OnMessage)) {
                REX::ERROR("BPR could not register the F4SE message listener");
                return;
            }
            REX::INFO("BPR 3.0.0 initialized for Fallout 4 {}", f4se->RuntimeVersion().string());
            g_initialized = true;
        });
        return g_initialized;
    }

    F4SE_PLUGIN_QUERY(const F4SE::QueryInterface* f4se, F4SE::PluginInfo* info)
    {
        if (!f4se || !info || f4se->IsEditor()) {
            return false;
        }
        // Legacy F4SE 0.6.x decides whether to load the DLL from this structure.
        // Populate it directly: the modern version export is not guaranteed to
        // be discoverable through CommonLib's singleton during the query phase.
        info->infoVersion = F4SE::PluginInfo::kVersion;
        info->name = "BPR";
        info->version = REL::Version{ 3, 0, 0, 0 }.pack();
        return SupportedRuntime(f4se->RuntimeVersion());
    }

    F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* f4se)
    {
        return f4se && InitPlugin(f4se);
    }

    F4SE_PLUGIN_PRELOAD(const F4SE::LoadInterface* f4se)
    {
        return f4se && InitPlugin(f4se);
    }
}
