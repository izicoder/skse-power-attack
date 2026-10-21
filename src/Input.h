#pragma once

#include "PCH.h"
#include "Chord.h"

namespace SPA
{
	namespace Keys
	{
		constexpr std::uint32_t kMouseOffset = 256;
		constexpr std::uint32_t kGamepadOffset = 266;
		constexpr std::uint32_t kUnknown = 0xFFFFFFFF;

		// Turns a ButtonEvent id into the code stored in the config (SKSE InputMap numbering).
		std::uint32_t Encode(RE::INPUT_DEVICE a_device, std::uint32_t a_idCode);

		// Human readable name for the menu and the log.
		std::string Describe(int a_code);

		// "Shift + Mouse 5", or "None" for an unbound box. Only the key name comes from SKSE; the
		// modifier prefix is Chord.h, which is why that part is unit tested.
		std::string DescribeChord(const Chord::Bind& a_bind);
	}

	namespace Input
	{
		void Register();

		// Drops the modifier state: a key held while a menu opens or a save loads never gets its
		// release event and would otherwise stay "held" for the rest of the session.
		void ResetState();

		// Modifier bits held right now (Chord::kShift and friends). The menu seeds its capture
		// with this so Shift already held when the capture starts still counts.
		unsigned HeldMods();
	}
}
