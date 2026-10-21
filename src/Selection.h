#pragma once

#include "Binds.h"

// The tap decision, kept free of the game so it can be unit tested (tests/SelectionTests.cpp):
// which power attack a row fires.
//
// Vanilla keeps one action per hand and a third for the two-weapon spin. A row names one of
// those three and nothing else is consulted: no weapon detection, so what fires never depends
// on what the player happens to hold. What the engine does with a left power attack while the
// left hand is empty is the engine's business.

namespace SPA::Selection
{
	enum class Choice
	{
		kRight = 0,  // index order matters: Attack.cpp keeps its action pairs in this order
		kLeft = 1,
		kDual = 2,
	};

	constexpr Choice ForSlot(Binds::Slot a_slot) noexcept
	{
		switch (a_slot) {
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

	// The label the log prints next to the action, so a support log says which row fired.
	constexpr const char* Describe(Choice a_choice) noexcept
	{
		switch (a_choice) {
		case Choice::kDual: return "dual power attack (both hands)";
		case Choice::kLeft: return "left power attack";
		case Choice::kRight: return "right power attack";
		}
		return "power attack";
	}
}
