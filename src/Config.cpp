#include "PCH.h"
#include "Config.h"

#include <Windows.h>

namespace PartyInventory::Config
{
    namespace
    {
        using FileTime = std::optional<std::filesystem::file_time_type>;

        std::mutex g_mutex;
        Settings   g_settings{};
        FileTime   g_defaultsTime;
        FileTime   g_userTime;

        std::filesystem::path DataDirectory()
        {
            // The DLL is in <Data>\SKSE\Plugins; derive <Data> from its location.
            HMODULE module = nullptr;
            GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(&DataDirectory),
                &module);

            char buffer[MAX_PATH]{};
            if (module && GetModuleFileNameA(module, buffer, MAX_PATH) != 0) {
                return std::filesystem::path(buffer).parent_path().parent_path().parent_path();
            }
            return std::filesystem::path("Data");
        }

        std::filesystem::path DefaultsPath()
        {
            return DataDirectory() / "MCM" / "Config" / "PartyInventory" / "settings.ini";
        }

        std::filesystem::path UserPath()
        {
            return DataDirectory() / "MCM" / "Settings" / "PartyInventory.ini";
        }

        FileTime StampOf(const std::filesystem::path& a_path)
        {
            std::error_code ec;
            const auto      time = std::filesystem::last_write_time(a_path, ec);
            return ec ? FileTime{} : FileTime{ time };
        }

        // GetPrivateProfile* resolves relative paths against the Windows directory, so always pass
        // absolute paths. Returns nothing if the key is not present in the file.
        std::optional<std::uint32_t> ReadValue(const std::filesystem::path& a_path, const char* a_section, const char* a_key)
        {
            char text[64]{};
            GetPrivateProfileStringA(a_section, a_key, "\x01", text, sizeof(text), a_path.string().c_str());
            if (text[0] == '\x01' || text[0] == '\0') {
                return std::nullopt;
            }
            try {
                // Decimal, or 0x-prefixed hex. A leading minus is not meaningful for any setting here.
                return static_cast<std::uint32_t>(std::stoul(text, nullptr, 0));
            } catch (...) {
                return std::nullopt;
            }
        }

        // The user's value wins over the shipped default, which wins over the built-in default.
        std::uint32_t Value(
            const std::filesystem::path& a_defaults,
            const std::filesystem::path& a_user,
            const char*                  a_section,
            const char*                  a_key,
            std::uint32_t                a_builtIn)
        {
            if (const auto user = ReadValue(a_user, a_section, a_key)) {
                return *user;
            }
            if (const auto shipped = ReadValue(a_defaults, a_section, a_key)) {
                return *shipped;
            }
            return a_builtIn;
        }

        Settings ReadAll(const std::filesystem::path& a_defaults, const std::filesystem::path& a_user)
        {
            const Settings d{};
            Settings       s{};
            s.openKey = Value(a_defaults, a_user, "Controls", "iOpenKey", d.openKey);
            s.toggleKey = Value(a_defaults, a_user, "Controls", "iToggleKey", d.toggleKey);
            s.allowOthers = Value(a_defaults, a_user, "Access", "bAllowOthers", d.allowOthers ? 1 : 0) != 0;
            s.requireConsent = Value(a_defaults, a_user, "Access", "bRequireConsent", d.requireConsent ? 1 : 0) != 0;
            s.mode = std::min<std::uint32_t>(Value(a_defaults, a_user, "Access", "iMode", d.mode), 3);
            s.maxDistance = Value(a_defaults, a_user, "Access", "iMaxDistance", d.maxDistance);
            s.notifications = Value(a_defaults, a_user, "General", "bNotifications", d.notifications ? 1 : 0) != 0;
            s.debug = Value(a_defaults, a_user, "General", "bDebug", d.debug ? 1 : 0) != 0;
            return s;
        }
    }

    void Load()
    {
        const auto defaults = DefaultsPath();
        const auto user = UserPath();
        const auto settings = ReadAll(defaults, user);

        {
            std::scoped_lock lock(g_mutex);
            g_settings = settings;
            g_defaultsTime = StampOf(defaults);
            g_userTime = StampOf(user);
        }

        spdlog::set_level(settings.debug ? spdlog::level::debug : spdlog::level::info);
        spdlog::info(
            "Settings loaded (defaults: {} [{}], user: {} [{}]): openKey=0x{:02X} toggleKey=0x{:02X} allowOthers={} requireConsent={} mode={} maxDistance={}",
            defaults.string(),
            StampOf(defaults) ? "found" : "missing",
            user.string(),
            StampOf(user) ? "found" : "missing",
            settings.openKey,
            settings.toggleKey,
            settings.allowOthers,
            settings.requireConsent,
            settings.mode,
            settings.maxDistance);
    }

    bool ReloadIfChanged()
    {
        const auto defaults = DefaultsPath();
        const auto user = UserPath();
        const auto defaultsTime = StampOf(defaults);
        const auto userTime = StampOf(user);

        {
            std::scoped_lock lock(g_mutex);
            if (defaultsTime == g_defaultsTime && userTime == g_userTime) {
                return false;
            }
        }

        const auto settings = ReadAll(defaults, user);

        bool changed = false;
        {
            std::scoped_lock lock(g_mutex);
            g_defaultsTime = defaultsTime;
            g_userTime = userTime;
            changed = !(settings == g_settings);
            g_settings = settings;
        }

        if (changed) {
            spdlog::set_level(settings.debug ? spdlog::level::debug : spdlog::level::info);
            spdlog::info(
                "Settings changed: openKey=0x{:02X} toggleKey=0x{:02X} allowOthers={} requireConsent={} mode={} maxDistance={}",
                settings.openKey,
                settings.toggleKey,
                settings.allowOthers,
                settings.requireConsent,
                settings.mode,
                settings.maxDistance);
        }
        return changed;
    }

    Settings Get()
    {
        std::scoped_lock lock(g_mutex);
        return g_settings;
    }

    bool AllowOthers()
    {
        std::scoped_lock lock(g_mutex);
        return g_settings.allowOthers;
    }

    void SetAllowOthers(bool a_allow)
    {
        const auto user = UserPath();

        std::error_code ec;
        std::filesystem::create_directories(user.parent_path(), ec);
        if (!WritePrivateProfileStringA("Access", "bAllowOthers", a_allow ? "1" : "0", user.string().c_str())) {
            spdlog::error("Could not write {}", user.string());
        }

        // Apply immediately; the next ReloadIfChanged() will see the new file time and reread harmlessly.
        std::scoped_lock lock(g_mutex);
        g_settings.allowOthers = a_allow;
    }
}
