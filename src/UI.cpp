#include "PCH.h"
#include "UI.h"
#include "Config.h"
#include "Input.h"
#include "Log.h"
#include "SKSEMenuFramework.h"

// The vendored SKSEMenuFramework.h exposes the ImGui API in namespace ImGuiMCP;
// alias it so call sites read like normal ImGui code.
namespace ImGui = ImGuiMCP;

namespace SPA::UI
{
	namespace
	{
		// -1 = no capture running, otherwise the index into Settings::binds. The modifiers pressed
		// during a capture live here so the row can show "listening for Shift + ..." while the
		// user is still holding the modifier down.
		std::atomic<int>      s_captureSlot{ -1 };
		std::atomic<unsigned> s_captureMods{ Chord::kNone };
		std::atomic<int>  renderFrames{ 0 };

		constexpr ImGui::ImVec4 kValueColor{ 0.80f, 0.90f, 1.00f, 1.0f };
		constexpr ImGui::ImVec4 kWarnColor{ 1.00f, 0.80f, 0.30f, 1.0f };

		// Reset wipes every setting, so the second click has to land inside this window.
		constexpr float kResetConfirmSeconds = 4.0f;

		std::atomic<std::int64_t> resetArmedTicks{ 0 };

		float SecondsSince(std::int64_t a_ticks)
		{
			const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
			return static_cast<float>(now - a_ticks) / static_cast<float>(std::chrono::steady_clock::period::den) *
				static_cast<float>(std::chrono::steady_clock::period::num);
		}

		// Pops on the way out, including when an exception unwinds through it: the ImGui style
		// stack is global state, so a leaked push would tint every later frame.
		class ScopedTextColor
		{
		public:
			explicit ScopedTextColor(const ImGui::ImVec4& a_color) { ImGui::PushStyleColor(ImGui::ImGuiCol_Text, a_color); }
			~ScopedTextColor() { ImGui::PopStyleColor(); }

			ScopedTextColor(const ScopedTextColor&) = delete;
			ScopedTextColor& operator=(const ScopedTextColor&) = delete;
		};

		void SaveNow(const char* a_reason)
		{
			PAI("ui: saving config ({}) values: {}", a_reason, Config::Describe());
			Config::Save();
		}

		void Body(const char* a_text)
		{
			const ScopedTextColor color{ kValueColor };
			ImGui::TextWrapped("%s", a_text);
		}

		void BeginCapture(Binds::Slot a_slot)
		{
			// Seeded from the sink's own mask: a modifier already held when the button is clicked
			// belongs to the chord, and its press happened before the capture began.
			const auto held = Input::HeldMods();
			s_captureMods.store(held, std::memory_order_release);
			if (s_captureSlot.exchange(static_cast<int>(Binds::Index(a_slot))) < 0) {
				PAI("ui: waiting for a new key (box={}, modifiers already held=0x{:X})", Binds::Name(a_slot), held);
			}
			SKSEMenuFramework::SetHotkeyEnabled(false);
		}

		void EndCapture()
		{
			s_captureMods.store(Chord::kNone, std::memory_order_release);
			if (s_captureSlot.exchange(-1) >= 0) {
				SKSEMenuFramework::SetHotkeyEnabled(true);
				PA("ui: key capture finished, framework hotkey re-enabled");
			}
		}

		// One chord row: current chord, Rebind, Clear. Every box can be cleared, including auto:
		// the menu is the only way to unbind what the plugin does, so it has to be able to.
		void RenderBind(const char* a_label, Chord::Bind& a_bind, Binds::Slot a_slot)
		{
			const int  index = static_cast<int>(Binds::Index(a_slot));
			const bool capturing = s_captureSlot.load(std::memory_order_acquire) == index;

			ImGui::Text("%s:", a_label);
			ImGui::SameLine();
			if (capturing) {
				const auto mods = s_captureMods.load(std::memory_order_acquire);
				{
					const ScopedTextColor warn{ kWarnColor };
					if (mods == Chord::kNone) {
						ImGui::TextUnformatted("listening... press a key (hold Shift, Ctrl or Alt first), Esc cancels");
					} else {
						ImGui::TextUnformatted(std::format("listening for {}a key, Esc cancels", Chord::ModsPrefix(mods)).c_str());
					}
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel")) {
					EndCapture();
				}
				return;
			}

			{
				const ScopedTextColor value{ kValueColor };
				ImGui::TextUnformatted(Keys::DescribeChord(a_bind).c_str());
			}
			ImGui::SameLine();
			if (ImGui::Button(std::format("Rebind##{}", index).c_str())) {
				BeginCapture(a_slot);
			}
			if (a_bind.IsBound()) {
				ImGui::SameLine();
				if (ImGui::Button(std::format("Clear##{}", index).c_str())) {
					a_bind = Chord::Bind{};
					Config::Touch();
					SaveNow(std::format("{} key cleared", Binds::Name(a_slot)).c_str());
				}
			}
		}

