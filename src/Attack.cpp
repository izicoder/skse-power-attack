#include "PCH.h"
#include "Attack.h"
#include "Config.h"
#include "Log.h"

namespace SPA::Attack
{
	namespace
	{
		const SKSE::TaskInterface* s_task = nullptr;

		// Bumped whenever a save is loaded or a new game starts. The delayed power attack below
		// runs on its own thread, so the generation it was scheduled under decides whether it
		// still belongs to the same game: a save loaded inside the 200 ms wait drops it instead
		// of acting on a world that is being torn down.
		std::atomic<std::uint64_t> s_generation{ 0 };

		// Vanilla BGSAction records, looked up by form id: "AttackRight" and
		// "PowerAttackRight". Actions are what the input handlers themselves submit, so this goes
		// through the same animation/state machinery as a real button press instead of forcing an
		// animation event by hand.
		RE::BGSAction* s_rightAttack = nullptr;
		RE::BGSAction* s_rightPowerAttack = nullptr;

		// Actor::ProcessAction -- RELOCATION_ID(40551, 41557). Called on the task interface (main
		// thread) with a freshly built TESActionData.
		using ProcessActionFn = bool(RE::TESActionData*);
		REL::Relocation<ProcessActionFn> s_processAction{ RELOCATION_ID(40551, 41557) };

		void PerformAction(RE::BGSAction* a_action, RE::Actor* a_actor, const char* a_reason)
		{
			if (!s_task || !a_action || !a_actor) {
				PAE("attack: cannot perform '{}' (task={} action={} actor={})", a_reason,
					s_task != nullptr, static_cast<const void*>(a_action), static_cast<const void*>(a_actor));
				return;
			}

			// The action is built on the task thread: TESActionData::Create() allocates from the
			// game heap and the actor's animation state is not safe to submit from an input event.
			s_task->AddTask([a_action, a_actor, reason = std::string{ a_reason }]() {
				try {
					std::unique_ptr<RE::TESActionData> data{ RE::TESActionData::Create() };
					if (!data) {
						PAE("attack: TESActionData::Create() returned null, nothing performed ({})", reason);
						return;
					}
					data->source = RE::NiPointer<RE::TESObjectREFR>(a_actor);
					data->action = a_action;
					const bool performed = s_processAction(data.get());
					PA("attack: '{}' -> action 0x{:08X} performed={}", reason, a_action->GetFormID(), performed);
				} catch (const std::exception& e) {
					PAE("attack: performing '{}' threw: {}", reason, e.what());
				} catch (...) {
					PAE("attack: performing '{}' threw a non-std exception", reason);
				}
			});
		}

		// Jumping and blocking both eat the power attack action, so those two cases get the
		// vanilla right attack first and the power attack a moment later. The short wait leaves
		// the normal attack's window before the power attack lands.
		//
		// The wait is a chain of main-thread tasks, not a thread: a task checks the clock and
		// re-posts itself until the delay is up. The plugin therefore starts no threads at all,
		// and nothing ever calls into the engine from one.
		constexpr auto kFallbackDelay = std::chrono::milliseconds(200);

		void ScheduleDelayedPowerAttack(RE::Actor* a_actor, std::uint64_t a_generation, std::chrono::steady_clock::time_point a_due)
		{
			if (!s_task) {
				PAE("attack: no task interface, the delayed power attack is dropped");
				return;
			}

			s_task->AddTask([a_actor, a_generation, a_due]() {
				try {
					if (s_generation.load(std::memory_order_acquire) != a_generation) {
						PA("attack: a save was loaded during the fallback delay, dropping it");
						return;
					}
					if (std::chrono::steady_clock::now() < a_due) {
						ScheduleDelayedPowerAttack(a_actor, a_generation, a_due);  // next frame
						return;
					}
					PerformAction(s_rightPowerAttack, a_actor, "tap (after the jump/block attack)");
				} catch (const std::exception& e) {
					PAE("attack: the delayed power attack threw: {}", e.what());
				} catch (...) {
					PAE("attack: the delayed power attack threw a non-std exception");
				}
			});
		}

		void DelayedPowerAttack(RE::Actor* a_actor)
		{
			const auto generation = s_generation.load(std::memory_order_acquire);
			PA("attack: power attack queued for {} ms after the normal attack (generation {})",
				kFallbackDelay.count(), generation);
			ScheduleDelayedPowerAttack(a_actor, generation, std::chrono::steady_clock::now() + kFallbackDelay);
		}
	}

	void Init(const SKSE::TaskInterface* a_task)
	{
		s_task = a_task;
		PAI("attack: task interface = 0x{:X}", reinterpret_cast<std::uintptr_t>(a_task));

		if (const auto address = s_processAction.address(); address == 0) {
			PAE("attack: ProcessAction has no address on this runtime, the tap will do nothing");
		}

		s_rightAttack = RE::TESForm::LookupByID<RE::BGSAction>(0x13005);
		s_rightPowerAttack = RE::TESForm::LookupByID<RE::BGSAction>(0x13383);
		PAI("attack: AttackRight = 0x{:X}, PowerAttackRight = 0x{:X}",
			reinterpret_cast<std::uintptr_t>(s_rightAttack), reinterpret_cast<std::uintptr_t>(s_rightPowerAttack));

		if (!s_rightPowerAttack) {
			PAE("attack: PowerAttackRight (0x00013383) was not found - is Skyrim.esm loaded?");
		}
	}

	void PowerAttack(const char* a_reason)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || !s_rightPowerAttack) {
			PAE("attack: no player or no power attack action, ignoring the tap");
			return;
		}

		bool isJumping = false;
		bool isBlocking = false;
		player->GetGraphVariableBool("bInJumpState", isJumping);
		player->GetGraphVariableBool("IsBlocking", isBlocking);
		PA("attack: {} (jumping={} blocking={})", a_reason, isJumping, isBlocking);

		if (isJumping || isBlocking) {
			PerformAction(s_rightAttack, player, "tap while jumping/blocking, normal attack first");
			DelayedPowerAttack(player);
			return;
		}

		PerformAction(s_rightPowerAttack, player, a_reason);
	}

	void Reset()
	{
		const auto generation = s_generation.fetch_add(1, std::memory_order_release) + 1;
		PA("attack: state reset (generation {}), a delayed power attack still waiting is dropped", generation);
	}
}
