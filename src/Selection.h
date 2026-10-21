#pragma once

#include "Binds.h"

// The tap decision, kept free of the game so it can be unit tested (tests/SelectionTests.cpp):
// given the four boxes and what the player holds, which power attack goes out.
//
// Vanilla keeps one action per hand and a third for the two-weapon spin, so this is a choice
// between three actions. The auto box picks from the equipment; right, left and dual are what
// the player bound them to, whether or not the hands could do it (the engine decides that).

namespace SPA::Selection
{
	enum class Choice
	{
		kRight = 0,  // index order matters: Attack.cpp keeps its action pairs in this order
		kLeft = 1,
		kDual = 2,
	};

	constexpr Choice Choose(bool a_rightWeapon, bool a_leftWeapon, Choice a_fallback) noexcept
	{
		if (a_rightWeapon && a_leftWeapon) {
			return Choice::kDual;
		}
		if (a_rightWeapon) {
			return Choice::kRight;
		}
		if (a_leftWeapon) {
			return Choice::kLeft;
		}
		return a_fallback;
	}

	// Which action a box fires. Only the auto box looks at the hands; with nothing equipped it
	// swings right, because the auto box is the one bound by default.
	constexpr Choice ForSlot(Binds::Slot a_slot, bool a_rightWeapon, bool a_leftWeapon) noexcept
	{
		switch (a_slot) {
		case Binds::Slot::kAuto: return Choose(a_rightWeapon, a_leftWeapon, Choice::kRight);
		case Binds::Slot::kRight: return Choice::kRight;
		case Binds::Slot::kLeft: return Choice::kLeft;
		case Binds::Slot::kDual: return Choice::kDual;
		}
		return Choice::kRight;
	}

	constexpr const char* Name(Choice a_choice) noexcept
	{
		switch (a_choice) {
		case Choice::kRight: return "right";
		case Choice::kLeft: return "left";
		case Choice::kDual: return "dual";
		}
		return "right";
	}

	// The label the log prints next to the action, so a support log says why it swung that hand.
	constexpr const char* Describe(Choice a_choice, bool a_anythingEquipped) noexcept
	{
		switch (a_choice) {
		case Choice::kDual: return "dual power attack (both hands)";
		case Choice::kLeft: return a_anythingEquipped ? "left power attack" : "left power attack (nothing equipped)";
		case Choice::kRight: return a_anythingEquipped ? "right power attack" : "right power attack (nothing equipped)";
		}
		return "power attack";
	}
}
