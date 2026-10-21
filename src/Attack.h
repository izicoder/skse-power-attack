#pragma once

#include "PCH.h"
#include "Selection.h"

namespace SPA::Attack
{
	// Resolves the vanilla action records and keeps the task interface. Call once at kDataLoaded.
	void Init(const SKSE::TaskInterface* a_task);

	// One tap: submits the power attack the bound box asks for. The auto box follows the hands,
	// and with nothing equipped it swings right. The jump/block fallback runs first when the
	// engine would swallow the action.
	void PowerAttack(const char* a_reason, Binds::Slot a_slot);

	// The plain attack of the same box. Only used when a press was taken away from the game and
	// the tap did not become a power attack, so a held or blocked key still swings.
	void NormalAttack(const char* a_reason, Binds::Slot a_slot);

	// Forgets per-save state: a delayed power attack still waiting (the jump/block fallback)
	// is dropped instead of running into the save that is loading. Called before a save loads
	// and on a new game.
	void Reset();
}
