# AGENTS.md

## What this is

**SKSE Power Attack** — a minimal SKSE plugin (C++23, CommonLibSSE-NG, Address Library) that turns a click of
one key into a vanilla right-hand power attack. One DLL: no plugin file, no Papyrus, no MCM. Optional settings
tab through SKSE Menu Framework.

A rewrite of jarari's One Click Power Attack, deliberately reduced to the single-click behaviour. Do not
re-add the original's extras (dual attack, MCO window notify/queue, long-press modes, block unbinding) unless
asked; "simple" is the point.

## Layout

| Path | What it does |
|---|---|
| `src/Plugin.cpp` | `SKSEPlugin_Load` + messaging: config load, action init, input/UI registration. |
| `src/Config.h/.cpp` | `Settings` (4 fields), JSON load/save next to the DLL, validation, revision counter. |
| `src/Attack.h/.cpp` | Resolves `AttackRight` / `PowerAttackRight`, submits them through the task interface, jump/block fallback. |
| `src/Input.h/.cpp` | Key encoding (SKSE InputMap numbering), labels, the input sink and its gates. |
| `src/UI.h/.cpp` | SKSE Menu Framework tab: rebinds, toggles, reset. |
| `src/Log.h` | `PA`/`PAI`/`PAW`/`PAE`, trace level, flushed per line. |
| `extern/SKSEMenuFramework.h` | Vendored framework header (do not edit). |

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

Never build with `MOD_OUTPUT_FOLDER` pointing at an enabled MO2 mod while the game is running: the post-build
copy replaces a DLL the running process has loaded. Configure with `-DMOD_OUTPUT_FOLDER=` for a local-only
build.

## Conventions

- Tabs, Allman braces, `namespace SPA`.
- Heavily commented where the *why* is not obvious (engine calls, magic form ids, thread boundaries).
- Never let an exception cross an engine boundary: catch at the sink/hook/entry points and log.
- The menu edits `Config::Get()` in place and calls `Config::Touch()`; the game thread reads `Config::Cached()`.
  Keep that split if you add settings.
- `Config::Save()` writes a sibling `.tmp` and moves it over the real file; keep that.
- Never break a user's existing `SKSEPowerAttack.json`: unknown keys are preserved.
- Commit locally when a change builds, single-line imperative messages. Never push without being asked.
