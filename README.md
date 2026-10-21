# SKSE Power Attack

Tap mouse button 5 (or any key or chord you bind) and you power attack. **One DLL, nothing else** — no `.esp`,
no `.esl`, no Papyrus, no MCM quest: a pure SKSE plugin that submits the vanilla power attack action of the row
you bound.

> **AI-generated.** Every line of this plugin was written by an AI. No human authored or thoroughly reviewed
> it, so treat it as a hobby project: expect rough edges, and check it yourself before trusting it in your
> load order.

## The rules

1. **Three keys, three attacks.** Right hand, left hand, dual wield. A key fires only the row it is bound to: there
   is no automatic weapon detection, so what comes out never depends on what you happen to hold.
2. **Tap, not hold.** The attack fires when the key is *released*, and only if the press was shorter than the tap
   window (200 ms by default). Holding a key you bound does nothing, which is why it is safe next to sprint or
   jump.
3. **Modifiers.** Hold Shift, Ctrl or Alt (either side of the keyboard) *first*, then press the key: both are
   stored in that row, e.g. `Shift + Mouse 5`. Extra modifiers you did not name are ignored, so sprinting with Alt
   held never blocks a plain `Mouse 5`.
4. **Two rows, one chord.** A chord that names modifiers beats a bare chord on the same key, and two rows with
   the exact same chord resolve to the one higher in the menu. Nothing is stolen when you rebind.
5. **The game's own attack key is taken over.** If a row uses the key the game attacks with (left mouse by
   default), the plugin keeps that press away from the game, so a tap gives *only* the power attack instead of a
   normal swing plus a power attack. Let the same key go after the tap window and the press is handed back as the
   normal attack — holding the button still swings.
6. **Any other key is left alone.** A key the game does not attack with (mouse 4/5, an unused letter) keeps
   whatever it did before; there is nothing to cancel.
7. **Jumping or blocking.** The engine eats the power attack action in those two states, so the plugin performs
   the normal attack of the same row first and the power attack 200 ms later.
8. **Never a power attack:** bows, crossbows and staves equipped; on a horse; in a killmove; sitting or
   sleeping; paused game; dialogue. The press is not taken in those cases either, so the bow still fires.
9. **Empty stamina** is allowed by default (for mods that let you power attack at 0 stamina); one checkbox turns
   that off.
10. **No key in a row = that row does nothing.** Every row can be cleared, including the right hand.

## Features

- Chords: hold Shift, Ctrl or Alt while pressing a key to bind `Shift + Mouse 5`, `Ctrl + E`, whatever.
- Three power attack rows: right hand, left hand, dual wield. Bind one, or all three.
- Every tap submits the matching vanilla action (the game has a full set: `0x13383` right, `0x2E2F6` left,
  `0x2E2F7` dual), so combos, stamina costs, perks and animations all behave exactly like a real power attack.
- **Settings tab** in [SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352) with
  the three rebinds, the tap window, the stamina toggle and a logging switch. Every change saves itself.
- Works without SKSE Menu Framework — the JSON keeps working on its own.

## Settings

`SKSE/Plugins/SKSEPowerAttack.json`, next to the DLL (inside the mod folder under MO2), written with defaults
on first run:

```json
{
	"binds": {
		"right": { "key": 260, "mods": 0 },
		"left":  { "key": -1,  "mods": 0 },
		"dual":  { "key": -1,  "mods": 0 }
	},
	"tapWindowMs": 200,
	"allowZeroStamina": true,
	"logging": true,
	"version": 3
}
```

| Key | Default | Meaning |
|---|---|---|
| `binds.right.key` | `260` | key for the right-hand power attack (mouse button 5); `-1` = unbound |
| `binds.right.mods` | `0` | modifiers for it: `1` shift, `2` ctrl, `4` alt, add them together |
| `binds.left` / `binds.dual` | `-1` | the other two rows, same shape; both unbound by default |
| `tapWindowMs` | `200` | press *and* release inside this many ms, otherwise nothing happens (20–1000) |
| `allowZeroStamina` | `true` | allow the tap with an empty stamina bar |
| `logging` | `true` | write `SKSEPowerAttack.log` |
| `version` | `3` | format of the file; written by the plugin, do not edit |

Key codes use SKSE's `InputMap` numbering:

- `0`–`255` — keyboard scancode (e.g. `42` = left shift, `29` = left ctrl)
- `256`–`263` — mouse button (`256` left, `257` right, `258` middle, …)
- `266`–`281` — gamepad (dpad, start/back, sticks, shoulders, A/B/X/Y, LT/RT)

