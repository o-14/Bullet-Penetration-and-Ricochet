#include "runtime/MCMReload.h"

#include "runtime/RuntimeConfig.h"
#include "pch.h"

#include <filesystem>

namespace BPR::Runtime
{
    namespace
    {
        constexpr std::string_view kPauseMenu{ "PauseMenu" };
        const std::filesystem::path kSettingsFile{ R"(Data\MCM\Settings\BPR.ini)" };

        struct FileStamp
        {
            std::filesystem::file_time_type writeTime{};
            std::uintmax_t size{ 0 };
            bool exists{ false };
            auto operator<=>(const FileStamp&) const = default;
        };

        FileStamp ReadStamp() noexcept
        {
            std::error_code error;
            if (!std::filesystem::is_regular_file(kSettingsFile, error) || error) {
                return {};
            }
            FileStamp stamp;
            stamp.writeTime = std::filesystem::last_write_time(kSettingsFile, error);
            if (error) {
                return {};
            }
            stamp.size = std::filesystem::file_size(kSettingsFile, error);
            if (error) {
                return {};
            }
            stamp.exists = true;
            return stamp;
        }

        class PauseMenuListener final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
        {
        public:
            RE::BSEventNotifyControl ProcessEvent(
                const RE::MenuOpenCloseEvent& event,
                RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
            {
                if (event.opening || event.menuName.empty() || event.menuName != kPauseMenu) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                const FileStamp current = ReadStamp();
                if (current != _stamp) {
                    ReloadConfiguration();
                    _stamp = ReadStamp();
                    REX::INFO("BPR applied MCM settings after the Pause menu closed");
                }
                return RE::BSEventNotifyControl::kContinue;
            }

            void Capture() noexcept { _stamp = ReadStamp(); }

            static PauseMenuListener& GetSingleton() noexcept
            {
                static PauseMenuListener singleton;
                return singleton;
            }

        private:
            FileStamp _stamp;
        };
    }

    void InstallMCMReload() noexcept
    {
        static bool installed = false;
        if (installed) {
            return;
        }
        if (RE::UI* ui = RE::UI::GetSingleton()) {
            PauseMenuListener& listener = PauseMenuListener::GetSingleton();
            listener.Capture();
            ui->GetEventSource<RE::MenuOpenCloseEvent>()->RegisterSink(&listener);
            installed = true;
            REX::INFO("BPR MCM reload listener installed");
        }
    }

    void CaptureMCMState() noexcept
    {
        PauseMenuListener::GetSingleton().Capture();
    }
}
