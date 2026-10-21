#pragma once

#include "Chord.h"

#include <array>
#include <cstddef>
#include <optional>

// The power attack keys, in the order the menu shows them. Kept free of the game so the choice
// between them can be unit tested (tests/BindsTests.cpp).
//
// The order is the priority. Two boxes on the exact same chord are allowed - the one higher in
// the menu wins - which means the player never has to fight the menu to bind what they want.

namespace SPA::Binds
{
	enum class Slot : std::size_t
	{
		kAuto = 0,  // follows the hands: both weapons -> dual, one weapon -> that hand, nothing -> right
		kRight = 1,
		kLeft = 2,
		kDual = 3,
	};

	inline constexpr std::size_t kSlotCount = 4;

	using BindList = std::array<Chord::Bind, kSlotCount>;

	constexpr std::size_t Index(Slot a_slot) noexcept
	{
		return static_cast<std::size_t>(a_slot);
	}

	constexpr const char* Name(Slot a_slot) noexcept
	{
		switch (a_slot) {
		case Slot::kAuto: return "auto";
		case Slot::kRight: return "right";
		case Slot::kLeft: return "left";
		case Slot::kDual: return "dual";
		}
		return "auto";
	}

	// Which box a press belongs to, or nullopt when no box uses that key. The most specific
	// chord wins (Shift + Mouse 5 beats Mouse 5), and a full duplicate goes to the box higher in
	// the menu, because that is the only difference left between the two.
	constexpr std::optional<Slot> Resolve(const BindList& a_binds, int a_code, unsigned a_held) noexcept
	{
		std::optional<Slot> best{};
		int                 bestSpecificity = -1;
		for (std::size_t i = 0; i < kSlotCount; ++i) {
			const auto& bind = a_binds[i];
			if (!Chord::Matches(bind, a_code, a_held)) {
				continue;
			}
			const int specificity = Chord::Specificity(bind);
			if (specificity > bestSpecificity) {  // strict: an equal chord keeps the box above it
				bestSpecificity = specificity;
				best = static_cast<Slot>(i);
			}
		}
		return best;
	}
}
