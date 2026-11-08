#pragma once

#include "STRPluginMessagingAPI/STRPluginMessagingAPI.h"

// Link to Skyrim Together Reborn through the STR Plugin Messaging API (STRPM).
//
// Two jobs:
//  * ProxyResolver: tell which in-game actors are remote players' proxies, and which connection each
//    belongs to.
//  * A tiny text protocol on the "partyinventory.v1" channel:
//      PIv1|ACCESS|<0|1>   broadcast: "others may (not) open my inventory". Repeated every few seconds.
//      PIv1|OPEN|<0|1>     sent to the owner: "someone opened / closed your inventory".
namespace PartyInventory::StrLink
{
    // Loads STRPluginMessagingAPI.dll, registers the channel and proxy listener, starts the announcer.
    [[nodiscard]] bool Start();
    void               Stop();
    [[nodiscard]] bool IsRunning() noexcept;

    // If the actor is a remote player's proxy, returns that player's connection ID.
    [[nodiscard]] std::optional<STRPM::ConnectionID> ResolveProxy(RE::FormID a_actorFormID);

    // True if this peer announced (recently) that others may open their inventory.
    [[nodiscard]] bool PeerAllowsAccess(STRPM::ConnectionID a_connection);

    // Announce the local "allow others" state to everyone now.
    void BroadcastAccessState();

    // Tell the owner that their inventory was opened or closed.
    void SendOpenState(STRPM::ConnectionID a_connection, bool a_open);
}
