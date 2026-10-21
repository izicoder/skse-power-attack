#pragma once

// A key bind is a chord: one key plus the modifiers that have to be held with it. Kept free of
// the game so the matching rule can be unit tested (tests/ChordTests.cpp).
//
// Extra held modifiers are tolerated: sprinting with Alt down must not stop a plain Mouse 5 from
// firing. So a chord that names modifiers is more specific than a bare chord on the same key, and
// Binds::Resolve prefers it. Two chords with the same key and the same modifiers are the same
// chord, and the menu order decides those.

#include <string>

namespace SPA::Chord
{
	constexpr int kNoKey = -1;

	// Modifier bits, in the order a label prints them.
	enum Mods : unsigned
	{
		kNone = 0u,
		kShift = 1u << 0,
		kCtrl = 1u << 1,
		kAlt = 1u << 2,
	};

	constexpr unsigned kAllMods = kShift | kCtrl | kAlt;

	// Scan codes of the modifier keys on DirectInput keyboards, which is what the input sink
	// sees. Left and right count the same: it does not matter which one you hold.
	constexpr int kScanEscape = 1;
	constexpr int kScanLCtrl = 29;
	constexpr int kScanLShift = 42;
	constexpr int kScanRShift = 54;
	constexpr int kScanLAlt = 56;
	constexpr int kScanRCtrl = 157;
	constexpr int kScanRAlt = 184;

	constexpr unsigned ModBitFor(int a_code) noexcept
	{
		switch (a_code) {
		case kScanLShift:
		case kScanRShift:
			return kShift;
		case kScanLCtrl:
		case kScanRCtrl:
			return kCtrl;
		case kScanLAlt:
		case kScanRAlt:
			return kAlt;
		default:
			return kNone;
		}
	}

	constexpr bool IsModifier(int a_code) noexcept
	{
		return ModBitFor(a_code) != kNone;
	}

	struct Bind
	{
		int      key{ kNoKey };
		unsigned mods{ kNone };

		constexpr bool IsBound() const noexcept { return key >= 0; }
	};

	// Every modifier the chord names is held. Modifiers it does not name are ignored.
	constexpr bool ModsHeld(const Bind& a_bind, unsigned a_held) noexcept
	{
		return (a_bind.mods & a_held) == a_bind.mods;
	}

	constexpr bool Matches(const Bind& a_bind, int a_code, unsigned a_held) noexcept
	{
		return a_bind.IsBound() && a_bind.key == a_code && ModsHeld(a_bind, a_held);
	}

	// How many modifiers the chord names. Resolve() compares these.
	constexpr int Specificity(const Bind& a_bind) noexcept
	{
		const auto mods = a_bind.mods & kAllMods;
		return (mods & kShift ? 1 : 0) + (mods & kCtrl ? 1 : 0) + (mods & kAlt ? 1 : 0);
	}

	// "Shift + Ctrl + " for a label; empty for a bare chord. The caller appends the key name,
	// which is the engine's job because only SKSE knows what a scan code is called.
	inline std::string ModsPrefix(unsigned a_mods)
	{
		std::string prefix;
		const auto add = [&prefix](const char* a_name) {
			prefix += a_name;
			prefix += " + ";
		};
		if (a_mods & kShift) {
			add("Shift");
		}
		if (a_mods & kCtrl) {
			add("Ctrl");
		}
		if (a_mods & kAlt) {
			add("Alt");
		}
		return prefix;
	}
}
