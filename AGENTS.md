# AGENTS.md

## What this is

**SKSE Power Attack** — a minimal SKSE plugin (C++23, CommonLibSSE-NG, Address Library) that turns a tap of
one key into a vanilla right-hand power attack. One DLL: no plugin file, no Papyrus, no MCM. Optional settings
tab through SKSE Menu Framework.

A minimal plugin by design: one tap, one power attack. Do not add extras (dual attack, attack-window
notify/queue, long-press modes, block unbinding) unless asked; "simple" is the point.

Keep the surface small, because that is what "compatible with everything" means here: no hooks, no vtable or
record edits, input events are never consumed, no threads (use the task interface and a re-posting task for
anything that has to wait, see `Attack::ScheduleDelayedPowerAttack`), and no config file shipped.

## Layout

| Path | What it does |
|---|---|
| `src/Plugin.cpp` | `SKSEPlugin_Load` + messaging: config load, action init, input/UI registration. |
| `src/Config.h/.cpp` | `Settings` (6 fields), JSON load/save next to the DLL, format version + migrations, validation, revision counter. |
| `src/Selection.h` | The tap decision, engine-free: hand(s) held + which key -> right, left or dual power attack. Unit tested. |
| `src/Binds.h` | Bind assignment, engine-free: one key, one role ("last one wins", the main key is protected). Unit tested. |
| `src/Attack.h/.cpp` | Resolves all six vanilla attack/power-attack actions, submits them through the task interface, jump/block fallback as a task chain (no threads). |
| `src/Input.h/.cpp` | Key encoding (SKSE InputMap numbering), labels, the tap sink (press + release inside the window) for both keys, its gates, the menu-open sink. |
| `tests/` | `SelectionTests.cpp`, `BindsTests.cpp`: plain `main()`, no framework, no engine. |
| `src/UI.h/.cpp` | SKSE Menu Framework tab: rebinds, toggles, two-click reset. |
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

1. `ctest --test-dir build/vs2022 --output-on-failure` passes (`SelectionTests`, `BindsTests`). They build in
   seconds without the engine, so write the case there before touching `Selection.h` or `Binds.h`.
2. `cmake --build build/vs2022` (or `Build-Release.ps1` when the version matters) ends with
   `Linking CXX shared library SKSEPowerAttack.dll` and no error lines.
3. `.\tools\Deploy.ps1` puts it in the MO2 folder (refuses while `SkyrimSE` runs), the mod is enabled, Skyrim is
   launched.
4. `…\Documents\My Games\Skyrim Special Edition\SKSE\SKSEPowerAttack.log` shows the banner, `config: loaded:`,
   `input: input sink registered` and then one `tap #N ...` line per tap - see the log lines under Gotchas.
   `attack: 'right power attack' -> action 0x00013383 performed=true` names the hand that was picked.

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
  (default 200 ms, range 20-1000). The modifier is judged at press, the gates at release. This mirrors
  BlinkDash's tap semantics deliberately.
- **The hand is not a setting.** `src/Selection.h` picks the action from what the player holds: weapons in both
  hands -> `ActionDualPowerAttack`, one weapon -> that hand's action, nothing -> the tapped key's own hand. The
  vanilla form ids are `0x13004` left attack, `0x13005` right attack, `0x50C96` dual attack, `0x2E2F6` left
  power, `0x13383` right power, `0x2E2F7` dual power. An earlier draft shipped a `powerAttack: right|left|dual`
  key; the owner rejected it as clunky configuration. Do not bring it back.
- **Two keys, one rule.** `hotkey` (mouse button 5) and `leftHotkey` (unbound by default) both call the same
  `Selection::Choose`; only the empty-handed fallback differs (`Selection::FallbackForKey`). The second key is a
  mirror, not a second feature.
- **One key, one role.** `src/Binds.h` `Assign()` gives a key to whoever was bound last and unbinds the previous
  holder; the main key is protected (the menu refuses instead of taking it). The menu goes through `Assign()`;
  `Validate()` applies the same precedence to a hand-edited file by dropping the optional key.
- The tests in `tests/` cover exactly those two pure headers (`Selection`, `Binds`) and are the reason they are
  engine-free: `cmake --build build/vs2022 --target spa_SelectionTests spa_BindsTests`, or
  `ctest --test-dir build/vs2022 --output-on-failure`.
- Default hotkey is mouse button 5 (`260`), not right mouse.
- The jump/block fallback is 200 ms of re-posting tasks, never a thread.
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
- Reading a test run: `tap #N on '<key>' (held N ms) -> power attack`, `held N ms > 200 ms window, no power
  attack (hold)`, `tap ignored (<reason>)`, `input: menu '<name>' opened, dropping the modifier state`.
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
