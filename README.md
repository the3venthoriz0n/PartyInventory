# PartyInventory

Open another player's inventory like a container, and let party members open yours, in
Skyrim Together Reborn (STR). No accept / ready / confirm rounds: look at a friend, press one key,
and a vanilla item menu opens showing their inventory. Take items, give items, close the menu.

Status: **0.1.0 prototype.** It compiles; it has not been played yet. See "Known unknowns".

## Requirements

- Skyrim Special Edition / Anniversary Edition 1.6.1170 or 1.7.104, with SKSE and Address Library
- Skyrim Together Reborn 1.8.4
- **STRPluginMessagingAPI and STRPluginMessagingBridge** (v0.10.1) on every player, and the
  `strpm-chat-relay` resource on the server
- **A patched STR server** that allows party members to change each other's inventories:
  the `player-inventory-transfers` branch of <https://github.com/the3venthoriz0n/TiltedEvolution>
  (image `ghcr.io/the3venthoriz0n/st-reborn-server:patched`), with
  `bAllowPlayerInventoryTransfers=true` under `[Gameplay]` in `STServer.ini`.
  The stock server rejects these changes, and without the patch items would vanish.
- The players must be in the **same STR party** and near each other.
- PartyInventory on every player who wants to open others or be opened.

## Install

Copy `Data\SKSE\Plugins\PartyInventory.dll` and `PartyInventory.ini` into your Skyrim `Data` folder
(or install the zip through Vortex).

## Use

| Key (default) | Does |
|---|---|
| **G** | Open the inventory of the player under your crosshair |
| **H** | Allow or forbid other players from opening *your* inventory (off by default, saved to the INI) |

1. You and your friend both press **H** once, so each of you allows the other. A HUD message confirms it.
2. Look at your friend and press **G**. Their inventory opens.
3. When someone opens yours, you get a HUD notification, and another when they close it.

Keys, the menu mode, the maximum distance and consent are configurable in `PartyInventory.ini`.

## How it works

- STR keeps a mirror of each player's inventory on their proxy actor in your game. PartyInventory finds
  out which actors are player proxies from the STR Plugin Messaging **ProxyResolver**, then opens the
  game's own `ContainerMenu` on that proxy in "NPC" mode (take and give).
- Moving an item in that menu is a normal inventory change. STR sends it to the server, and the patched
  server relays it to the owner's client, so enchantments and tempering travel with the item.
- Consent is cooperative. Each client announces "allow / do not allow" on the `partyinventory.v1`
  STRPM channel every 15 seconds, and the opener checks it before opening. The server does not enforce
  it, so treat it as a courtesy between friends, not a security boundary. Only party members in range
  can touch your inventory at all (the server enforces that).

## Known unknowns (test these first)

- Whether `ContainerMenu` opens normally on an STR player proxy, and shows the friend's real inventory.
- Whether taking and giving items in that menu reaches the other player (look for
  `Allowed player-to-player inventory change` in the server log).
- If the owner changes the same item while a friend takes it, one side may fail to apply the change.
  Avoid doing both at once until this is understood.
- Items STR cannot sync (potions you brewed yourself, whose form exists only in your save) will not
  arrive. Do not move them.

## Troubleshooting

Log: `Documents\My Games\Skyrim Special Edition\SKSE\PartyInventory.log` (set `Debug=1` for more).

- `STRPluginMessagingAPI.dll not found`: install the STR Plugin Messaging API and bridge.
- `Open refused: ... is not a mapped player proxy`: the STR messaging layer does not know that actor
  as a player yet (the relay is missing, or you are not connected).
- `has not allowed access`: your friend needs PartyInventory and must press the toggle key.
- Menu opens but items do not move: check the server log for `Rejected player-to-player inventory change`
  and read the `sameParty` / `inRange` / `allowSetting` values.

## Build

```
git clone --recurse-submodules <this repo>
build_release.bat          (needs Visual Studio 2022+/2026, CMake, and vcpkg at C:\dev\vcpkg or VCPKG_ROOT)
make_archive.bat           (creates dist\PartyInventory-<version>.zip)
```

## License

GPL-3.0-or-later (it links CommonLibSSE-NG, which is GPL-3.0-or-later). See `COPYING`.
`src/STRPluginMessagingAPI*` is from <https://github.com/Caelvanost/STRPluginMessagingAPI> (MIT).
