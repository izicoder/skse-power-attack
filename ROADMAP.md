# Roadmap

Where the plugin stands, what came in from the Nexus page, and what is worth doing next. Kept short on
purpose: the whole point of this mod is that it stays one tap and one DLL.

Checked 21 Oct 2026, with `0.2.1` live.

## Where it stands

- Live on Nexus (`196152`) and GitHub (`izicoder/skse-power-attack`), version `0.2.1`.
- Feature-complete for the original idea: tap a key, get a vanilla right-hand power attack, plus the
  jump/block fallback, the empty-stamina toggle, an optional modifier and the SKSE Menu Framework tab.
- Untested surface: only 1.6.1170 was ever played. No VR support by design (Address Library, SE/AE only).
  No automated tests, so "verified" means a build plus a log from a real session.

## From the comment section

Three comments since release, none of them a bug report:

| Comment | Verdict |
|---|---|
| "why is it called vibe coded if everyone hates it" + "you get to vibe while the ai codes" | No action. Page text and the AI disclosure stay as they are. |
| "Could you vibe code a mod for dual wield power attacks next" | Not this mod. See below. |

## Next

Nothing here is promised; each item is only worth doing if the idea survives being small.

1. **Bug reports first.** Nobody has posted a crash, a log or a "does nothing" yet. If one arrives, it outranks
   everything below. Collect version, SKSE log, game version.
2. **Wider runtime check.** `0.2.1` was built and played on 1.6.1170. Confirming it loads on the other common
   AE runtimes and on SE 1.5.97 costs a DLL load and one tap each, and it is the cheapest way to stop the
   "requires Address Library" footnote turning into a support thread.
3. **Left-hand variant.** A setting (`hand`: right/left) that submits `PowerAttackLeft` instead. Same code path,
   one action id, no new concepts. Probably a candidate for `0.3.0`.
4. **Menu polish.** The tab currently shows rebinds, the stamina toggle and logging. A "press a key to bind"
   flow instead of typing a number would cut the support cost of key codes, if it can be done without pulling
   in a new dependency.

## Deliberately not planned

- **Dual-wield power attack** (the one request so far). It needs the dual attack actions and a two-weapon state
  check, which turns a one-action plugin into a combat-behaviour plugin, and it collides with every animation
  framework that already offers it. If it ever happens it is a separate mod.
- Attack-window notify or queue, long-press modes, block unbinding, a MCM, a calendar of extras. "Simple" is what
  makes it compatible with everything.
- VR.
- Any attribution to another power attack mod on the page, in the README or in comments.

## Shipping

Two commands, never one, and only when asked:

```powershell
.\tools\Build-Release.ps1 -Version 0.3.0
.\tools\Publish-Release.ps1 -Version 0.3.0 -Nexus -GitHub -DryRun
.\tools\Publish-Release.ps1 -Version 0.3.0 -Nexus -GitHub
```

The Nexus API can only add a version to the existing file entry. A brand new file entry for this mod is still a
manual upload.
