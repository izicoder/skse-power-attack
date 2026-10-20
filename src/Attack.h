#pragma once

#include "PCH.h"

namespace SPA::Attack
{
	// Resolves the vanilla action records and keeps the task interface. Call once at kDataLoaded.
	void Init(const SKSE::TaskInterface* a_task);

	// One click: performs a right-hand power attack on the player.
	void PowerAttack(const char* a_reason);

	// Forgets per-save state (nothing is queued across a load, a half-finished attack is not
	// resumed). Called on the loading screen / before a save is loaded.
	void Reset();
}
