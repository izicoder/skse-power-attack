#pragma once

#include "PCH.h"

namespace SPA
{
	// Key codes use SKSE's InputMap numbering:
	//   0..255    keyboard scancode
	//   256..263  mouse button (256 = left, 257 = right, 258 = middle, ...)
	//   266..281  gamepad button (dpad, start/back, sticks, shoulders, A/B/X/Y, LT/RT)
	// which is SKSE's own numbering, so a hand-copied value still means the same key.
	constexpr int kNoKey = -1;
	constexpr int kMaxKeyCode = 281;

	struct Settings
	{
		int  hotkey{ 260 };            // click this to power attack (260 = mouse button 5)
		int  modifierKey{ kNoKey };    // optional: hotkey only counts while this is held
		bool allowZeroStamina{ true };  // power attack with an empty stamina bar (0 stamina mods)
		bool logging{ true };          // write SKSEPowerAttack.log
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
