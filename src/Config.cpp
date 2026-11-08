#include "PCH.h"
#include "Config.h"

#include <Windows.h>

namespace PartyInventory::Config
{
    namespace
    {
        Settings          g_settings{};
        std::atomic<bool> g_allowOthers{ false };

        std::string IniPath()
        {
            // GetPrivateProfile* resolves relative paths against the Windows directory, so always
            // use an absolute path next to the plugin DLL.
            HMODULE module = nullptr;
            GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(&IniPath),
                &module);

            char buffer[MAX_PATH]{};
            if (!module || GetModuleFileNameA(module, buffer, MAX_PATH) == 0) {
                return "Data\\SKSE\\Plugins\\PartyInventory.ini";
            }

            std::filesystem::path path(buffer);
            path.replace_extension(".ini");
            return path.string();
        }

        std::uint32_t ReadUInt(const std::string& a_path, const char* a_section, const char* a_key, std::uint32_t a_default)
        {
            // Accept decimal or 0x-prefixed hex.
            char text[64]{};
            char fallback[32]{};
            std::snprintf(fallback, sizeof(fallback), "%u", a_default);
            GetPrivateProfileStringA(a_section, a_key, fallback, text, sizeof(text), a_path.c_str());
            try {
                return static_cast<std::uint32_t>(std::stoul(text, nullptr, 0));
            } catch (...) {
                return a_default;
            }
        }
    }

    void Load()
    {
        const auto path = IniPath();
        Settings   s{};

        s.openKey = ReadUInt(path, "Controls", "OpenKey", s.openKey);
        s.toggleKey = ReadUInt(path, "Controls", "ToggleAccessKey", s.toggleKey);
        s.requireConsent = ReadUInt(path, "Access", "RequireConsent", s.requireConsent ? 1 : 0) != 0;
        s.mode = std::min<std::uint32_t>(ReadUInt(path, "Access", "Mode", s.mode), 3);
        s.maxDistance = ReadUInt(path, "Access", "MaxDistance", s.maxDistance);
        s.notifications = ReadUInt(path, "General", "Notifications", s.notifications ? 1 : 0) != 0;
        s.debug = ReadUInt(path, "General", "Debug", s.debug ? 1 : 0) != 0;

        g_settings = s;
        g_allowOthers.store(ReadUInt(path, "Access", "AllowOthers", 0) != 0);

        spdlog::info(
            "Config loaded from {}: openKey=0x{:02X} toggleKey=0x{:02X} requireConsent={} mode={} maxDistance={} allowOthers={}",
            path,
            s.openKey,
            s.toggleKey,
            s.requireConsent,
            s.mode,
            s.maxDistance,
            g_allowOthers.load());
    }

    const Settings& Get() noexcept
    {
        return g_settings;
    }

    bool AllowOthers() noexcept
    {
        return g_allowOthers.load();
    }

    void SetAllowOthers(bool a_allow)
    {
        g_allowOthers.store(a_allow);
        WritePrivateProfileStringA("Access", "AllowOthers", a_allow ? "1" : "0", IniPath().c_str());
    }
}