		// Reset wipes every setting, so it takes two clicks inside the window above; the second
		// click is the one that resets. Cancel and the expiry both disarm it.
		void RenderReset()
		{
			const auto armed = resetArmedTicks.load(std::memory_order_acquire);
			const bool confirming = armed != 0 && SecondsSince(armed) < kResetConfirmSeconds;

			if (ImGui::Button(confirming ? "Confirm reset" : "Reset defaults")) {
				if (confirming) {
					resetArmedTicks.store(0, std::memory_order_release);
					PAI("ui: reset to defaults confirmed (was: {})", Config::Describe());

					// The capture is separate state: leaving it armed would swallow the next key
					// and write it over the defaults that were just restored.
					EndCapture();
					Config::ResetToDefaults();
					Log::SetEnabled(Config::Get().logging);
					SaveNow("reset defaults");
				} else {
					resetArmedTicks.store(std::chrono::steady_clock::now().time_since_epoch().count(), std::memory_order_release);
					PAI("ui: reset armed, click again within {:.0f}s to confirm", kResetConfirmSeconds);
				}
			}

			// Right after the button, before Cancel becomes "the last item".
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Mouse 5 on auto, the right, left and dual boxes unbound, power attacks at 0 stamina, logging on.");
			}

			// Re-read: a confirmed reset above just cleared it, so Cancel must not linger.
			const auto stillArmed = resetArmedTicks.load(std::memory_order_acquire);
			if (stillArmed != 0 && SecondsSince(stillArmed) < kResetConfirmSeconds) {
				ImGui::SameLine();
				if (ImGui::Button("Cancel##reset")) {
					resetArmedTicks.store(0, std::memory_order_release);
					PA("ui: reset cancelled");
				}
			}
		}

		void RenderTab()
		{
			const int frame = renderFrames.fetch_add(1) + 1;
			if (frame <= 5) {
				PA("ui: Render() frame {} values: {}", frame, Config::Describe());
			}

			auto& settings = Config::Get();

			ImGui::SeparatorText("Power attack keys");
			RenderBind("Auto (follows your hands)", settings.binds[Binds::Index(Binds::Slot::kAuto)], Binds::Slot::kAuto);
			RenderBind("Right power attack", settings.binds[Binds::Index(Binds::Slot::kRight)], Binds::Slot::kRight);
			RenderBind("Left power attack", settings.binds[Binds::Index(Binds::Slot::kLeft)], Binds::Slot::kLeft);
			RenderBind("Dual power attack", settings.binds[Binds::Index(Binds::Slot::kDual)], Binds::Slot::kDual);

			// The action is not a setting for the three fixed boxes: they are whatever they say.
			// Only auto looks at the hands, so there is nothing to get wrong when the loadout changes.
			ImGui::Spacing();
			Body("Hold Shift, Ctrl or Alt and press a key to bind a chord. Two boxes may share a chord: the one higher in this list wins. Binding the game's own attack key takes that press away from the game, so a tap gives only the power attack.");
			ImGui::Spacing();
			Body("Auto picks from your hands: a weapon in both -> dual, one weapon -> that hand, nothing equipped -> right.");
			Body("Tap = release inside the tap window -> power attack. Hold it longer and nothing happens, except on the game's own attack key, where the hold becomes a normal swing.");
			Body("Jumping or blocking swallows the power attack action, so there the plugin swings normally first and power attacks 200 ms later.");
			Body("Bows, crossbows, staves, horses, killmoves, sitting, dialogue and a paused game are left to the game: no power attack, and the press is not taken.");

			ImGui::Spacing();
			ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
			if (ImGui::SliderInt("Tap window (ms)", &settings.tapWindowMs, kMinTapWindowMs, kMaxTapWindowMs)) {
				// The slider is the range, so this is only a guard against a hand-edited value.
				settings.tapWindowMs = (std::clamp)(settings.tapWindowMs, kMinTapWindowMs, kMaxTapWindowMs);
				Config::Touch();  // the game thread sees the new window while the slider is dragged
			}
			// Saving on release, not on every frame of a drag: one file write per adjustment.
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				PAI("ui: tapWindowMs -> {}", settings.tapWindowMs);
				SaveNow("tap window changed");
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Press and release inside this many ms to power attack. Holding the key does nothing.");
			}

