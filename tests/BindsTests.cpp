// Unit tests for the row resolution: which of the three power attack rows a press belongs to.
// Pure logic only (Binds.h), no game, no SKSE:
//
//   cmake --build build/vs2022 --target spa_BindsTests && build/vs2022/spa_BindsTests.exe
//
// The two rules under test: an identical chord goes to the row higher in the menu, and a chord
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

	// One row, one chord.
	{
		auto binds = Empty();
		binds[Binds::Index(Slot::kRight)] = { kMouse5, Chord::kNone };
		Expect("right only", Slot::kRight, binds, kMouse5, Chord::kNone);
		Expect("right only, other key", std::nullopt, binds, kMouse4, Chord::kNone);
		Expect("right only, with a modifier held", Slot::kRight, binds, kMouse5, Chord::kAlt);
	}

	// Each row can own its own key.
	{
		auto binds = Empty();
		binds[0] = { kMouse5, Chord::kNone };
		binds[1] = { 30, Chord::kNone };
		binds[2] = { kMouse4, Chord::kNone };
		Expect("right row", Slot::kRight, binds, kMouse5, Chord::kNone);
		Expect("left row", Slot::kLeft, binds, 30, Chord::kNone);
		Expect("dual row", Slot::kDual, binds, kMouse4, Chord::kNone);
	}

	// The same chord on two rows is allowed and goes to the row above.
	{
		auto binds = Empty();
		binds[0] = { kMouse5, Chord::kNone };
		binds[1] = { kMouse5, Chord::kNone };
		Expect("duplicate goes to right", Slot::kRight, binds, kMouse5, Chord::kNone);

		binds[0] = Chord::Bind{};
		Expect("duplicate with right unbound goes to left", Slot::kLeft, binds, kMouse5, Chord::kNone);

		binds[1] = Chord::Bind{};
		binds[2] = { kMouse5, Chord::kNone };
		Expect("duplicate on the last row", Slot::kDual, binds, kMouse5, Chord::kNone);
	}

	// A chord that names a modifier is more specific, so it wins even from a lower row. Without
	// this, Shift + Mouse 5 could never fire while a bare Mouse 5 sits in a row above it.
	{
		auto binds = Empty();
		binds[0] = { kMouse5, Chord::kNone };
		binds[1] = { kMouse5, Chord::kShift };
		Expect("shift wins over the bare chord above it", Slot::kLeft, binds, kMouse5, Chord::kShift);
		Expect("bare chord still fires on its own", Slot::kRight, binds, kMouse5, Chord::kNone);
		Expect("bare chord fires while alt is held", Slot::kRight, binds, kMouse5, Chord::kAlt);
		Expect("ctrl does not count as shift", Slot::kRight, binds, kMouse5, Chord::kCtrl);
	}

	// Two modifiers beat one, wherever the rows are.
	{
		auto binds = Empty();
		binds[0] = { kMouse5, Chord::kShift };
		binds[2] = { kMouse5, Chord::kShift | Chord::kAlt };
		Expect("two modifiers beat one", Slot::kDual, binds, kMouse5, Chord::kShift | Chord::kAlt);
		Expect("one modifier alone", Slot::kRight, binds, kMouse5, Chord::kShift);
	}

	// Unbound rows never take a press, even when the code is 0.
	{
		auto binds = Empty();
		Expect("all unbound, code 0", std::nullopt, binds, 0, Chord::kNone);
		Expect("all unbound, mouse 5", std::nullopt, binds, kMouse5, Chord::kNone);
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
