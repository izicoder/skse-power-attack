// Unit tests for the tap decision: which power attack a box fires. Pure logic only
// (Selection.h), so they build and run without the game, CommonLibSSE or SKSE:
//
//   cmake --build build/vs2022 --target spa_SelectionTests && build/vs2022/spa_SelectionTests.exe
//
// Add a case here before changing the rule; the table in Choose() is the whole auto box.

#include "Selection.h"

#include <cstdio>

namespace
{
	using namespace SPA;
	using Binds::Slot;
	using Selection::Choice;

	int failures = 0;

	void Expect(const char* a_what, Choice a_expected, Choice a_actual)
	{
		if (a_expected != a_actual) {
			++failures;
			std::printf("FAIL %s: expected %s, got %s\n", a_what,
				Selection::Name(a_expected), Selection::Name(a_actual));
		}
	}

	constexpr Slot kSlots[] = { Slot::kAuto, Slot::kRight, Slot::kLeft, Slot::kDual };
}

int main()
{
	// The auto box: a weapon in both hands is the dual-wield spin.
	Expect("both hands", Choice::kDual, Selection::Choose(true, true, Choice::kRight));
	// One weapon swings that hand.
	Expect("right hand only", Choice::kRight, Selection::Choose(true, false, Choice::kRight));
	Expect("left hand only", Choice::kLeft, Selection::Choose(false, true, Choice::kRight));
	// Nothing equipped falls back, and the auto box falls back to the right hand (unarmed and
	// beast forms still attack).
	Expect("nothing equipped", Choice::kRight, Selection::Choose(false, false, Choice::kRight));

	// Every box on every equipment state, which is the table the player sees in the menu.
	for (int right = 0; right <= 1; ++right) {
		for (int left = 0; left <= 1; ++left) {
			const bool hasRight = right != 0;
			const bool hasLeft = left != 0;
			for (const auto slot : kSlots) {
				Choice expected = Choice::kRight;
				switch (slot) {
				case Slot::kAuto: expected = Selection::Choose(hasRight, hasLeft, Choice::kRight); break;
				case Slot::kRight: expected = Choice::kRight; break;
				case Slot::kLeft: expected = Choice::kLeft; break;
				case Slot::kDual: expected = Choice::kDual; break;
				}
				const auto actual = Selection::ForSlot(slot, hasRight, hasLeft);
				if (actual != expected) {
					++failures;
					std::printf("FAIL box %s right=%d left=%d: expected %s, got %s\n",
						Binds::Name(slot), right, left,
						Selection::Name(expected), Selection::Name(actual));
				}
			}
		}
	}

	// The three fixed boxes ignore the hands entirely: what the player bound is what fires.
	for (const auto slot : { Slot::kRight, Slot::kLeft, Slot::kDual }) {
		for (int right = 0; right <= 1; ++right) {
			for (int left = 0; left <= 1; ++left) {
				if (Selection::ForSlot(slot, right != 0, left != 0) != Selection::ForSlot(slot, false, false)) {
					++failures;
					std::printf("FAIL box %s followed the hands\n", Binds::Name(slot));
				}
			}
		}
	}

	if (failures == 0) {
		std::printf("ok: selection tests passed\n");
		return 0;
	}
	std::printf("%d failure(s)\n", failures);
	return 1;
}
