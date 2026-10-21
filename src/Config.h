#pragma once

#include "PCH.h"
#include "Binds.h"

namespace SPA
{
	// Key codes use SKSE's InputMap numbering:
	//   0..255    keyboard scancode
	//   256..263  mouse button (256 = left, 257 = right, 258 = middle, ...)
	//   266..281  gamepad button (dpad, start/back, sticks, shoulders, A/B/X/Y, LT/RT)
	// which is SKSE's own numbering, so a hand-copied value still means the same key.
	// Defined once, in Chord.h, so the bind rules and the settings share it.
	constexpr int kNoKey = Chord::kNoKey;
	constexpr int kMaxKeyCode = 281;

	// The chord each row gets out of the box. Validate() puts the right row's chord back when its
	// key is junk, because that is the row every fresh install uses.
	constexpr Chord::Bind kDefaultRight{ 256, Chord::kShift };  // Shift + left mouse
	constexpr Chord::Bind kDefaultLeft{ 257, Chord::kShift };   // Shift + right mouse
	constexpr Chord::Bind kDefaultDual{ 260, Chord::kNone };    // mouse button 5

	// Tap window bounds in milliseconds, shared by the setting, its validation and the menu
	// slider: below 20 ms a human cannot tap it anyway, and above a second it is a hold.
	constexpr int kMinTapWindowMs = 20;
	constexpr int kMaxTapWindowMs = 1000;

	struct Settings
	{
		// The three power attack keys in menu order (Binds::Slot). A key is a chord, so the modifier
		// travels with it instead of being one global setting.
		Binds::BindList binds{ kDefaultRight, kDefaultLeft, kDefaultDual };
		int  tapWindowMs{ 200 };        // press AND release inside this window, otherwise nothing happens
		bool allowZeroStamina{ true };  // power attack with an empty stamina bar (0 stamina mods)
		bool dualFallback{ false };     // left/right rows fire the two-weapon attack when dual wielding
		bool submitWhileBlocking{ false };  // old behaviour: submit the attack even with a block up (stuck shield)
		bool logging{ true };           // write SKSEPowerAttack.log
	};

	class Config
	{
	public:
		// The live settings. The menu edits these in place.
		static Settings& Get();

		// A copy taken under the settings lock.
		static Settings Snapshot();

		// Game-thread copy, refreshed only when something changed (Revision). Do not touch this
		// from the menu thread.
		static const Settings& Cached();

		// Bumped by every change; the game thread compares it to know when to re-read.
		static std::uint64_t Revision();
		static void          Touch();

		static void ResetToDefaults();
		static std::string Describe();
		static bool        Validate();

		static void                      Load();
		static void                      Save();
		static std::filesystem::path     Path();
	};
}
