#include "PCH.h"
#include "Config.h"
#include "Log.h"

namespace OCPA
{
	namespace
	{
		std::recursive_mutex s_lock;
		std::atomic<std::uint64_t> s_revision{ 1 };

		// One JSON file next to the loaded DLL, i.e. inside the mod folder under MO2.
		std::filesystem::path ResolvePath()
		{
			HMODULE module = nullptr;
			::GetModuleHandleExW(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCWSTR>(&Config::Path),
				&module);

			wchar_t buffer[MAX_PATH * 8]{};
			const auto chars = ::GetModuleFileNameW(module, buffer, static_cast<DWORD>(std::size(buffer)));
			std::filesystem::path path{ std::wstring_view{ buffer, static_cast<std::size_t>(chars) } };
			path = path.parent_path() / L"OneClickPowerAttack.json";
			PA("config: path = {}", path.string());
			return path;
		}

		// Keys this build understands. Anything else in the file is kept on save, so a file
		// written by a newer build survives a downgrade.
		bool IsKnownKey(std::string_view a_key)
		{
			static constexpr std::string_view kKeys[] = { "hotkey", "modifierKey", "allowZeroStamina", "logging" };
			return std::find(std::begin(kKeys), std::end(kKeys), a_key) != std::end(kKeys);
		}

		nlohmann::json s_unknown{ nlohmann::json::object() };

		// Numbers are read through one rounding path, and a wrong type costs only that key
		// instead of aborting the whole load.
		std::optional<int> ReadInt(const nlohmann::json& a_json, const char* a_key)
		{
			if (!a_json.contains(a_key) || !a_json.at(a_key).is_number()) {
				return std::nullopt;
			}
			const double value = a_json.at(a_key).get<double>();
			if (!std::isfinite(value)) {
				PAW("config: '{}' is {}, ignoring it", a_key, a_json.at(a_key).dump());
				return std::nullopt;
			}
			return static_cast<int>(std::lround((std::clamp)(value, -1.0e6, 1.0e6)));
		}

		std::optional<bool> ReadBool(const nlohmann::json& a_json, const char* a_key)
		{
			if (!a_json.contains(a_key)) {
				return std::nullopt;
			}
			const auto& value = a_json.at(a_key);
			if (!value.is_boolean()) {
				PAW("config: '{}' is {}, expected true or false, keeping the default", a_key, value.type_name());
				return std::nullopt;
			}
			return value.get<bool>();
		}

		nlohmann::json ToJson(const Settings& a_settings)
		{
			auto json = s_unknown;
			json.update(nlohmann::json{
				{ "hotkey", a_settings.hotkey },
				{ "modifierKey", a_settings.modifierKey },
				{ "allowZeroStamina", a_settings.allowZeroStamina },
				{ "logging", a_settings.logging },
			});
			return json;
		}
	}

	Settings& Config::Get()
	{
		static Settings settings;
		return settings;
	}

	Settings Config::Snapshot()
	{
		std::scoped_lock lock{ s_lock };
		return Get();
	}

	const Settings& Config::Cached()
	{
		static Settings    cache;
		static std::uint64_t cachedRevision = 0;
		const auto         revision = Revision();
		if (revision != cachedRevision) {
			std::scoped_lock lock{ s_lock };
			cache = Get();
			cachedRevision = revision;
		}
		return cache;
	}

	std::uint64_t Config::Revision()
	{
		return s_revision.load(std::memory_order_acquire);
	}

	void Config::Touch()
	{
		s_revision.fetch_add(1, std::memory_order_release);
	}

	void Config::ResetToDefaults()
	{
		std::scoped_lock lock{ s_lock };
		Get() = Settings{};
		Touch();
	}

	std::string Config::Describe()
	{
		std::scoped_lock lock{ s_lock };
		const auto& s = Get();
		return std::format("hotkey={} modifier={} allowZeroStamina={} logging={}",
			s.hotkey, s.modifierKey, s.allowZeroStamina, s.logging);
	}

