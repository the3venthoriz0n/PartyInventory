#pragma once

namespace PartyInventory::Config
{
    struct Settings
    {
        std::uint32_t openKey{ 0x22 };      // DirectInput scan code, default G
        std::uint32_t toggleKey{ 0x23 };    // DirectInput scan code, default H
        bool          requireConsent{ true };
        std::uint32_t mode{ 3 };            // ContainerMenu::ContainerMode: 0 loot, 1 steal, 2 pickpocket, 3 NPC (give and take)
        std::uint32_t maxDistance{ 350 };   // game units; vanilla activation reach is a bit under this
        bool          notifications{ true };
        bool          debug{ false };
    };

    // Reads PartyInventory.ini (next to the DLL). Safe to call more than once.
    void Load();

    [[nodiscard]] const Settings& Get() noexcept;

    // Whether *other* players may open the local player's inventory. Persisted to the INI.
    [[nodiscard]] bool AllowOthers() noexcept;
    void               SetAllowOthers(bool a_allow);
}
