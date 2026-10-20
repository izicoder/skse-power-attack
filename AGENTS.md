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
| `src/Config.h/.cpp` | `Settings` (5 fields), JSON load/save next to the DLL, format version + migrations, validation, revision counter. |
| `src/Attack.h/.cpp` | Resolves `AttackRight` / `PowerAttackRight`, submits them through the task interface, jump/block fallback as a task chain (no threads). |
| `src/Input.h/.cpp` | Key encoding (SKSE InputMap numbering), labels, the tap sink (press + release inside the window), its gates, the menu-open sink. |
| `src/UI.h/.cpp` | SKSE Menu Framework tab: rebinds, toggles, two-click reset. |
| `src/Log.h` | `PA`/`PAI`/`PAW`/`PAE`, trace level, flushed per line. |
| `extern/SKSEMenuFramework.h` | Vendored framework header (do not edit). |
| `tools/Deploy.ps1` | Manual deploy of the built DLL into one mod folder (refuses while Skyrim runs). |
| `tools/Build-Release.ps1` | Version-stamped build + `packages/SKSEPowerAttack-<version>.zip` for test installs. |

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

There are no automated tests. Verifying a change means building, enabling the mod, launching Skyrim and reading
`…\Documents\My Games\Skyrim Special Edition\SKSE\SKSEPowerAttack.log`.

Building never deploys. `tools/Deploy.ps1` copies the DLL into a mod folder and is the only thing that does;
it refuses to run while `SkyrimSE` is open. Do not turn on `MOD_OUTPUT_FOLDER` in the build: a post-build copy
would silently replace a DLL the running game has loaded.

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
