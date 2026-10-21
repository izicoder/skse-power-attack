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

- **Automatic hand selection, then dropped.** Both hands with weapons -> dual, one weapon -> that hand, empty
  hands -> the tapped key's own hand (`src/Selection.h`, unit tested). Removed on request: fists count as weapons
  in some setups, so detection lied. Nothing reads the hands now.
- **Optional second key** (`leftHotkey`, format 1 only), unbound by default.
- **Three power attack rows** (`src/Binds.h`, `src/Selection.h`): right, left, dual. The row decides the action,
  a plain switch, no weapon detection. Every row can be cleared; the old `leftHotkey` became the left row and the
  old auto row became the right one on upgrade.
- **Dual-wield fallback** (`Selection::WithDualFallback`, `Settings::dualFallback`, on by default): while the dual
  row has no key, a one-handed weapon in each hand makes the left and right rows fire the two-weapon attack, which
  is what the removed auto row used to give dual wielders. Two guards: a key on the dual row, or the menu toggle.
- **Chords** (`src/Chord.h`): a bind is a key plus Shift/Ctrl/Alt, either side. A chord that names a modifier
  beats a bare chord on the same key, and an exact duplicate goes to the row higher in the menu, so rebinding
  never needs a fight. Config format 3, one migration step per jump (`hotkey`/`leftHotkey`/`modifierKey` -> 2,
  `binds.auto` -> `binds.right` -> 3).
- **Modifiers read from the whole input batch first** (`src/Input.cpp`): Ctrl and Alt pressed in the same poll as
  the chord key were dispatched after it, which made those two chords miss about half the time. Shift usually
  arrives first, which is why it looked fine.
- **The game's attack key is cancelled** (`src/Input.cpp`): a press the game would attack with is reported as
  handled (`kStop`) while a row owns it, so a tap gives only the power attack. A hold of that key is handed back
  as the normal attack. Untested in play - see "Next".

## Next

Nothing here is promised; each item is only worth doing if the idea survives being small.

1. **Bug reports first.** Nobody has posted a crash, a log or a "does nothing" yet. If one arrives, it outranks
   everything below. Collect version, SKSE log, game version.
2. **Verify the cancel in play.** Bind Left Mouse, tap it: exactly one power attack and no normal swing first.
   Hold it: one normal swing when you let go. `input: game attack key: mouse=256 ...` in the log says whether the
   control map reports the same numbering as the input events; if a tap still double-fires, the sink's `kStop` is
   not honoured and the only other route is a hook on the attack handler, which breaks the "no hooks" promise,
   so ask first.
3. **Wider runtime check.** `0.2.1` was built and played on 1.6.1170. Confirming it loads on the other common
   AE runtimes and on SE 1.5.97 costs a DLL load and one tap each, and it is the cheapest way to stop the
   "requires Address Library" footnote turning into a support thread.
4. **Dual-wield check in play.** `ActionDualPowerAttack` is vanilla, but it has not been played yet: verify it
   fires with sword+sword, dagger+sword and with MCO/ADXP installed.
5. **Menu polish.** The menu already captures chords and states each rule in one line; what is left is a way to
   show the last tap from inside the game instead of the log.

## Deliberately not planned

- **A "which hand" setting.** Tried and rejected: it is configuration the player should not have to think about.
  Right, left and dual are keys instead, and nothing guesses.
- Consuming keys the game does not attack with: there is nothing to cancel, and swallowing the press of a binding
  this build cannot see can only cost the game something.
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
