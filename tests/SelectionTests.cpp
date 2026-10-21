// Unit tests for the tap decision: which power attack a tap submits. They cover the pure logic
// only (Selection.h), so they build and run without the game, CommonLibSSE or SKSE:
//
//   cmake --build build/vs2022 --target spa_tests && build/vs2022/spa_tests.exe
//
// Add a case here before changing the rule; the table in Choose() is the whole feature.

#include "Selection.h"

#include <cstdio>
#include <vector>

namespace
{
	using SPA::Selection::Choice;
	using SPA::Selection::Key;

	int failures = 0;

	void Check(bool a_condition, const char* a_what, Choice a_expected, Choice a_actual)
	{
		if (!a_condition) {
			++failures;
			std::printf("FAIL %s: expected %s, got %s\n", a_what,
				SPA::Selection::Name(a_expected), SPA::Selection::Name(a_actual));
		}
	}

	void Expect(const char* a_what, Choice a_expected, Choice a_actual)
	{
		Check(a_expected == a_actual, a_what, a_expected, a_actual);
	}

	Choice Mirror(Choice a_choice)
	{
		switch (a_choice) {
		case Choice::kRight: return Choice::kLeft;
		case Choice::kLeft: return Choice::kRight;
		case Choice::kDual: return Choice::kDual;
		}
		return a_choice;
	}
}

int main()
{
	constexpr Key kMain = Key::kMain;
	constexpr Key kLeft = Key::kLeft;
	const auto mainFallback = SPA::Selection::FallbackForKey(kMain);
	const auto leftFallback = SPA::Selection::FallbackForKey(kLeft);

	// A weapon in both hands is the dual-wield spin, whichever key was tapped.
	Expect("both hands, main key", Choice::kDual, SPA::Selection::Choose(true, true, mainFallback));
	Expect("both hands, second key", Choice::kDual, SPA::Selection::Choose(true, true, leftFallback));

	// One weapon swings that hand, whichever key was tapped.
	Expect("right hand only, main key", Choice::kRight, SPA::Selection::Choose(true, false, mainFallback));
	Expect("right hand only, second key", Choice::kRight, SPA::Selection::Choose(true, false, leftFallback));
	Expect("left hand only, main key", Choice::kLeft, SPA::Selection::Choose(false, true, mainFallback));
	Expect("left hand only, second key", Choice::kLeft, SPA::Selection::Choose(false, true, leftFallback));

	// Nothing equipped: each key falls back to its own hand (unarmed and beast forms still hit).
	Expect("nothing equipped, main key", Choice::kRight, SPA::Selection::Choose(false, false, mainFallback));
	Expect("nothing equipped, second key", Choice::kLeft, SPA::Selection::Choose(false, false, leftFallback));

	// The fallbacks themselves: the main key defaults right, the second key defaults left.
	Expect("main key fallback", Choice::kRight, mainFallback);
	Expect("second key fallback", Choice::kLeft, leftFallback);

	// Symmetry: swapping the hands AND the key (main <-> second) must swap the right/left answer
	// and leave dual alone, for every equipment state. This is what keeps the second key a mirror
	// instead of a second rule; the empty-handed case falls back to the key's own hand, so the key
	// has to be mirrored along with the hands.
	for (int right = 0; right <= 1; ++right) {
		for (int left = 0; left <= 1; ++left) {
			const bool hasRight = right != 0;
			const bool hasLeft = left != 0;
			for (const auto key : { kMain, kLeft }) {
				const auto otherKey = key == kLeft ? kMain : kLeft;
				const auto direct = SPA::Selection::Choose(hasRight, hasLeft, SPA::Selection::FallbackForKey(key));
				const auto mirrored = SPA::Selection::Choose(hasLeft, hasRight, SPA::Selection::FallbackForKey(otherKey));
				if (Mirror(direct) != mirrored) {
					++failures;
					std::printf("FAIL symmetry right=%d left=%d key=%s: %s mirrors to %s, got %s\n",
						right, left, key == kLeft ? "second" : "main",
						SPA::Selection::Name(direct), SPA::Selection::Name(Mirror(direct)),
						SPA::Selection::Name(mirrored));
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
