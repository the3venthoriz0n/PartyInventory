#pragma once

namespace PartyInventory
{
    // Shows a HUD notification. Safe to call from any thread: the message is queued to the game thread.
    void ShowHud(std::string a_message);

    // Display name of an actor, or an empty string.
    [[nodiscard]] std::string ActorName(RE::Actor* a_actor);
}
