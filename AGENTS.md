# AGENTS.md

## What this is

**SKSE Power Attack** — a minimal SKSE plugin (C++23, CommonLibSSE-NG, Address Library) that turns a tap of
one bound chord into a vanilla power attack. One DLL: no plugin file, no Papyrus, no MCM. Optional settings
tab through SKSE Menu Framework.

A minimal plugin by design: one tap, one power attack. Do not add extras (attack-window notify/queue,
long-press modes, block unbinding) unless asked; "simple" is the point.

Keep the surface small, because that is what "compatible with everything" means here: no hooks, no vtable or
record edits, no threads (use the task interface and a re-posting task for anything that has to wait, see
`Attack::ScheduleDelayedPowerAttack`), and no config file shipped. Input is consumed only for what a chord owns:
a press matching a chord that names a modifier, and the game's own attack key while a row owns it. Everything
else is passed on untouched.

## Layout

| Path | What it does |
|---|---|
| `src/Plugin.cpp` | `SKSEPlugin_Load` + messaging: config load, action init, input/UI registration. |
| `src/Config.h/.cpp` | `Settings` (`binds` + 3 fields), JSON load/save next to the DLL, format version + migrations, validation, revision counter. |
| `src/Chord.h` | What a bind is, engine-free: a key plus a Shift/Ctrl/Alt mask, the scan codes of the modifiers, matching and the label prefix. Unit tested. |
| `src/Binds.h` | The three rows in menu order (`kRight`, `kLeft`, `kDual`), engine-free, and `Resolve()`: which row a press belongs to. Unit tested. |
| `src/Selection.h` | The tap decision, engine-free: `ForSlot()` maps a row to its vanilla action and nothing else. Unit tested. |
| `src/Attack.h/.cpp` | Resolves all six vanilla attack/power-attack actions, submits them through the task interface, the jumping fallback as a task chain (no threads), nothing at all while blocking. `PowerAttack` and the `NormalAttack` hand-back. |
| `src/Input.h/.cpp` | Key encoding (SKSE InputMap numbering), labels including `Keys::DescribeChord`, the tap sink (modifiers over the whole batch first, then three tap slots, press + release inside the window), its gates, the cancel, the menu-open sink. |
| `tests/` | `ChordTests.cpp`, `BindsTests.cpp`, `SelectionTests.cpp`: plain `main()`, no framework, no engine. |
| `src/UI.h/.cpp` | SKSE Menu Framework tab: three chord rows with chord capture, toggles, two-click reset. |
| `src/Log.h` | `PA`/`PAI`/`PAW`/`PAE`, trace level, flushed per line. |
| `extern/SKSEMenuFramework.h` | Vendored framework header (do not edit). |
| `ROADMAP.md` | Current state, the Nexus comment requests and their verdicts, next steps, deliberate non-goals. |
| `tools/Deploy.ps1` | Manual deploy of the built DLL into one mod folder (refuses while Skyrim runs). |
| `tools/Build-Release.ps1` | Version-stamped build + `packages/SKSEPowerAttack-<version>.zip` for test installs. |
| `tools/Publish-Release.ps1` | Uploads that zip to Nexus (v3 API) and/or a GitHub release — only when named on the command line. |

## Build

From a VS developer shell (`vcvars64.bat`):

```powershell
$env:VCPKG_ROOT = "E:/dev/vibe/skyrim-blink/.vcpkg"
cmake --preset vs2022-local
cmake --build build/vs2022
```

`extern/CommonLibSSE` is a source checkout of the v13.0.0 tag, consumed with `add_subdirectory`; the prebuilt
bundle is reused via `COMMONLIB_PREBUILT_DIR` in the local preset. Do not switch the triplet off
`x64-windows-static-md`, build Debug, or drop `directxtk`/`rapidcsv` from `vcpkg.json` — the library's own
`find_package` needs them.

