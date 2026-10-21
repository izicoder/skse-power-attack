#pragma once

// Bind assignment rules, kept free of the game so they can be unit tested (tests/BindsTests.cpp).
// One key must never be two roles at once: the last key the user binds takes it, and whatever
// held it before lets go (its value becomes kNoKey).
//
// The one exception is the main power attack key. It is mandatory, so an optional bind cannot
// take its key - the menu refuses that with a log line instead of leaving the tap with no key.

namespace SPA::Binds
{
	// Same "no key" value as SPA::kNoKey in Config.h, which aliases this one.
	inline constexpr int kNoKey = -1;

	enum class Target
	{
		kHotkey,
		kLeftHotkey,
		kModifier,
	};

	// Assigns a_code to a_target. Returns false when the assignment is refused (a_code is -1, or
	// it belongs to the main key); the three values are left untouched in that case.
	constexpr bool Assign(int& a_hotkey, int& a_leftHotkey, int& a_modifierKey, Target a_target, int a_code) noexcept
	{
		if (a_code == kNoKey) {
			return false;
		}
		if (a_target != Target::kHotkey && a_code == a_hotkey) {
			return false;  // the main key is not up for grabs
		}

		if (a_target != Target::kLeftHotkey && a_leftHotkey == a_code) {
			a_leftHotkey = kNoKey;
		}
		if (a_target != Target::kModifier && a_modifierKey == a_code) {
			a_modifierKey = kNoKey;
		}

		switch (a_target) {
		case Target::kHotkey: a_hotkey = a_code; break;
		case Target::kLeftHotkey: a_leftHotkey = a_code; break;
		case Target::kModifier: a_modifierKey = a_code; break;
		}
		return true;
	}
}
