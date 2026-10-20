#pragma once

#include "PCH.h"

namespace SPA::Attack
{
	// Resolves the vanilla action records and keeps the task interface. Call once at kDataLoaded.
	void Init(const SKSE::TaskInterface* a_task);

	// One click: performs a right-hand power attack on the player.
	void PowerAttack(const char* a_reason);

	// Forgets per-save state: a delayed power attack still waiting (the jump/block fallback)
	// is dropped instead of running into the save that is loading. Called before a save loads
	// and on a new game.
	void Reset();
}