			ImGui::Spacing();
			if (ImGui::Checkbox("Power attack with an empty stamina bar", &settings.allowZeroStamina)) {
				PA("ui: allowZeroStamina -> {}", settings.allowZeroStamina);
				Config::Touch();
				SaveNow("allowZeroStamina changed");
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Leave on if another mod lets you power attack at 0 stamina.");
			}

			ImGui::SeparatorText("Diagnostics");
			if (ImGui::Checkbox("Write SKSEPowerAttack.log", &settings.logging)) {
				PA("ui: logging -> {}", settings.logging);
				Log::SetEnabled(settings.logging);
				Config::Touch();
				SaveNow("logging changed");
			}

			ImGui::SeparatorText("Reset");
			RenderReset();

			ImGui::Spacing();
			Body("Every change saves itself to SKSE/Plugins/SKSEPowerAttack.json.");
		}

		// The framework calls this from the game's draw loop, so nothing may escape: an
		// exception is logged and the next frame tries again (the colour scopes above keep the
		// ImGui style stack balanced while it unwinds).
		void __stdcall Render()
		{
			try {
				RenderTab();
			} catch (const std::exception& e) {
				PAE("ui: Render threw: {}", e.what());
			} catch (...) {
				PAE("ui: Render threw a non-std exception");
			}
		}

		// While capturing, listen to raw input through the framework's input hook. A modifier is
		// part of the chord, never the end of the capture, which is what makes Shift + Mouse 5
		// bindable at all.
		bool CaptureKey(RE::InputEvent* a_event)
		{
			const auto target = s_captureSlot.load(std::memory_order_acquire);
			if (target < 0 || !a_event) {
				return false;
			}

			// Tracked from this batch, in order, so the modifiers the user presses stay in the
			// preview whether or not the input sink has run yet.
			auto mods = s_captureMods.load(std::memory_order_acquire);

			for (auto* event = a_event; event; event = event->next) {
				const auto* button = event->AsButtonEvent();
				if (!button) {
					continue;
				}

				const auto code = static_cast<int>(Keys::Encode(button->device.get(), button->GetIDCode()));
				if (code == static_cast<int>(Keys::kUnknown)) {
					continue;
				}

				if (const auto modBit = Chord::ModBitFor(code); modBit != Chord::kNone) {
					// Not swallowed: the menu is open, and letting go has to take the modifier
					// back out of the preview.
					const bool held = button->Value() > 0.0f;
					mods = held ? (mods | modBit) : (mods & ~modBit);
					s_captureMods.store(mods, std::memory_order_release);
					PAI("ui: capture modifier '{}' {} (0x{:X})", Keys::Describe(code), held ? "down" : "up", mods);
					continue;
				}

				if (!button->IsDown()) {
					continue;
				}

				// Escape leaves the box as it was: the way out of a capture without binding.
				if (code == Chord::kScanEscape) {
					PAI("ui: key capture cancelled with Escape");
					EndCapture();
					return true;
				}

				auto&      settings = Config::Get();
				const auto bind = Chord::Bind{ code, mods };
				settings.binds[static_cast<std::size_t>(target)] = bind;

				PAI("ui: captured '{}' -> {}", Keys::DescribeChord(bind), Config::Describe());
				Config::Touch();
				EndCapture();
				SaveNow("key rebound");
				return true;  // swallow the press so it does not also reach the game
			}
			return false;
		}

		// The framework's input hook: an exception must not escape into it, and a press that
		// could not be handled is simply not swallowed.
		bool __stdcall InputListener(RE::InputEvent* a_event)
		{
			try {
				return CaptureKey(a_event);
			} catch (const std::exception& e) {
				PAE("ui: key capture threw: {}, ignoring this press", e.what());
			} catch (...) {
				PAE("ui: key capture threw a non-std exception, ignoring this press");
			}
			return false;
		}

		std::unique_ptr<SKSEMenuFramework::Model::InputEvent> inputEvent;
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			PAW("ui: SKSE Menu Framework not detected, menu disabled (the JSON settings still apply)");
			return;
		}

		SKSEMenuFramework::SetSection("SKSE Power Attack");
		SKSEMenuFramework::AddSectionItem("Settings", Render);
		inputEvent = std::make_unique<SKSEMenuFramework::Model::InputEvent>(&InputListener);
		PAI("ui: menu registered (settings tab + key capture)");
	}
}
