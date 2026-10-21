// Unit tests for the box resolution: which of the four power attack boxes a press belongs to.
// Pure logic only (Binds.h), no game, no SKSE:
//
//   cmake --build build/vs2022 --target spa_BindsTests && build/vs2022/spa_BindsTests.exe
//
// The two rules under test: an identical chord goes to the box higher in the menu, and a chord
// that names a modifier beats a bare chord on the same key.

#include "Binds.h"

#include <cstdio>
#include <string>

namespace
{
	using namespace SPA;
	using Binds::BindList;
	using Binds::Slot;

	constexpr int kMouse5 = 260;
	constexpr int kMouse4 = 259;

	int failures = 0;

	void Check(bool a_condition, const char* a_what)
	{
		if (!a_condition) {
			++failures;
			std::printf("FAIL %s\n", a_what);
		}
	}

	void Expect(const char* a_what, std::optional<Slot> a_expected, const BindList& a_binds, int a_code, unsigned a_held)
	{
		const auto actual = Binds::Resolve(a_binds, a_code, a_held);
		if (actual != a_expected) {
			++failures;
			const auto name = [](std::optional<Slot> a_slot) {
				return a_slot ? std::string{ Binds::Name(*a_slot) } : std::string{ "nothing" };
			};
			std::printf("FAIL %s: expected %s, got %s\n", a_what, name(a_expected).c_str(), name(actual).c_str());
		}
	}

	BindList Empty()
	{
		return BindList{};
	}
}

int main()
{
	// Nothing bound: no key belongs to the plugin.
	Expect("nothing bound", std::nullopt, Empty(), kMouse5, Chord::kNone);

	// One box, one chord.
	{
		auto binds = Empty();
		binds[Binds::Index(Slot::kAuto)] = { kMouse5, Chord::kNone };
		Expect("auto only", Slot::kAuto, binds, kMouse5, Chord::kNone);
		Expect("auto only, other key", std::nullopt, binds, kMouse4, Chord::kNone);
		Expect("auto only, with a modifier held", Slot::kAuto, binds, kMouse5, Chord::kAlt);
	}

	// Each box can own its own key.
	{
		auto binds = Empty();
		binds[0] = { kMouse5, Chord::kNone };
		binds[1] = { 30, Chord::kNone };
		binds[2] = { 31, Chord::kNone };
		binds[3] = { kMouse4, Chord::kNone };
		Expect("auto box", Slot::kAuto, binds, kMouse5, Chord::kNone);
		Expect("right box", Slot::kRight, binds, 30, Chord::kNone);
		Expect("left box", Slot::kLeft, binds, 31, Chord::kNone);
		Expect("dual box", Slot::kDual, binds, kMouse4, Chord::kNone);
	}

	// The same chord on two boxes is allowed and goes to the box above.
	{
		auto binds = Empty();
		binds[0] = { kMouse5, Chord::kNone };
		binds[2] = { kMouse5, Chord::kNone };
		Expect("duplicate goes to auto", Slot::kAuto, binds, kMouse5, Chord::kNone);

		binds[0] = Chord::Bind{};
		Expect("duplicate with auto unbound goes to left", Slot::kLeft, binds, kMouse5, Chord::kNone);

		binds[1] = { kMouse5, Chord::kNone };
		binds[3] = { kMouse5, Chord::kNone };
		Expect("three duplicates, topmost wins", Slot::kRight, binds, kMouse5, Chord::kNone);
	}

	// A chord that names a modifier is more specific, so it wins even from a lower box. Without
	// this, Shift + Mouse 5 could never fire while a bare Mouse 5 sits in a box above it.
	{
		auto binds = Empty();
		binds[0] = { kMouse5, Chord::kNone };
		binds[1] = { kMouse5, Chord::kShift };
		Expect("shift wins over the bare chord above it", Slot::kRight, binds, kMouse5, Chord::kShift);
		Expect("bare chord still fires on its own", Slot::kAuto, binds, kMouse5, Chord::kNone);
		Expect("bare chord fires while alt is held", Slot::kAuto, binds, kMouse5, Chord::kAlt);
		Expect("ctrl does not count as shift", Slot::kAuto, binds, kMouse5, Chord::kCtrl);
	}

	// Two modifiers beat one, wherever the boxes are.
	{
		auto binds = Empty();
		binds[0] = { kMouse5, Chord::kShift };
		binds[3] = { kMouse5, Chord::kShift | Chord::kAlt };
		Expect("two modifiers beat one", Slot::kDual, binds, kMouse5, Chord::kShift | Chord::kAlt);
		Expect("one modifier alone", Slot::kAuto, binds, kMouse5, Chord::kShift);
	}

	// Unbound boxes (no key) never take a press, even when the code is 0.
	{
		auto binds = Empty();
		binds[0] = Chord::Bind{};
		binds[1] = Chord::Bind{};
		Expect("all unbound", std::nullopt, binds, 0, Chord::kNone);
	}

	// Every code the resolver can see comes from the sink, so it is never negative; a negative
	// key means "unbound" and must not match anything.
	{
		auto binds = Empty();
		binds[0] = { Chord::kNoKey, Chord::kNone };
		Expect("negative key is unbound", std::nullopt, binds, Chord::kNoKey, Chord::kNone);
	}

	if (failures == 0) {
		std::printf("ok: bind tests passed\n");
		return 0;
	}
	std::printf("%d failure(s)\n", failures);
	return 1;
}
