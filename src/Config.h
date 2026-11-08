#pragma once

// Settings live in MCM Helper's files, so they can be edited in the SkyUI Mod Configuration Menu:
//   defaults  Data\MCM\Config\PartyInventory\settings.ini   (shipped with the mod)
//   user      Data\MCM\Settings\PartyInventory.ini          (written by the MCM; overrides the defaults)
// Without MCM Helper the same files can be edited by hand. Changes are picked up within a second.
namespace PartyInventory::Config
{
    struct Settings
    {
        std::uint32_t openKey{ 0x15 };      // DirectInput scan code, default Y
        std::uint32_t toggleKey{ 0x23 };    // DirectInput scan code, default H
        bool          allowOthers{ false }; // may other players open MY inventory
        bool          requireConsent{ true };
        std::uint32_t mode{ 3 };            // ContainerMenu::ContainerMode: 0 loot, 1 steal, 2 pickpocket, 3 NPC (give and take)
        std::uint32_t maxDistance{ 350 };   // game units; vanilla activation reach is a bit under this
        bool          notifications{ true };
        bool          debug{ false };

        bool operator==(const Settings&) const = default;
    };

    // Reads the settings files. Safe to call more than once.
    void Load();

    // Re-reads the files if either changed on disk. Returns true if any setting value changed.
    // Cheap enough to call about once a second from any thread.
    bool ReloadIfChanged();

    // A copy of the current settings (thread-safe).
    [[nodiscard]] Settings Get();

    [[nodiscard]] bool AllowOthers();

    // Used by the toggle key. Saved to the user settings file, so the MCM shows the same state.
    void SetAllowOthers(bool a_allow);
}
