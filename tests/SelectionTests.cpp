// Unit tests for the tap decision: which power attack a row fires. Pure logic only
// (Selection.h), so they build and run without the game, CommonLibSSE or SKSE:
//
//   cmake --build build/vs2022 --target spa_SelectionTests && build/vs2022/spa_SelectionTests.exe
//
// There is nothing to keep in sync with the player's gear any more: a row names its action.

#include "Selection.h"

#include <cstdio>

namespace
{
	using namespace SPA;

	int failures = 0;

	void Expect(const char* a_what, Selection::Choice a_expected, Selection::Choice a_actual)
	{
		if (a_expected != a_actual) {
			++failures;
			std::printf("FAIL %s: expected %s, got %s\n", a_what,
				Selection::Name(a_expected), Selection::Name(a_actual));
		}
	}

	void ExpectText(const char* a_what, const char* a_expected, const char* a_actual)
	{
		if (std::string_view{ a_expected } != a_actual) {
			++failures;
			std::printf("FAIL %s: expected \"%s\", got \"%s\"\n", a_what, a_expected, a_actual);
		}
	}
}

int main()
{
	// The whole rule: the row is the action.
	Expect("right row", Selection::Choice::kRight, Selection::ForSlot(Binds::Slot::kRight));
	Expect("left row", Selection::Choice::kLeft, Selection::ForSlot(Binds::Slot::kLeft));
	Expect("dual row", Selection::Choice::kDual, Selection::ForSlot(Binds::Slot::kDual));

	// The row order the menu shows and the slot count the config and the sink rely on.
	static_assert(Binds::kSlotCount == 3);
	static_assert(Binds::Index(Binds::Slot::kRight) == 0);
	static_assert(Binds::Index(Binds::Slot::kLeft) == 1);
	static_assert(Binds::Index(Binds::Slot::kDual) == 2);

	// The dual-wield fallback: only while armed, only for the left and right rows, and only with a
	// one-handed weapon in each hand. "Armed" is the caller's answer to the setting plus "the dual
	// row has no key".
	{
		using Selection::WithDualFallback;
		Expect("both hands armed", Selection::Choice::kDual, WithDualFallback(Selection::Choice::kRight, true, true, true));
		Expect("left row, both hands", Selection::Choice::kDual, WithDualFallback(Selection::Choice::kLeft, true, true, true));
		Expect("disarmed keeps the right row", Selection::Choice::kRight, WithDualFallback(Selection::Choice::kRight, false, true, true));
		Expect("right hand empty keeps the row", Selection::Choice::kRight, WithDualFallback(Selection::Choice::kRight, true, true, false));
		Expect("left hand empty keeps the row", Selection::Choice::kLeft, WithDualFallback(Selection::Choice::kLeft, true, false, true));
		Expect("both hands empty keeps the row", Selection::Choice::kRight, WithDualFallback(Selection::Choice::kRight, true, false, false));
		Expect("dual row is untouched", Selection::Choice::kDual, WithDualFallback(Selection::Choice::kDual, true, false, false));
	}

	// Labels: the log and the menu read these, so a typo is a user-visible bug.
	ExpectText("right name", "right", Selection::Name(Selection::Choice::kRight));
	ExpectText("left name", "left", Selection::Name(Selection::Choice::kLeft));
	ExpectText("dual name", "dual", Selection::Name(Selection::Choice::kDual));
	ExpectText("right label", "right power attack", Selection::Describe(Selection::Choice::kRight));
	ExpectText("left label", "left power attack", Selection::Describe(Selection::Choice::kLeft));
	ExpectText("dual label", "dual power attack (both hands)", Selection::Describe(Selection::Choice::kDual));

	if (failures == 0) {
		std::printf("ok: selection tests passed\n");
		return 0;
	}
	std::printf("%d failure(s)\n", failures);
	return 1;
}