A modifier key cannot be a chord's key (it is what you hold), so `42` in a `key` field is repaired.

The file carries a `version` and is migrated in place on launch: a key that changes name, type or meaning gets
exactly one migration step, so a file from an older release is converted and rewritten once instead of being
reset. What the steps do:

- Format 1 → 2 (`hotkey`, `leftHotkey`, `modifierKey`): the old `hotkey` and its modifier become one chord, the
  old `leftHotkey` becomes the left row.
- Format 2 → 3: the old `binds.auto` chord moves to `binds.right` when that row was free, otherwise it is
  dropped and the log says so. The right row is what auto was for most people anyway; the game now swings
  exactly the hand the row names instead of detecting it from your gear.
- The old second key is a plain left power attack now — it used to give the dual attack while dual-wielding.

Options this build does not recognise are preserved rather than dropped (a file from a newer build survives a
downgrade), a bad value costs only that row, and no config is shipped in the archive, so an update never
overwrites a tuned file.

A key can sit in two rows; the one higher in the list wins. Nothing is stolen when you rebind.

Upgrading from this plugin's pre-rename build? Its `OneClickPowerAttack.json` sits next to the DLL and is
imported once on first run, then left untouched.

## Compatibility

- **Nothing in the load order.** No `.esp`/`.esl`, no records, no Papyrus, no MCM quest and no edits to game
  files, so the only possible conflict is another plugin that submits a power attack on the same key.
- **No hooks.** The plugin never patches game code, never rewrites a vtable and never edits a record: it reads
  input events, looks up two vanilla actions and calls the engine's own `ProcessAction` through the task
  interface.
- **Input is passed on, except the attack key you bind.** The sink reports "handled" only for the press and
  release of a key the game itself attacks with, and only while a row is bound to it — that is the cancel in
  rule 5 above. Every other event reaches the game unchanged, so the game and every other mod see exactly what
  they would without it.
- **No threads.** The 200 ms jump/block fallback is a chain of main-thread tasks, not a worker thread.
- **Menu optional.** With SKSE Menu Framework installed there is a settings tab; without it the menu is skipped
  and the JSON still applies.
- **Animation mods** (MCO/ADXP, Valhalla, Vanargand, ...) work because the vanilla action is submitted instead of
  forcing an animation event.
- **Gamepad** buttons bind like keys and mouse buttons.
- **No VR support** — the binary resolves addresses through the Address Library for SE/AE.

## Requirements

- Skyrim SE/AE
- [SKSE](https://www.nexusmods.com/skyrimspecialedition/mods/30379)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
- Optional: [SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352)

Built against CommonLibSSE-NG v13.0.0 (Address Library) and developed/tested on 1.6.1170 only. The DLL imports
the same MSVC runtime as SKSE64 itself (VCRUNTIME140 / MSVCP140), so nothing else has to be installed.

## Install

Put `SKSEPowerAttack.dll` in `…/mods/<your mod>/SKSE/Plugins/` and enable the mod. **Disable any other
one-click power attack plugin** when enabling this one — both listen to the same click and would submit the
power attack twice.

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

The pure logic (chords, row resolution, the action table) is unit tested without the game:

```powershell
ctest --test-dir build/vs2022 --output-on-failure
```

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

## Publishing a release

Only when asked, and never as part of a build:

```powershell
.\tools\Publish-Release.ps1 -Version 0.2.1 -Nexus -GitHub -DryRun   # validate, upload nothing
.\tools\Publish-Release.ps1 -Version 0.2.1 -Nexus -GitHub
```

The Nexus key comes from `$env:NEXUS_API_KEY` or `%USERPROFILE%\.pi\nexus-api-key.txt`; GitHub uses `gh`.
Both paths refuse a stale archive: the DLL inside the zip must report the `-Version` being published (it is
stamped by `Build-Release.ps1`), and a version number the Nexus file entry already has is refused unless
`-Force` is given. Release notes default to the commit subjects since the last release tag; `-Notes "..."` overrides.

The Nexus side only adds **versions to an existing file entry** — the API has no call to create one, so the
first archive for a new mod is uploaded on the mod page by hand.

## Logging

`…\Documents\My Games\Skyrim Special Edition\SKSE\SKSEPowerAttack.log`, trace level, flushed every line.
Untick **Write SKSEPowerAttack.log** to silence it. A tap logs which row fired, the chord, how long the key was
held and the action it submitted; a cancelled press says so on the same line.

## Credits

- Ryan-rsm-McKenzie / CharmedBaryon for CommonLibSSE / CommonLibSSE-NG, meh321 for the Address Library.
