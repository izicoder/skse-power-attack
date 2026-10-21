#pragma once

// The tap decision, kept free of the game so it can be unit tested (tests/SelectionTests.cpp):
// given what the player holds and which key was tapped, which power attack goes out.
//
// Vanilla keeps one action per hand and a third for the two-weapon spin, so this is a choice
// between three actions, not a setting:
//   both hands hold a weapon -> dual (ActionDualPowerAttack)
//   one hand holds a weapon  -> that hand (ActionRightPowerAttack / ActionLeftPowerAttack)
//   no weapon at all         -> the key's own hand (unarmed/beast form still attacks)
//
// The second key is a mirror of the first, not a second rule: same table, left and right
// swapped, which only changes the empty-handed case.

namespace SPA::Selection
{
	enum class Choice
	{
		kRight = 0,  // index order matters: Attack.cpp keeps its action pairs in this order
		kLeft = 1,
		kDual = 2,
	};

	// Which of the two binds was tapped.
	enum class Key
	{
		kMain,
		kLeft,
	};

	// The hand a key attacks with when there is nothing to choose from.
	constexpr Choice FallbackForKey(Key a_key) noexcept
	{
		return a_key == Key::kLeft ? Choice::kLeft : Choice::kRight;
	}

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
