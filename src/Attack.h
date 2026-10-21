#pragma once

#include "PCH.h"
#include "Selection.h"

namespace SPA::Attack
{
	// Resolves the vanilla action records and keeps the task interface. Call once at kDataLoaded.
	void Init(const SKSE::TaskInterface* a_task);

	// One tap: submits the power attack the player's hands call for (or the fallback hand's, with
	// nothing equipped), after the jump/block fallback when the engine would swallow it.
	void PowerAttack(const char* a_reason, Selection::Choice a_fallback);

	// Forgets per-save state: a delayed power attack still waiting (the jump/block fallback)
	// is dropped instead of running into the save that is loading. Called before a save loads
	// and on a new game.
	void Reset();
}
