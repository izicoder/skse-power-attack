#include "PCH.h"
#include "Input.h"
#include "Attack.h"
#include "Config.h"
#include "Log.h"

namespace OCPA
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
	}

	namespace Input
	{
		namespace
		{
			std::atomic<bool> s_modifierHeld{ false };
			std::atomic<int>  s_clicks{ 0 };

			bool IsRidingHorse(RE::Actor* a_actor)
			{
				return a_actor->AsActorState()->actorState1.sitSleepState == RE::SIT_SLEEP_STATE::kRidingMount;
			}

			// Anything that makes a power attack meaningless or unsafe to submit. The reason is
			// only logged when a click actually arrives, so a menu or a horse cannot flood the log.
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

				// Bows, crossbows and staves have no power attack; the click is theirs to handle.
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
						for (auto* event = *a_events; event; event = event->next) {
							auto* button = event->AsButtonEvent();
							if (!button) {
								continue;
							}

							const auto code = static_cast<int>(Keys::Encode(button->device.get(), button->GetIDCode()));
							if (code == static_cast<int>(Keys::kUnknown)) {
								continue;
							}

							// ButtonEvent semantics: the first press has heldDownSecs == 0, every
							// repeat while held has it > 0, and the release has value == 0.
							const bool down = button->Value() > 0.0f && button->HeldDuration() <= 0.0f;
							const bool up = button->Value() <= 0.0f && button->HeldDuration() > 0.0f;

							if (settings.modifierKey != kNoKey) {
								if (code == settings.modifierKey && (down || up)) {
									s_modifierHeld.store(down, std::memory_order_release);
									PA("input: modifier '{}' {}", Keys::Describe(code), down ? "down" : "up");
								}
							}

							if (code != settings.hotkey || !down) {
								continue;
							}

							if (settings.modifierKey != kNoKey && !s_modifierHeld.load(std::memory_order_acquire)) {
								PA("input: '{}' pressed without the modifier '{}' held, ignoring",
									Keys::Describe(code), Keys::Describe(settings.modifierKey));
								continue;
							}

							if (const char* reason = GateReason(settings)) {
								PA("input: click ignored ({})", reason);
								continue;
							}

							const auto click = s_clicks.fetch_add(1, std::memory_order_relaxed) + 1;
							PAI("input: click #{} on '{}' -> power attack", click, Keys::Describe(code));
							Attack::PowerAttack("click");
						}
					} catch (const std::exception& e) {
						PAE("input: sink threw: {}", e.what());
					} catch (...) {
						PAE("input: sink threw a non-std exception");
					}

					return RE::BSEventNotifyControl::kContinue;
				}
			};
		}

		void ResetState()
		{
			s_modifierHeld.store(false, std::memory_order_release);
			PA("input: modifier state cleared");
		}

		void Register()
		{
			if (auto* manager = RE::BSInputDeviceManager::GetSingleton()) {
				manager->AddEventSink(Sink::GetSingleton());
				PAI("input: input sink registered (click '{}' to power attack)", Keys::Describe(Config::Cached().hotkey));
			} else {
				PAE("input: BSInputDeviceManager singleton is null, the click will not work");
			}
		}
	}
}
