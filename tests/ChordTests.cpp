// Unit tests for the chord rules: which modifiers belong to which scan code, when a chord
// matches, and how labels are built. Pure logic only (Chord.h), no game, no SKSE:
//
//   cmake --build build/vs2022 --target spa_ChordTests && build/vs2022/spa_ChordTests.exe

#include "Chord.h"

#include <cstdio>

namespace
{
	using namespace SPA;

	int failures = 0;

	void Check(bool a_condition, const char* a_what)
	{
		if (!a_condition) {
			++failures;
			std::printf("FAIL %s\n", a_what);
		}
	}

	void ExpectLabel(const char* a_what, const char* a_expected, unsigned a_mods)
	{
		if (const auto actual = Chord::ModsPrefix(a_mods); actual != a_expected) {
			++failures;
			std::printf("FAIL %s: expected \"%s\", got \"%s\"\n", a_what, a_expected, actual.c_str());
		}
	}
}

int main()
{
	constexpr int kMouse5 = 260;

	// Both sides of the keyboard are the same modifier, so it does not matter which you hold.
	Check(Chord::ModBitFor(Chord::kScanLShift) == Chord::kShift, "left shift is shift");
	Check(Chord::ModBitFor(Chord::kScanRShift) == Chord::kShift, "right shift is shift");
	Check(Chord::ModBitFor(Chord::kScanLCtrl) == Chord::kCtrl, "left ctrl is ctrl");
	Check(Chord::ModBitFor(Chord::kScanRCtrl) == Chord::kCtrl, "right ctrl is ctrl");
	Check(Chord::ModBitFor(Chord::kScanLAlt) == Chord::kAlt, "left alt is alt");
	Check(Chord::ModBitFor(Chord::kScanRAlt) == Chord::kAlt, "right alt is alt");

	// Everything else is a chord key, not a modifier.
	Check(Chord::ModBitFor(kMouse5) == Chord::kNone, "mouse 5 is not a modifier");
	Check(Chord::ModBitFor(30) == Chord::kNone, "the A key is not a modifier");
	Check(Chord::ModBitFor(-1) == Chord::kNone, "no key is not a modifier");
	Check(!Chord::IsModifier(kMouse5), "IsModifier(mouse 5) is false");
	Check(Chord::IsModifier(Chord::kScanLShift), "IsModifier(shift) is true");

	// A bare chord matches no matter what else is held: sprinting with Alt must not block it.
	{
		const Chord::Bind bare{ kMouse5, Chord::kNone };
		Check(Chord::Matches(bare, kMouse5, Chord::kNone), "bare chord matches with nothing held");
		Check(Chord::Matches(bare, kMouse5, Chord::kAlt), "bare chord matches while alt is held");
		Check(Chord::Matches(bare, kMouse5, Chord::kShift | Chord::kCtrl | Chord::kAlt), "bare chord matches with everything held");
		Check(!Chord::Matches(bare, kMouse5 + 1, Chord::kNone), "bare chord does not match another key");
	}

	// A chord with modifiers needs all of them and does not care about the rest.
	{
		const Chord::Bind shift{ kMouse5, Chord::kShift };
		Check(Chord::Matches(shift, kMouse5, Chord::kShift), "shift chord matches with shift");
		Check(Chord::Matches(shift, kMouse5, Chord::kShift | Chord::kAlt), "shift chord matches with shift and alt");
		Check(!Chord::Matches(shift, kMouse5, Chord::kNone), "shift chord needs shift");
		Check(!Chord::Matches(shift, kMouse5, Chord::kCtrl), "ctrl is not shift");
		Check(!Chord::Matches(shift, kMouse5, Chord::kAlt), "alt is not shift");
	}

	{
		const Chord::Bind both{ kMouse5, Chord::kShift | Chord::kCtrl };
		Check(Chord::Matches(both, kMouse5, Chord::kShift | Chord::kCtrl), "two-modifier chord matches");
		Check(!Chord::Matches(both, kMouse5, Chord::kShift), "two-modifier chord needs both");
	}

	// An unbound box never matches, whatever key arrives.
	{
		const Chord::Bind unbound{};
		Check(!Chord::Matches(unbound, kMouse5, Chord::kNone), "unbound chord never matches");
		Check(!Chord::Matches(unbound, Chord::kNoKey, Chord::kNone), "no key is not a key");
	}

	// Specificity is the number of modifiers named, which is what Resolve() compares.
	{
		Check(Chord::Specificity({ kMouse5, Chord::kNone }) == 0, "bare chord is specificity 0");
		Check(Chord::Specificity({ kMouse5, Chord::kShift }) == 1, "shift chord is specificity 1");
		Check(Chord::Specificity({ kMouse5, Chord::kShift | Chord::kAlt }) == 2, "shift+alt chord is specificity 2");
		Check(Chord::Specificity({ kMouse5, Chord::kAllMods }) == 3, "all modifiers is specificity 3");
	}

	// Labels: the key name is the engine's job, so this only covers the prefix.
	ExpectLabel("bare chord label", "", Chord::kNone);
	ExpectLabel("shift label", "Shift + ", Chord::kShift);
	ExpectLabel("ctrl label", "Ctrl + ", Chord::kCtrl);
	ExpectLabel("alt label", "Alt + ", Chord::kAlt);
	ExpectLabel("two modifiers label", "Shift + Ctrl + ", Chord::kShift | Chord::kCtrl);
	ExpectLabel("all modifiers label", "Shift + Ctrl + Alt + ", Chord::kAllMods);

	if (failures == 0) {
		std::printf("ok: chord tests passed\n");
		return 0;
	}
	std::printf("%d failure(s)\n", failures);
	return 1;
}
