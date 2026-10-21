#include "PCH.h"
#include "Input.h"
#include "Attack.h"
#include "Config.h"
#include "Log.h"

namespace SPA
{
	namespace Keys
	{
		std::uint32_t Encode(RE::INPUT_DEVICE a_device, std::uint32_t a_idCode)
		{
			switch (a_device) {
			case RE::INPUT_DEVICE::kKeyboard:
				return a_idCode & 0xFF;
			case RE::INPUT_DEVICE::kMouse:
				return kMouseOffset + (a_idCode & 0xFF);
			case RE::INPUT_DEVICE::kGamepad:
				{
					// ButtonEvent::idCode for a gamepad is the XInput mask, except for the two
					// triggers, which use their own small ids. Map them onto SKSE's gamepad order.
					static constexpr std::uint32_t kMasks[] = {
						0x0001, 0x0002, 0x0004, 0x0008,  // dpad up / down / left / right
						0x0010, 0x0020,                  // start / back
						0x0040, 0x0080,                  // left / right thumb click
						0x0100, 0x0200,                  // left / right shoulder
						0x1000, 0x2000, 0x4000, 0x8000,  // A / B / X / Y
						0x0009, 0x000A                   // left / right trigger
					};
					for (std::size_t i = 0; i < std::size(kMasks); ++i) {
						if (kMasks[i] == a_idCode) {
							return kGamepadOffset + static_cast<std::uint32_t>(i);
						}
					}
					return kUnknown;
				}
			default:
				return kUnknown;
			}
		}

		std::string Describe(int a_code)
		{
			if (a_code < 0) {
				return "None";
			}
			if (a_code < static_cast<int>(kMouseOffset)) {
				if (auto name = SKSE::InputMap::GetKeyboardKeyName(static_cast<std::uint32_t>(a_code)); !name.empty()) {
					return name;
				}
				return std::format("Key 0x{:02X}", a_code);
			}
			if (a_code < static_cast<int>(kGamepadOffset)) {
				switch (a_code - static_cast<int>(kMouseOffset)) {
				case 0: return "Left Mouse";
				case 1: return "Right Mouse";
				case 2: return "Middle Mouse";
				default: return std::format("Mouse {}", a_code - static_cast<int>(kMouseOffset) + 1);
				}
			}
			if (a_code <= kMaxKeyCode) {
				return SKSE::InputMap::GetGamepadButtonName(static_cast<std::uint32_t>(a_code));
			}
			return std::format("Key {}", a_code);
		}

		std::string DescribeChord(const Chord::Bind& a_bind)
		{
			if (!a_bind.IsBound()) {
				return "None";
			}
			return Chord::ModsPrefix(a_bind.mods) + Describe(a_bind.key);
		}
	}

	namespace Input
	{
		namespace
		{
			// One waiting press per box. The code is stored so a release only finishes the tap its
			// own press started (two boxes may listen on the same key with different modifiers),
			// and "taken" remembers that the game did not get this press.
			struct Tap
			{
				std::atomic<std::int64_t> pressTicks{ 0 };
				std::atomic<int>          code{ Chord::kNoKey };
				std::atomic<bool>         taken{ false };
			};

			std::array<Tap, Binds::kSlotCount> s_taps{};
			std::atomic<unsigned> s_heldMods{ 0 };
			std::atomic<int>      s_clicks{ 0 };

			std::int64_t Now()
			{
				return std::chrono::steady_clock::now().time_since_epoch().count();
			}

			RE::INPUT_DEVICE DeviceOf(int a_code)
			{
				const auto code = static_cast<std::uint32_t>(a_code);
				if (code < Keys::kMouseOffset) {
					return RE::INPUT_DEVICE::kKeyboard;
				}
				if (code < Keys::kGamepadOffset) {
					return RE::INPUT_DEVICE::kMouse;
				}
				return RE::INPUT_DEVICE::kGamepad;
			}

			// True when the game itself attacks with this key. That press is taken even for a bare
			// chord, because a tap on it would otherwise swing normally and power attack.
			bool IsGameAttackKey(int a_code)
			{
				auto* map = RE::ControlMap::GetSingleton();
				if (!map) {
					return false;
				}
				// "Attack" is the gameplay event id in Skyrim's control map; the returned code uses
				// the same numbering as ButtonEvent, i.e. the one Encode() produced.
				return map->GetMappedKey("Attack"sv, DeviceOf(a_code)) == static_cast<std::uint32_t>(a_code);
			}