Building never deploys. `tools/Deploy.ps1` copies the DLL into a mod folder and is the only thing that does;
it refuses to run while `SkyrimSE` is open. Do not turn on `MOD_OUTPUT_FOLDER` in the build: a post-build copy
would silently replace a DLL the running game has loaded. The default deploy target is
`C:\Modding\MO2\mods\SKSE Power Attack` (the old `One Click Power Attack (SKSE)` folder is gone).

## Verifying a change

A change counts as verified when:

1. `ctest --test-dir build/vs2022 --output-on-failure` passes (`ChordTests`, `BindsTests`, `SelectionTests`).
   They build in seconds without the engine, so write the case there before touching `Chord.h`, `Binds.h` or
   `Selection.h`.
2. `cmake --build build/vs2022` (or `Build-Release.ps1` when the version matters) ends with
   `Linking CXX shared library SKSEPowerAttack.dll` and no error lines.
3. `.\tools\Deploy.ps1` puts it in the MO2 folder (refuses while `SkyrimSE` runs), the mod is enabled, Skyrim is
   launched.
4. `…\Documents\My Games\Skyrim Special Edition\SKSE\SKSEPowerAttack.log` shows the banner, `config: loaded:`,
   `input: game attack key:` (the control map's own attack binding), `input: input sink registered` and then one
   `tap #N ...` line per tap - see the log lines under Gotchas. `attack: 'right power attack' -> action
   0x00013383 performed=true` names the action that was submitted.

The cancel is the only part that cannot be verified from a build: bind Left Mouse, tap it (one power attack, no
normal swing) and hold it (one normal swing on release).

## Release and the mod page

- Two commands per release, never one: `.\tools\Build-Release.ps1 -Version x.y.z`, then
  `.\tools\Publish-Release.ps1 -Version x.y.z -Nexus -GitHub` (`-DryRun` first to validate). Building never
  publishes and publishing never builds.
- The mod is **live** on Nexus as `196152` (`https://www.nexusmods.com/skyrimspecialedition/mods/196152`) and
  on GitHub as `izicoder/skse-power-attack` (as of 0.2.1). The API key lives outside the repo:
  `$env:NEXUS_API_KEY` or `%USERPROFILE%\.pi\nexus-api-key.txt`; GitHub goes through `gh` (signed in as
  `izicoder`).
- Nexus has two id spaces: `196152` is the game-scoped id from the URL; the v3 ids (mod `7318624468536`,
  mod-file `8200119`) are what the upload API takes and are re-resolved on every run. Never hardcode a v3 id.
- The API can only add a **version** to an existing file entry — the first upload for a new mod is manual.
- Guards kept on purpose: the DLL inside the archive must report `-Version`, a version the Nexus file already
  has is refused without `-Force`, and an archive containing `SKSEPowerAttack.json` is refused.
- Page state: category Combat; tags Gameplay, Quality of Life, AI-Generated Content; requirements SKSE64 and
  Address Library, plus SKSE Menu Framework with an "optional" note; short description is the owner's
  vibe-coded one-liner. Screenshots and the publish button are the owner's.

## Decisions already made

- **Tap, not hold.** The attack fires on release, and only if the press was shorter than `tapWindowMs`
  (default 200 ms, range 20-1000). The chord is resolved at press, the gates at release. This mirrors
  BlinkDash's tap semantics deliberately.
- **Three rows, order is priority.** `src/Binds.h` lists them in menu order: right, left, dual. The row decides
  the action (`Selection::ForSlot`), which is a plain switch: there is no weapon detection, so the hands play no
  part in the decision. A press goes to the most specific chord (one that names modifiers beats a bare chord on
  the same key), and an exact duplicate goes to the row higher in the menu, so duplicates are legal and
  rebinding never fights the user. The vanilla form ids are `0x13004` left attack, `0x13005` right attack,
  `0x50C96` dual attack, `0x2E2F6` left power, `0x13383` right power, `0x2E2F7` dual power - the game has a full
  set for all three, which is what makes three rows possible. An earlier draft shipped a `powerAttack:
  right|left|dual` setting, and a later one detected the hand from equipped weapons; both were rejected in
  favour of rows ("fists counted as weapons", so detection lied about empty hands). Do not bring either back.
- **Dual-wield fallback, two guards.** `Selection::WithDualFallback` (unit tested) turns a left or right row into
  the two-weapon attack when both hands hold a one-handed weapon. Armed only while `Settings::dualFallback` is on
  AND the dual row has no key, so a key on dual or an unticked box both kill it. `Attack::OneHandedWeapon` is the
  only code that reads equipped gear now and is only called while the fallback is armed: fists are not weapon
  forms and two-handers are excluded by `WEAPON_TYPE`, which is what went wrong with the old auto row.
- **A bind is a chord** (`src/Chord.h`): key + `Chord::Mods` mask (shift 1, ctrl 2, alt 4), both left and right
  modifier scan codes map to the same bit. A modifier can never be a chord's key, and extra held modifiers are
  tolerated so sprinting does not block a bare chord.
- **Modifiers come from events, batch by batch, modifiers first.** The sink applies every modifier event in the
  batch before it looks at any key in the same batch, because Ctrl or Alt pressed in the same poll as the chord
  key can be dispatched after it (Shift usually arrives first, which is why only Ctrl and Alt looked unreliable).
  Do not switch to `BSInputDevice::IsPressed` for this: `GetKeyboard()` drags `BSWin32KeyboardDevice` into the
  link and the prebuilt CommonLibSSE bundle leaves those vtables unresolved (`LNK2001`).
- **A matched chord owns its press, and so does the game's attack key.** A taken press is blanked in place
  (`button->value = 0`), never cut with `kStop`, for a press whose matched chord names a modifier (vanilla ignores
  modifiers, so its unmodified action is not what the player asked for: Shift + E must not loot) and for the game's
  attack key even on a bare chord (`Input::IsGameAttackKey`, the control map's `"Attack"` event id), which is what
  stops a tap from swinging twice. A gate at press leaves the press with the game. If a taken press turns out to be
  a hold or hits a gate at release, `Attack::NormalAttack` hands the swing back. `kStop` is never returned: it drops
  the whole batch, releases included, and a swallowed release latches a held state.
- **Blocking is not a tap state: nothing is submitted while `IsBlocking`.** The engine only bashes from a block, and
  a shield or torch has no power attack at all, so `Attack::PowerAttack` and `Attack::NormalAttack` both log and
  return. The press still raises the block (the game reads its own held-button state, so blanking the event does not
  stop it) and the release lowers it. An attack submitted behind the engine's back there is what left a shield up for
  good - do not bring back the old normal-then-power fallback for blocks. Jumping keeps that fallback; it is safe.
- **What a hand can attack with is the engine's call, not a list.** There is no "can attack" record flag: `BGSAction`
  is a keyword plus an index, a torch is a `TESObjectLIGH` (bash only, like a shield), and modded items are unknown.
  So `ProcessAction` returning `performed=false` is the only abort, and it is final - never add a fallback on it.
- The tests in `tests/` cover exactly those three pure headers (`Chord`, `Binds`, `Selection`) and are the
  reason they are engine-free: `cmake --build build/vs2022 --target spa_ChordTests spa_BindsTests
  spa_SelectionTests`, or `ctest --test-dir build/vs2022 --output-on-failure`.
- Defaults are Shift + left mouse on the right row, Shift + right mouse on the left row, mouse button 5 on dual,
  dual-wield fallback off (`Config.h` `kDefaultRight` / `kDefaultLeft` / `kDefaultDual`). Every row can be cleared -
  the menu is the only way to unbind, so it has to be able to.
- The jump fallback is 200 ms of re-posting tasks, never a thread. Blocking has no fallback at all.
- No attribution to any other power-attack mod anywhere (README, page, comments); it was removed on request,
  do not re-add it.
- Key codes are SKSE InputMap numbers as-is. Do not invent an offset.
- SE/AE only: addresses come from the Address Library, VR is not supported.

## Gotchas that cost time here

- `SPA_VERSION` is a CMake cache variable, so a plain `cmake --build` keeps the last stamped number. Use
  `Build-Release.ps1` whenever the version matters.
- This machine's PowerShell has no `Get-FileHash` - compare files with `fc /b`, or .NET `SHA256`.
- Do not clean the repo root with `Get-ChildItem *.txt | Remove-Item`: it deletes `CMakeLists.txt`. Tooling
  scratch files belong outside the repo (`.playwright-mcp/` is ignored for that reason).
- A config file written by the pre-rename build (`OneClickPowerAttack.json`) is imported once and left in
  place; `config: importing the settings left by the earlier build` in the log is correct, not a bug.
- Reading a test run: `tap #N on '<chord>' (<box> row, held N ms) -> power attack`,
  `'<chord>' held N ms > 200 ms window, no power attack (hold)`, `tap ignored (<reason>)`,
  `'<chord>' down with modifiers 0x0, but the left row wants 'Shift + Mouse 5' - no row matched`,
  `input: menu '<name>' opened, dropping the modifier state`. A press taken from the game adds
  `, the game does not get this press` on the down line, and a tap that does not fire hands the swing back with
  `attack: the tap key was held, normal attack`.
- Config format is 3. A format 1 file (`hotkey`/`leftHotkey`/`modifierKey`) or a format 2 one (`binds` with
  `auto`) is migrated in place: format 1 becomes the format 2 shape first, then `binds.auto` moves to
  `binds.right` when that row is free (dropped, with a warning, when it is not). The left row is a plain left
  power attack now - the old format 1 second key used to give the dual attack while dual-wielding.
- The input sink never returns `kStop`. It used to, and it cost a shield that stayed up: a drop takes the whole
  batch, releases included. Taking a press means blanking its value, which the game's own attack handling respects
  but its held-button state does not - a press on the block key still raises the block. That is why the shield case
  is answered in `Attack.cpp` (submit nothing while blocking), not in the sink. A hook is still the only way to stop
  the engine's own block; ask before adding one.
- In the menu, a capture ends on the first non-modifier key press; modifiers only update the preview, and Escape
  cancels. That is what makes `Shift + Mouse 5` bindable - do not "fix" it by ending the capture on a modifier.
- Publishing to GitHub refuses with "HEAD (...) is not pushed to its upstream branch": push first, the release
  is pinned to that exact commit.
- Release tags only exist on the remote (`gh` creates them), so a local `git describe --tags` finds nothing and
  the default notes fall back to the whole history. `git fetch --tags` before publishing, or pass `-Notes`.
- The repo is codegraph-indexed (`.codegraph/`, git-ignored): ask `codegraph_explore` for architecture and
  flow questions, and re-run `codegraph index` after a large refactor.

## Conventions

- Tabs, Allman braces, `namespace SPA`.
- Heavily commented where the *why* is not obvious (engine calls, magic form ids, thread boundaries).
- Never let an exception cross an engine boundary: catch at the sink/hook/entry points and log.
- The menu edits `Config::Get()` in place and calls `Config::Touch()`; the game thread reads `Config::Cached()`.
  Keep that split if you add settings.
- `Config::Save()` writes a sibling `.tmp` and moves it over the real file; keep that.
- **Never break a user's existing `SKSEPowerAttack.json`.** Unknown keys are preserved, a bad value costs only
  that key, and a rename/type change means bumping `kSettingsFormatVersion` in `Config.cpp` plus exactly one
  `Migrate` step for the jump — not a new key that silently ignores the old value. Never ship a config file in
  the archive: the plugin writes one on first run, so an update never overwrites tuned settings.
- A new setting is read in `ApplyDocument()` (both the real file and the legacy import use it), validated in
  `Validate()` and shown in `UI.cpp` — a key nobody can reach from the menu is a bug, not a feature.
- Commit locally when a change builds, single-line imperative messages. Never push without being asked.
