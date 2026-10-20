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
		enum class Bind
		{
			kNone,
			kHotkey,
			kModifier
		};

		std::atomic<Bind> s_capture{ Bind::kNone };
		std::atomic<int>  renderFrames{ 0 };

		constexpr ImGui::ImVec4 kValueColor{ 0.80f, 0.90f, 1.00f, 1.0f };
		constexpr ImGui::ImVec4 kWarnColor{ 1.00f, 0.80f, 0.30f, 1.0f };

		void SaveNow(const char* a_reason)
		{
			PAI("ui: saving config ({}) values: {}", a_reason, Config::Describe());
			Config::Save();
		}

		void Body(const char* a_text)
		{
			ImGui::PushStyleColor(ImGui::ImGuiCol_Text, kValueColor);
			ImGui::TextWrapped("%s", a_text);
			ImGui::PopStyleColor();
		}

		void BeginCapture(Bind a_target)
		{
			if (s_capture.exchange(a_target) == Bind::kNone) {
				PAI("ui: waiting for a new key (target={})", static_cast<int>(a_target));
			}
			SKSEMenuFramework::SetHotkeyEnabled(false);
		}

		void EndCapture()
		{
			if (s_capture.exchange(Bind::kNone) != Bind::kNone) {
				SKSEMenuFramework::SetHotkeyEnabled(true);
				PA("ui: key capture finished, framework hotkey re-enabled");
			}
		}

		// One key bind row: current key, Rebind, and (optionally) Clear.
		void RenderBind(const char* a_label, int a_code, Bind a_target, bool a_allowNone)
		{
			const bool capturing = s_capture.load() == a_target;

			ImGui::Text("%s:", a_label);
			ImGui::SameLine();
			if (capturing) {
				ImGui::PushStyleColor(ImGui::ImGuiCol_Text, kWarnColor);
				ImGui::TextUnformatted("listening... press a key, mouse button or gamepad button");
				ImGui::PopStyleColor();
				ImGui::SameLine();
				if (ImGui::Button("Cancel")) {
					EndCapture();
				}
				return;
			}

			ImGui::PushStyleColor(ImGui::ImGuiCol_Text, kValueColor);
			ImGui::Text("%s", Keys::Describe(a_code).c_str());
			ImGui::PopStyleColor();
			ImGui::SameLine();
			if (ImGui::Button(std::format("Rebind##{}", static_cast<int>(a_target)).c_str())) {
				BeginCapture(a_target);
			}
			if (a_allowNone && a_code != kNoKey) {
				ImGui::SameLine();
				if (ImGui::Button(std::format("Clear##{}", static_cast<int>(a_target)).c_str())) {
					Config::Get().modifierKey = kNoKey;
					Config::Touch();
					SaveNow("modifier cleared");
				}
			}
		}

		void __stdcall Render()
		{
			const int frame = renderFrames.fetch_add(1) + 1;
			if (frame <= 5) {
				PA("ui: Render() frame {} values: {}", frame, Config::Describe());
			}

			auto& settings = Config::Get();

			ImGui::SeparatorText("Click");
			RenderBind("Power attack key", settings.hotkey, Bind::kHotkey, false);
			RenderBind("Modifier key (optional)", settings.modifierKey, Bind::kModifier, true);

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
			if (ImGui::Button("Reset defaults (right mouse, no modifier)")) {
				PAI("ui: reset to defaults (was: {})", Config::Describe());
				EndCapture();
				Config::ResetToDefaults();
				Log::SetEnabled(Config::Get().logging);
				SaveNow("reset defaults");
			}

			ImGui::Spacing();
			Body("Every change saves itself to SKSE/Plugins/SKSEPowerAttack.json.");
		}

		// While capturing, listen to raw input through the framework's input hook.
		bool __stdcall InputListener(RE::InputEvent* a_event)
		{
			const auto target = s_capture.load();
			if (target == Bind::kNone || !a_event) {
				return false;
			}

			for (auto* event = a_event; event; event = event->next) {
				const auto* button = event->AsButtonEvent();
				if (!button || !button->IsDown()) {
					continue;
				}

				const auto code = static_cast<int>(Keys::Encode(button->device.get(), button->GetIDCode()));
				if (code == static_cast<int>(Keys::kUnknown)) {
					continue;
				}

				auto& settings = Config::Get();
				if (target == Bind::kHotkey) {
					if (code == settings.modifierKey) {
						// Binding the modifier as the hotkey would make the click impossible.
						PAW("ui: '{}' is already the modifier, pick another key", Keys::Describe(code));
						continue;
					}
					settings.hotkey = code;
				} else {
					if (code == settings.hotkey) {
						PAW("ui: '{}' is already the power attack key, pick another key", Keys::Describe(code));
						continue;
					}
					settings.modifierKey = code;
				}

				PAI("ui: captured '{}' -> {}", Keys::Describe(code), Config::Describe());
				Config::Touch();
				EndCapture();
				SaveNow("key rebound");
				return true;  // swallow the press so it does not also reach the game
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
