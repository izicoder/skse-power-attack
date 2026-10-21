#include "PCH.h"
#include "Attack.h"
#include "Config.h"
#include "Log.h"
#include "Selection.h"

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

		// Vanilla BGSAction records, looked up by form id. Actions are what the input handlers
		// themselves submit, so this goes through the same animation/state machinery as a real
		// button press instead of forcing an animation event by hand.
		//
		// Vanilla has a full set for each attack: a normal and a power attack for the right hand,
		// the left hand and the two-weapon spin. The row names which set to use; nothing here
		// looks at what the player is holding.
		struct Actions
		{
			RE::BGSAction* attack{ nullptr };  // normal attack, only the jump/block fallback uses it
			RE::BGSAction* power{ nullptr };
		};

		// Indexed by Selection::Choice, so adding a case means adding one row here and one branch
		// in Selection.h - never a second copy of the rule.
		static_assert(static_cast<int>(Selection::Choice::kRight) == 0);
		static_assert(static_cast<int>(Selection::Choice::kLeft) == 1);
		static_assert(static_cast<int>(Selection::Choice::kDual) == 2);
		std::array<Actions, 3> s_actions{};

		// Actor::ProcessAction -- RELOCATION_ID(40551, 41557). Called on the task interface (main
		// thread) with a freshly built TESActionData.
		using ProcessActionFn = bool(RE::TESActionData*);
		REL::Relocation<ProcessActionFn> s_processAction{ RELOCATION_ID(40551, 41557) };

		const Actions& For(Selection::Choice a_choice)
		{
			return s_actions[static_cast<std::size_t>(a_choice)];
		}

		// What the tap resolved to: the action pair plus the label for the log.
		struct Pick
		{
			const Actions& actions;
			const char*    name;
		};

		// The row names the action and nothing else is consulted (Selection::ForSlot, unit tested).
		Pick Resolve(Binds::Slot a_slot)
		{
			const auto choice = Selection::ForSlot(a_slot);
			return { For(choice), Selection::Describe(choice) };
		}

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
		// normal attack of the same hand first and the power attack a moment later. The short
		// wait leaves the normal attack's window before the power attack lands.
		//
		// The wait is a chain of main-thread tasks, not a thread: a task checks the clock and
		// re-posts itself until the delay is up. The plugin therefore starts no threads at all,
		// and nothing ever calls into the engine from one.
		constexpr auto kFallbackDelay = std::chrono::milliseconds(200);

		void ScheduleDelayedPowerAttack(RE::Actor* a_actor, RE::BGSAction* a_powerAttack, std::uint64_t a_generation, std::chrono::steady_clock::time_point a_due)
		{
			if (!s_task) {
				PAE("attack: no task interface, the delayed power attack is dropped");
				return;
			}

			s_task->AddTask([a_actor, a_powerAttack, a_generation, a_due]() {
				try {
					if (s_generation.load(std::memory_order_acquire) != a_generation) {
						PA("attack: a save was loaded during the fallback delay, dropping it");
						return;
					}
					if (std::chrono::steady_clock::now() < a_due) {
						ScheduleDelayedPowerAttack(a_actor, a_powerAttack, a_generation, a_due);  // next frame
						return;
					}
					PerformAction(a_powerAttack, a_actor, "tap (after the jump/block attack)");
				} catch (const std::exception& e) {
					PAE("attack: the delayed power attack threw: {}", e.what());
				} catch (...) {
					PAE("attack: the delayed power attack threw a non-std exception");
				}
			});
		}

		// The resolved action is carried through the wait instead of being resolved again: the
		// hands can change in those 200 ms (an equip, a stagger) and the delayed attack has to be
		// the one the tap chose.
		void DelayedPowerAttack(RE::Actor* a_actor, RE::BGSAction* a_powerAttack)
		{
			const auto generation = s_generation.load(std::memory_order_acquire);
			PA("attack: power attack queued for {} ms after the normal attack (generation {})",
				kFallbackDelay.count(), generation);
			ScheduleDelayedPowerAttack(a_actor, a_powerAttack, generation, std::chrono::steady_clock::now() + kFallbackDelay);
		}

		RE::BGSAction* Lookup(RE::FormID a_id, const char* a_name)
		{
			auto* action = RE::TESForm::LookupByID<RE::BGSAction>(a_id);
			if (!action) {
				PAE("attack: {} (0x{:05X}) was not found - is Skyrim.esm loaded?", a_name, a_id);
			}
			return action;
		}
	}

	void Init(const SKSE::TaskInterface* a_task)
	{
		s_task = a_task;
		PAI("attack: task interface = 0x{:X}", reinterpret_cast<std::uintptr_t>(a_task));

		if (const auto address = s_processAction.address(); address == 0) {
			PAE("attack: ProcessAction has no address on this runtime, the tap will do nothing");
		}

		s_actions[static_cast<std::size_t>(Selection::Choice::kRight)] = {
			Lookup(0x13005, "ActionRightAttack"), Lookup(0x13383, "ActionRightPowerAttack")
		};
		s_actions[static_cast<std::size_t>(Selection::Choice::kLeft)] = {
			Lookup(0x13004, "ActionLeftAttack"), Lookup(0x2E2F6, "ActionLeftPowerAttack")
		};
		s_actions[static_cast<std::size_t>(Selection::Choice::kDual)] = {
			Lookup(0x50C96, "ActionDualAttack"), Lookup(0x2E2F7, "ActionDualPowerAttack")
		};

		const auto report = [](Selection::Choice a_choice) {
			const auto& a = For(a_choice);
			return std::format("{}={} attack / {} power",
				Selection::Name(a_choice),
				a.attack ? a.attack->GetFormID() : 0,
				a.power ? a.power->GetFormID() : 0);
		};
		PAI("attack: {} | {} | {}", report(Selection::Choice::kRight), report(Selection::Choice::kLeft), report(Selection::Choice::kDual));
	}

	void PowerAttack(const char* a_reason, Binds::Slot a_slot)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			PAE("attack: no player, ignoring the tap");
			return;
		}

		const auto pick = Resolve(a_slot);
		if (!pick.actions.power) {
			PAE("attack: no {} action resolved, ignoring the tap", pick.name);
			return;
		}

		bool isJumping = false;
		bool isBlocking = false;
		player->GetGraphVariableBool("bInJumpState", isJumping);
		player->GetGraphVariableBool("IsBlocking", isBlocking);
		PA("attack: {} (jumping={} blocking={}) -> {}", a_reason, isJumping, isBlocking, pick.name);

		if (isJumping || isBlocking) {
			PerformAction(pick.actions.attack, player, "tap while jumping/blocking, normal attack first");
			DelayedPowerAttack(player, pick.actions.power);
			return;
		}

		PerformAction(pick.actions.power, player, pick.name);
	}

	void NormalAttack(const char* a_reason, Binds::Slot a_slot)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			PAE("attack: no player, ignoring the normal attack");
			return;
		}

		const auto pick = Resolve(a_slot);
		if (!pick.actions.attack) {
			PAE("attack: no {} normal action resolved, ignoring it", pick.name);
			return;
		}

		PA("attack: {} -> {}", a_reason, pick.name);
		PerformAction(pick.actions.attack, player, a_reason);
	}

	void Reset()
	{
		const auto generation = s_generation.fetch_add(1, std::memory_order_release) + 1;
		PA("attack: state reset (generation {}), a delayed power attack still waiting is dropped", generation);
	}
}