			bool IsRidingHorse(RE::Actor* a_actor)
			{
				return a_actor->AsActorState()->actorState1.sitSleepState == RE::SIT_SLEEP_STATE::kRidingMount;
			}

			// A press that misses is silent unless the key is bound somewhere: then only the
			// modifiers failed to line up, which is worth a line when someone is chasing a chord.
			void LogNearMiss(const Settings& a_settings, int a_code, unsigned a_held)
			{
				for (std::size_t i = 0; i < Binds::kSlotCount; ++i) {
					const auto& bind = a_settings.binds[i];
					if (!bind.IsBound() || bind.key != a_code) {
						continue;
					}
					PAI("input: '{}' down with modifiers 0x{:X}, but the {} row wants '{}' - no row matched",
						Keys::Describe(a_code), a_held, Binds::Name(static_cast<Binds::Slot>(i)), Keys::DescribeChord(bind));
					return;
				}
			}

			// Anything that makes a power attack meaningless or unsafe to submit. The reason is
			// only logged when a tap actually arrives, so a menu or a horse cannot flood the log.
			const char* GateReason(const Settings& a_settings)
			{
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (!player) {
					return "no player";
				}
				if (IsRidingHorse(player)) {
					return "on a horse";
				}
				if (player->IsInKillMove()) {
					return "in a killmove";
				}
				if (player->AsActorState()->actorState1.sitSleepState != RE::SIT_SLEEP_STATE::kNormal) {
					return "sitting or sleeping";
				}

				auto* ui = RE::UI::GetSingleton();
				if (ui && ui->GameIsPaused()) {
					return "game paused";
				}
				if (ui && ui->IsMenuOpen("Dialogue Menu"sv)) {
					return "in dialogue";
				}

				// Bows, crossbows and staves have no power attack; the tap is theirs to handle.
				if (auto* form = player->GetEquippedObject(false); form && form->IsWeapon()) {
					if (auto* weapon = form->As<RE::TESObjectWEAP>(); weapon && (weapon->IsBow() || weapon->IsStaff() || weapon->IsCrossbow())) {
						return "bow, crossbow or staff equipped";
					}
				}

				if (!a_settings.allowZeroStamina) {
					auto* owner = player->AsActorValueOwner();
					if (!owner || owner->GetActorValue(RE::ActorValue::kStamina) <= 0.0f) {
						return "no stamina left";
					}
				}

				return nullptr;
			}

			class Sink : public RE::BSTEventSink<RE::InputEvent*>
			{
			public:
				static Sink* GetSingleton()
				{
					static Sink singleton;
					return &singleton;
				}

				RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_events, RE::BSTEventSource<RE::InputEvent*>*) override
				{
					if (!a_events || !*a_events) {
						return RE::BSEventNotifyControl::kContinue;
					}

					const auto& settings = Config::Cached();
					try {
						// Modifiers first, over the whole batch: Ctrl or Alt pressed in the same poll
						// as the chord key can be dispatched after it, and then the chord would miss.
						// They are never a chord's own key, so their game action is never touched.
						for (auto* event = *a_events; event; event = event->next) {
							auto* button = event->AsButtonEvent();
							if (!button) {
								continue;
							}
							const auto code = static_cast<int>(Keys::Encode(button->device.get(), button->GetIDCode()));
							if (const auto modBit = Chord::ModBitFor(code); modBit != Chord::kNone) {
								if (button->Value() > 0.0f) {
									s_heldMods.fetch_or(modBit, std::memory_order_acq_rel);
								} else {
									s_heldMods.fetch_and(~modBit, std::memory_order_acq_rel);
								}
								PAI("input: modifier '{}' {}, held now 0x{:X}", Keys::Describe(code),
									button->Value() > 0.0f ? "down" : "up", s_heldMods.load(std::memory_order_acquire));
							}
						}

						for (auto* event = *a_events; event; event = event->next) {
							auto* button = event->AsButtonEvent();
							if (!button) {
								continue;
							}

							const auto code = static_cast<int>(Keys::Encode(button->device.get(), button->GetIDCode()));
							if (code == static_cast<int>(Keys::kUnknown)) {
								continue;
							}
							if (Chord::IsModifier(code)) {
								continue;  // already applied above
							}

							// ButtonEvent semantics: the first press has heldDownSecs == 0, every
							// repeat while held has it > 0, and the release has value == 0.
							const bool down = button->Value() > 0.0f && button->HeldDuration() <= 0.0f;
							if (button->Value() > 0.0f) {
								if (!down) {
									continue;  // a repeat while held, not a new press
								}

								const auto held = s_heldMods.load(std::memory_order_acquire);
								const auto slot = Binds::Resolve(settings.binds, code, held);
								if (!slot) {
									LogNearMiss(settings, code, held);
									continue;
								}

								auto&       tap = s_taps[Binds::Index(*slot)];
								const auto& bind = settings.binds[Binds::Index(*slot)];

								// Vanilla ignores modifiers, so a chord that names one is a press the
								// player meant for the plugin: the game would otherwise do the
								// unmodified action (right mouse blocks, E loots). A bare chord is
								// taken only when the game attacks with that key. A gate means the
								// game keeps its press: a drawn bow has to fire.
								const bool take = (bind.mods != Chord::kNone || IsGameAttackKey(code)) &&
									GateReason(settings) == nullptr;

								tap.code.store(code, std::memory_order_release);
								tap.taken.store(take, std::memory_order_release);
								tap.pressTicks.store(Now(), std::memory_order_release);

								// A press meant for us is blanked in place instead of being answered
								// with kStop: kStop drops the whole batch, and a release in that same
								// batch (a shield, a block, a movement key) would never reach the
								// engine - a swallowed release keeps its held state latched, which is
								// the stuck shield. Blanking the value leaves the event lists intact
								// for the other sinks: the game sees this button as already released.
								if (take) {
									button->GetRuntimeData().value = 0.0f;
								}

								PAI("input: '{}' down ({} row, modifiers 0x{:X}){}: release within {} ms to power attack",
									Keys::DescribeChord(bind), Binds::Name(*slot), held,
									take ? ", the game does not get this press" : "", settings.tapWindowMs);
								continue;
							}

							// Release: the row still waiting for this exact key, if any.
							std::optional<Binds::Slot> slot{};
							for (std::size_t i = 0; i < Binds::kSlotCount; ++i) {
								if (s_taps[i].code.load(std::memory_order_acquire) == code &&
									s_taps[i].pressTicks.load(std::memory_order_acquire) != 0) {
									slot = static_cast<Binds::Slot>(i);
									break;
								}
							}
							if (!slot) {
								PA("input: '{}' released without a recorded press, ignoring", Keys::Describe(code));
								continue;
							}

							auto&      tap = s_taps[Binds::Index(*slot)];
							const auto pressed = tap.pressTicks.exchange(0, std::memory_order_acq_rel);
							// The press was taken, the release never is. Block is a held button:
							// the engine keeps its own "held" flag and only a release clears it, so
							// swallowing the release left a shield up until the next key press.
							const bool took = tap.taken.exchange(false, std::memory_order_acq_rel);
							tap.code.store(Chord::kNoKey, std::memory_order_release);

							const auto& bind = settings.binds[Binds::Index(*slot)];

							// The engine reports how long the button was down; the timestamp taken
							// at the press covers the case where it does not.
							const auto   held = button->HeldDuration();
							const auto   ticksPerSecond = static_cast<double>(std::chrono::steady_clock::period::den) /
								static_cast<double>(std::chrono::steady_clock::period::num);
							const double heldSeconds = held > 0.0f
								? static_cast<double>(held)
								: (pressed != 0 ? static_cast<double>(Now() - pressed) / ticksPerSecond : 0.0);

							// Whole milliseconds on both sides, so the comparison matches the number
							// shown in the menu and the JSON.
							const int heldMs = static_cast<int>(std::lround(heldSeconds * 1000.0));

							// The press was taken from the game, so a tap that does not become a
							// power attack has to hand its swing back - otherwise holding a bound
							// key would do nothing at all.
							const auto handBack = [took, slot](const char* a_reason) {
								if (took) {
									Attack::NormalAttack(a_reason, *slot);
								}
							};

							if (heldMs > settings.tapWindowMs) {
								PAI("input: '{}' held {} ms > {} ms window, no power attack (hold)",
									Keys::DescribeChord(bind), heldMs, settings.tapWindowMs);
								handBack("the tap key was held, normal attack");
								continue;
							}

							if (const char* reason = GateReason(settings)) {
								PA("input: tap ignored ({})", reason);
								handBack("the tap was blocked, normal attack");
								continue;
							}

							const auto click = s_clicks.fetch_add(1, std::memory_order_relaxed) + 1;
							PAI("input: tap #{} on '{}' ({} row, held {} ms) -> power attack",
								click, Keys::DescribeChord(bind), Binds::Name(*slot), heldMs);
							Attack::PowerAttack("tap", *slot);
						}
					} catch (const std::exception& e) {
						PAE("input: sink threw: {}", e.what());
					} catch (...) {
						PAE("input: sink threw a non-std exception");
					}

					// Never kStop: it stops the whole batch for every later sink, and swallowing a
					// release is what left a shield up until the next key press. A taken press is
					// blanked in place above, so only that button is hidden from the game.
					return RE::BSEventNotifyControl::kContinue;
				}
			};

