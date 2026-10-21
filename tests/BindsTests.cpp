// Unit tests for bind assignment: what happens when a key is already in use. One key must never
// be two roles at once, and the last key the user binds takes it.
//
//   cmake --build build/vs2022 --target spa_tests && build/vs2022\spa_tests.exe

#include "Binds.h"

#include <cstdio>

namespace
{
	using SPA::Binds::Target;

	int failures = 0;

	void Check(bool a_condition, const char* a_what)
	{
		if (!a_condition) {
			++failures;
			std::printf("FAIL %s\n", a_what);
		}
	}

	struct Keys
	{
		int hotkey{ 260 };
		int leftHotkey{ SPA::Binds::kNoKey };
		int modifierKey{ SPA::Binds::kNoKey };

		bool Assign(Target a_target, int a_code) { return SPA::Binds::Assign(hotkey, leftHotkey, modifierKey, a_target, a_code); }
	};

	// Every key bound at most once, which is the invariant the whole rule exists for.
	void ExpectUnique(const Keys& a_keys, const char* a_what)
	{
		const int codes[] = { a_keys.hotkey, a_keys.leftHotkey, a_keys.modifierKey };
		for (int i = 0; i < 3; ++i) {
			if (codes[i] == SPA::Binds::kNoKey) {
				continue;
			}
			for (int j = i + 1; j < 3; ++j) {
				if (codes[i] == codes[j]) {
					++failures;
					std::printf("FAIL %s: key %d bound twice\n", a_what, codes[i]);
				}
			}
		}
	}
}

int main()
{
	// A key nobody uses just gets bound.
	{
		Keys keys;
		Check(keys.Assign(Target::kLeftHotkey, 42), "free key is accepted");
		Check(keys.leftHotkey == 42, "free key lands on the second key");
		Check(keys.hotkey == 260, "free key leaves the main key alone");
	}

	// The main key takes a key away from the second key: one key, one role.
	{
		Keys keys;
		keys.Assign(Target::kLeftHotkey, 42);
		Check(keys.Assign(Target::kHotkey, 42), "main key takes the second key's key");
		Check(keys.hotkey == 42, "main key holds it");
		Check(keys.leftHotkey == SPA::Binds::kNoKey, "second key lets go");
		ExpectUnique(keys, "main steals from second");
	}

	// The main key takes a key away from the modifier too.
	{
		Keys keys;
		keys.Assign(Target::kModifier, 42);
		Check(keys.Assign(Target::kHotkey, 42), "main key takes the modifier's key");
		Check(keys.hotkey == 42, "main key holds it");
		Check(keys.modifierKey == SPA::Binds::kNoKey, "modifier lets go");
	}

	// The second key takes a key away from the modifier.
	{
		Keys keys;
		keys.Assign(Target::kModifier, 42);
		Check(keys.Assign(Target::kLeftHotkey, 42), "second key takes the modifier's key");
		Check(keys.leftHotkey == 42, "second key holds it");
		Check(keys.modifierKey == SPA::Binds::kNoKey, "modifier lets go");
		ExpectUnique(keys, "second steals from modifier");
	}

	// The main key is mandatory, so nothing takes its key: the tap would have no key at all.
	{
		Keys keys;
		Check(!keys.Assign(Target::kLeftHotkey, 260), "second key cannot take the main key's key");
		Check(keys.hotkey == 260, "main key keeps it");
		Check(keys.leftHotkey == SPA::Binds::kNoKey, "second key stays unbound");

		Check(!keys.Assign(Target::kModifier, 260), "modifier cannot take the main key's key");
		Check(keys.modifierKey == SPA::Binds::kNoKey, "modifier stays unbound");
	}

	// Binding a key to itself changes nothing and is not a conflict.
	{
		Keys keys;
		Check(keys.Assign(Target::kHotkey, 260), "main key to its own key is accepted");
		Check(keys.hotkey == 260, "main key unchanged");
	}

	// "No key" is not an assignment: Clear() writes it directly.
	{
		Keys keys;
		Check(!keys.Assign(Target::kLeftHotkey, SPA::Binds::kNoKey), "unbinding through Assign is refused");
	}

	// Whatever order keys are bound in, the invariant holds.
	{
		Keys keys;
		const Target targets[] = { Target::kLeftHotkey, Target::kModifier, Target::kHotkey, Target::kModifier, Target::kLeftHotkey };
		const int codes[] = { 42, 260, 42, 43, 43 };
		for (int i = 0; i < 5; ++i) {
			keys.Assign(targets[i], codes[i]);
			ExpectUnique(keys, "sequence");
		}
	}

	if (failures == 0) {
		std::printf("ok: bind tests passed\n");
		return 0;
	}
	std::printf("%d failure(s)\n", failures);
	return 1;
}
