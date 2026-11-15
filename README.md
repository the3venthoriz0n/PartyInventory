# PartyInventory

Open another player's inventory like a container, and let party members open yours, in
Skyrim Together Reborn (STR). No accept / ready / confirm rounds: look at a friend, press one key,
and a vanilla item menu opens showing their inventory. Take items, give items, close the menu.

Status: **0.2.1 prototype.** It compiles; it has not been played yet. See "Known unknowns".

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

Install the zip through Vortex, or copy its `Data` folder into your Skyrim `Data` folder. It adds
`SKSE\Plugins\PartyInventory.dll` and the settings files under `MCM\Config\PartyInventory\`.

## Use

| Key (default) | Does |
|---|---|
| **Y** | Open the inventory of the player under your crosshair |
| **H** | Allow or forbid other players from opening *your* inventory (off by default, saved to the INI) |

1. You and your friend both press **H** once, so each of you allows the other. A HUD message confirms it.
2. Look at your friend and press **G**. Their inventory opens.
3. When someone opens yours, you get a HUD notification, and another when they close it.

## Settings

Everything is editable in the **SkyUI Mod Configuration Menu** under "Party Inventory": both keys, whether
others may open your inventory, whether you need their consent, the menu mode, the maximum distance,
notifications and debug logging. Changes apply within about a second, with no restart.

This uses **MCM Helper**, which has to be installed for the menu to appear. The mod ships a small
`PartyInventory.esp` (light plugin, one quest, no gameplay) that MCM Helper needs to list the page; enable it in your
plugin list. If you do not use SkyUI or
MCM Helper, edit the files by hand instead (also applied within a second):

- defaults, shipped with the mod: `Data\MCM\Config\PartyInventory\settings.ini`
- your overrides, written by the menu or by the **H** key: `Data\MCM\Settings\PartyInventory.ini`
  (a value here wins over the default; create the file if you want to set something yourself)

Keys are DirectInput scan codes: 21 = Y, 35 = H.

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
- The "Party Inventory" page is missing from the MCM: MCM Helper is not installed, `PartyInventory.esp` is
  not enabled, or it did not pick up `Data\MCM\Config\PartyInventory\config.json`. Edit the settings files by hand meanwhile.
- Menu opens but items do not move: check the server log for `Rejected player-to-player inventory change`
  and read the `sameParty` / `inRange` / `allowSetting` values.

## Build

```
git clone --recurse-submodules <this repo>
build_release.bat          (needs Visual Studio 2022+/2026, CMake, and vcpkg at C:\dev\vcpkg or VCPKG_ROOT)
make_archive.bat           (creates dist\PartyInventory-<version>.zip)
```

Packaging never overwrites or deletes an earlier build: if the zip name is already in `dist`, the new one gets a `_yyyyMMdd-HHmmss` suffix (for example `PartyInventory-0.2.1_20261114-160500.zip`), so versions sit side by side for rollback; the script prints the final path.

## Releasing

**To release, tag `main`.** The tag is the version, you never edit version numbers by hand for a release.

1. **Make sure `main` is what you want to ship** (the tag may only be pushed on a commit that is already on `main`, or the run stops before building anything).
2. **Pick the next version** (`vMAJOR.MINOR.PATCH`): patch for fixes, minor for new features.
3. **Tag and push:**

        git checkout main && git pull
        git tag v0.2.2
        git push origin v0.2.2

4. **Watch it:** `gh run watch` (or the Actions tab). The first run on a fresh cache takes a while; later runs reuse the cached build dependencies.
5. **Check the result:** the run is green and the GitHub release has:

- `PartyInventory_<version>.zip`

**What the workflow does** (`.github/workflows/release.yml`, run on a Windows runner): checks the tag looks like `v1.2.3` and its commit is on `main`; stamps the version into `CMakeLists.txt`, `vcpkg.json`, `src/main.cpp` and `tools/make_archive.ps1` (it stops if any of them does not match exactly once); builds; packages with the repo's script; uploads the zip(s) as a build artifact and creates the GitHub release. No Nexus upload and no version-bump commit are done: this repo has no Nexus page, so the numbers in the repo files stay as they are and only the tagged build carries the new version.

**Test builds, one tag per change.** A tag with a suffix, such as `v0.2.2-test1`, builds the same way and creates a GitHub *pre-release* with the zip named `PartyInventory_0.2.2-test1.zip`. Use one for every change you want kept as a build, and a plain `vX.Y.Z` tag for what you publish. The numeric part of the tag is what gets stamped into the build.

**Dry run, no release:** Actions tab, "Release", "Run workflow" on `main`. It builds and packages and uploads the zip as a build artifact; it does not create a release.

**If something goes wrong.** *"Tag ... is not on main" or "must look like v1.2.3":* delete the tag (`git push origin :refs/tags/vX.Y.Z` and `git tag -d vX.Y.Z`), fix, tag again. *The build fails on the runner:* use the dry run after each fix instead of pushing more tags. *A zip you package locally has the old version:* normal; only the workflow stamps the tag version.

## License

PartyInventory is free software under the **GNU General Public License, version 3 or (at your option) any later version (GPL-3.0-or-later)**. It statically links [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG), which is GPL-3.0-or-later (with a Modding Exception for Skyrim and SKSE), so the whole plugin is distributed under the GPL. The complete, unmodified licence text is in [`COPYING`](COPYING). Copyright the3venthoriz0n and the contributors named in [CREDITS.md](CREDITS.md). There is NO WARRANTY, to the extent permitted by law.

The licences of the libraries built in are reproduced in [`THIRD_PARTY_NOTICES.txt`](THIRD_PARTY_NOTICES.txt) (generated by `python -I tools/make_notices.py`).

`src/STRPluginMessagingAPI*` is from <https://github.com/Caelvanost/STRPluginMessagingAPI> (MIT per its header comments and relay manifest).

## Credits

Everybody whose work this mod uses, links or requires (CommonLibSSE-NG and its authors, SKSE, Address Library, SkyUI, MCM Helper, the libraries, Bethesda Game Studios, and the AI assistance) is listed in [CREDITS.md](CREDITS.md). It is also in the download as `Data/Docs/PartyInventory/CREDITS.txt`.

## Source code

- GitHub: https://github.com/the3venthoriz0n/PartyInventory (clone with `git clone --recurse-submodules`).
- Every GitHub release has a source archive attached next to the mod zip: `PartyInventory_<version>_source.zip`. It holds the repository at the release commit plus the pinned sources of the submodules (CommonLibSSE-NG). To make one yourself: `powershell -File tools\make_source_archive.ps1`.
- The mod download itself contains `Data/Docs/PartyInventory/` with `COPYING.txt` (the GPL), `CREDITS.txt`, `THIRD_PARTY_NOTICES.txt` and `SOURCE.txt` (where the source is).
- The build also downloads the vcpkg libraries listed in `vcpkg.json` (pinned by `vcpkg-configuration.json`) and the MinHook v1.3.4 decoder (CMake FetchContent).

