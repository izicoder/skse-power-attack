# SKSE Power Attack

Click mouse button 5 (or any key you bind) once and you power attack. **One DLL, nothing else** — no `.esp`,
no `.esl`, no Papyrus, no MCM quest: a pure SKSE plugin that submits the vanilla `PowerAttackRight` action
when you click.

Inspired by [One Click Power Attack](https://www.nexusmods.com/skyrimspecialedition/mods/60878) by jarari,
reduced to the one thing that mod's name promises.

> **AI-generated.** Every line of this plugin was written by an AI. No human authored or thoroughly reviewed
> it, so treat it as a hobby project: expect rough edges, and check it yourself before trusting it in your
> load order.

## What it does

- **One click = one power attack.** Every press of the configured key submits the vanilla right-hand power
  attack action, so combos, stamina costs, perks and animations all behave exactly like a real power attack.
- **Jump / block fallback.** While jumping or blocking the power attack action is eaten by the engine, so in
  those two cases the plugin performs a normal right attack first and the power attack 200 ms later.
- **Left alone:** bows, crossbows, staves, horses, killmoves, sitting/sleeping, paused game, dialogue.
- **Empty stamina** is allowed by default (for mods that let you power attack at 0 stamina); one checkbox
  turns that off.
- **Optional modifier:** bind e.g. Shift so only Shift+click power attacks.
- **Settings tab** in [SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352) with
  key rebinds, the stamina toggle and a logging switch. Every change saves itself.
- Works without SKSE Menu Framework — the JSON keeps working on its own.

## Settings

`SKSE/Plugins/SKSEPowerAttack.json`, next to the DLL (inside the mod folder under MO2), written with
defaults on first run:

| Key | Default | Meaning |
|---|---|---|
| `hotkey` | `260` | key that triggers the power attack (mouse button 5) |
| `modifierKey` | `-1` | optional modifier; `-1` = none |
| `allowZeroStamina` | `true` | allow the click with an empty stamina bar |
| `logging` | `true` | write `SKSEPowerAttack.log` |
| `version` | `1` | format of the file; written by the plugin, do not edit |

The file carries a `version` and is migrated in place on launch: a key that changes name, type or meaning
gets exactly one migration step, so a file from an older release is converted and rewritten once instead of
being reset. Options this build does not recognise are preserved rather than dropped (a file from a newer
build survives a downgrade), a bad value costs only that key, and no config is shipped in the archive, so an
update never overwrites a tuned file.

Upgrading from this plugin's pre-rename build? Its `OneClickPowerAttack.json` sits next to the DLL and is
imported once on first run, then left untouched.

Key codes use SKSE's `InputMap` numbering (the same values the original mod's `settings.ini` used):

- `0`–`255` — keyboard scancode (e.g. `42` = left shift, `29` = left ctrl)
- `256`–`263` — mouse button (`256` left, `257` right, `258` middle, …)
- `266`–`281` — gamepad (dpad, start/back, sticks, shoulders, A/B/X/Y, LT/RT)

The default used to be the right mouse button (`257`); the file defaults to mouse 5 now. An existing
`SKSEPowerAttack.json` keeps whatever it already contains, so change `hotkey` by hand or in the menu.

## Requirements

- Skyrim SE/AE
- [SKSE](https://www.nexusmods.com/skyrimspecialedition/mods/30379)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
- Optional: [SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352)

Built against CommonLibSSE-NG v13.0.0 (Address Library) and developed/tested on 1.6.1170 only.

## Install

Put `SKSEPowerAttack.dll` in `…/mods/<your mod>/SKSE/Plugins/` and enable the mod. **Disable any other
one-click power attack plugin** (e.g. One Click Power Attack NG) when enabling this one — both listen to the
same click and would submit the power attack twice.

## Building

Requires VS 2022/18 with the C++ workload, CMake + Ninja and a vcpkg checkout. Build from a VS developer shell
(`vcvars64.bat`), otherwise the Windows SDK is not found:

```powershell
$env:VCPKG_ROOT = "E:/dev/vibe/skyrim-blink/.vcpkg"   # any vcpkg checkout
cmake --preset vs2022-local
cmake --build build/vs2022
```

The build only produces `build/vs2022/SKSEPowerAttack.dll` — it never touches your mod list. Deploying is a
deliberate step, and it refuses to run while Skyrim is open:

```powershell
.\tools\Deploy.ps1                          # -> C:\Modding\MO2\mods\SKSE Power Attack
.\tools\Deploy.ps1 -Path "D:\mods\My Test"  # any mod folder
```

`CMakeUserPresets.json` (git-ignored) points `COMMONLIB_PREBUILT_DIR` at the prebuilt CommonLibSSE bundle
already extracted by the `skyrim-blink` project, so nothing is rebuilt from source. Drop it to let CMake
download the bundle instead.

## Packaging a test install

```powershell
.\tools\Build-Release.ps1 -Version 0.1.0
```

Stamps `0.1.0` into the DLL and writes `packages/SKSEPowerAttack-0.1.0.zip` containing
`SKSE/Plugins/SKSEPowerAttack.dll` (+ `docs/README.md`). Install it in MO2 with **Install a new mod from an
archive** (the disc icon / Ctrl+M), then enable it — that is also the honest test of what a user gets, since
no config file is shipped and the plugin has to create one on first run.

`-SkipBuild` re-zips whatever is already built; `-FromFolder "C:\Modding\MO2\mods\SKSE Power Attack" -SkipBuild`
archives exactly what is installed in that mod folder (the honest "what the game runs" build);
`-Output <path>` writes the archive somewhere else. No config file is ever shipped.

## Logging

`…\Documents\My Games\Skyrim Special Edition\SKSE\SKSEPowerAttack.log`, trace level, flushed every line.
Untick **Write SKSEPowerAttack.log** to silence it.

## Credits

- jarari for the original One Click Power Attack (the action handling and its caveats come from it).
- Ryan-rsm-McKenzie / CharmedBaryon for CommonLibSSE / CommonLibSSE-NG, meh321 for the Address Library.
