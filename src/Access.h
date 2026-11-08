#pragma once

// Opening another player's inventory like a container, and letting others open yours.
namespace PartyInventory::Access
{
    // Registers the key and menu listeners. Call once, after the game data has loaded.
    void Install();
}