			// The game keeps dispatching input while a menu is up, but a modifier held when the menu
			// opened never gets its release: the menu eats it. Clearing on open means the tap after
			// a menu starts from "nothing held" instead of needing the modifier pressed twice.
			class MenuSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
			{
			public:
				static MenuSink* GetSingleton()
				{
					static MenuSink singleton;
					return &singleton;
				}

				RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
				{
					if (a_event && a_event->opening) {
						PA("input: menu '{}' opened, dropping the modifier state", a_event->menuName.c_str());
						ResetState();
					}
					return RE::BSEventNotifyControl::kContinue;
				}
			};
		}

		unsigned HeldMods()
		{
			return s_heldMods.load(std::memory_order_acquire);
		}

		void ResetState()
		{
			s_heldMods.store(Chord::kNone, std::memory_order_release);
			for (auto& tap : s_taps) {
				// A press still waiting for its release is dropped: the release would otherwise
				// arrive after a load and fire a power attack nobody asked for.
				tap.pressTicks.store(0, std::memory_order_release);
				tap.code.store(Chord::kNoKey, std::memory_order_release);
				tap.taken.store(false, std::memory_order_release);
			}
			PA("input: modifier and pending tap state cleared");
		}

		void Register()
		{
			// Logged once so a support log shows whether IsGameAttackKey() can recognise the key:
			// the game reports its attack binding in the same numbering as ButtonEvent, and this is
			// the evidence if a tap ever fails to cancel the normal attack.
			if (auto* map = RE::ControlMap::GetSingleton()) {
				const auto mouse = map->GetMappedKey("Attack"sv, RE::INPUT_DEVICE::kMouse);
				const auto keyboard = map->GetMappedKey("Attack"sv, RE::INPUT_DEVICE::kKeyboard);
				const auto gamepad = map->GetMappedKey("Attack"sv, RE::INPUT_DEVICE::kGamepad);
				PAI("input: game attack key: mouse={} '{}', keyboard={} '{}', gamepad={} '{}'",
					mouse, Keys::Describe(static_cast<int>(mouse)),
					keyboard, Keys::Describe(static_cast<int>(keyboard)),
					gamepad, Keys::Describe(static_cast<int>(gamepad)));
			} else {
				PAW("input: no control map, a bound attack key will not cancel the normal attack");
			}

			if (auto* manager = RE::BSInputDeviceManager::GetSingleton()) {
				manager->AddEventSink(Sink::GetSingleton());
				const auto& settings = Config::Cached();
				PAI("input: input sink registered (right '{}', left '{}', dual '{}')",
					Keys::DescribeChord(settings.binds[0]), Keys::DescribeChord(settings.binds[1]),
					Keys::DescribeChord(settings.binds[2]));
			} else {
				PAE("input: BSInputDeviceManager singleton is null, the tap will not work");
			}

			if (auto* ui = RE::UI::GetSingleton()) {
				if (auto* source = ui->GetEventSource<RE::MenuOpenCloseEvent>()) {
					source->AddEventSink(MenuSink::GetSingleton());
					PAI("input: menu sink registered (modifier state is dropped when a menu opens)");
				} else {
					PAW("input: no menu event source, a held modifier survives menus");
				}
			} else {
				PAW("input: no UI singleton, a held modifier survives menus");
			}
		}
	}
}