	std::filesystem::path Config::Path()
	{
		static const auto path = ResolvePath();
		return path;
	}

	bool Config::Validate()
	{
		std::scoped_lock lock{ s_lock };
		auto& s = Get();
		bool changed = false;

		// 0 is a legal scancode, so "no key" is -1 and every code above the gamepad block is junk.
		if (s.hotkey < 0 || s.hotkey > kMaxKeyCode) {
			PAW("config: hotkey {} is not bindable, restoring the default 257 (right mouse)", s.hotkey);
			s.hotkey = Settings{}.hotkey;
			changed = true;
		}
		if (s.modifierKey != kNoKey && (s.modifierKey < 0 || s.modifierKey > kMaxKeyCode)) {
			PAW("config: modifierKey {} is not bindable, disabling the modifier", s.modifierKey);
			s.modifierKey = kNoKey;
			changed = true;
		}
		if (s.modifierKey == s.hotkey) {
			PAW("config: modifierKey equals hotkey ({}), disabling the modifier", s.modifierKey);
			s.modifierKey = kNoKey;
			changed = true;
		}
		return changed;
	}

	void Config::Load()
	{
		std::scoped_lock lock{ s_lock };
		const auto       path = Path();
		PAI("config: Load() begin, {}", path.string());

		std::error_code ec;
		if (!std::filesystem::exists(path, ec)) {
			PAW("config: no config file yet, writing the defaults");
			Save();
			return;
		}

		try {
			std::ifstream file{ path, std::ios::binary };
			if (!file) {
				PAE("config: could not open the file, keeping the defaults");
				return;
			}
			const auto json = nlohmann::json::parse(file);
			if (!json.is_object()) {
				PAE("config: root is {}, not an object, keeping the defaults", json.type_name());
				return;
			}

			auto& s = Get();

			s_unknown = nlohmann::json::object();
			for (const auto& [key, value] : json.items()) {
				if (IsKnownKey(key)) {
					PA("config:   {} = {}", key, value.dump());
				} else {
					s_unknown[key] = value;
					PAW("config:   keeping unknown option '{}' = {}", key, value.dump());
				}
			}

			if (const auto value = ReadInt(json, "hotkey")) {
				s.hotkey = *value;
			}
			if (const auto value = ReadInt(json, "modifierKey")) {
				s.modifierKey = *value;
			}
			if (const auto value = ReadBool(json, "allowZeroStamina")) {
				s.allowZeroStamina = *value;
			}
			if (const auto value = ReadBool(json, "logging")) {
				s.logging = *value;
			}

			if (Validate()) {
				PAI("config: repaired, rewriting the file");
				Save();
			}
			Touch();
			Log::SetEnabled(s.logging);
			PAI("config: loaded: {}", Describe());
		} catch (const std::exception& e) {
			PAE("config: load threw: {}, keeping the in-memory values", e.what());
		}
	}

	void Config::Save()
	{
		std::scoped_lock lock{ s_lock };
		const auto       path = Path();
		Validate();

		const auto payload = ToJson(Get()).dump(4);
		PA("config: saving: {}", Describe());

		std::error_code ec;
		std::filesystem::create_directories(path.parent_path(), ec);

		// Written beside the real file and moved over it, so a crash mid-write cannot leave a
		// half-written config behind (which would parse to nothing and silently reset everything).
		auto temp = path;
		temp += L".tmp";

		try {
			{
				std::ofstream file{ temp, std::ios::binary | std::ios::trunc };
				if (!file) {
					PAE("config: could not open '{}' for writing", temp.string());
					return;
				}
				file << payload;
				file.flush();
				if (!file.good()) {
					PAE("config: writing '{}' failed, the old config is untouched", temp.string());
					return;
				}
			}
			if (!::MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
				PAE("config: could not replace '{}' (error {}), the old config is untouched", path.string(), ::GetLastError());
				return;
			}
			Touch();
			PAI("config: saved to {}", path.string());
		} catch (const std::exception& e) {
			PAE("config: write threw: {}", e.what());
		}
	}
}
