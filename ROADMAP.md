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
| "Could you vibe code a mod for dual wield power attacks next" | Shipped in the next release, without a setting: the plugin now picks the hand from what you hold, so two weapons get the vanilla two-weapon power attack. |

## Done since 0.2.1

- **Automatic hand selection.** Both hands with weapons -> dual power attack; one weapon -> that hand; empty
  hands -> the tapped key's own hand. No config key, no menu toggle (`src/Selection.h`, unit tested).
- **Optional second key** (`leftHotkey`), unbound by default: same selection, left-hand fallback.

## Next

Nothing here is promised; each item is only worth doing if the idea survives being small.

1. **Bug reports first.** Nobody has posted a crash, a log or a "does nothing" yet. If one arrives, it outranks
   everything below. Collect version, SKSE log, game version.
2. **Wider runtime check.** `0.2.1` was built and played on 1.6.1170. Confirming it loads on the other common
   AE runtimes and on SE 1.5.97 costs a DLL load and one tap each, and it is the cheapest way to stop the
   "requires Address Library" footnote turning into a support thread.
3. **Dual-wield check in play.** `ActionDualPowerAttack` is vanilla, but it has not been played yet: verify it
   fires with sword+sword, dagger+sword and with MCO/ADXP installed.
4. **Menu polish.** The menu already captures a key press instead of asking for a code; what is left is showing
   the picked hand in the tab, if the player wants to see it.

## Deliberately not planned

- **A "which hand" setting.** Tried and rejected: it is configuration the player should not have to think about,
  and the hands already say what they want.
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
