#include "PCH.h"
#include "Config.h"
#include "Log.h"

namespace SPA
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
			path = path.parent_path() / L"SKSEPowerAttack.json";
			PA("config: path = {}", path.string());
			return path;
		}

		// Schema version of SKSEPowerAttack.json. Bump this whenever a key changes name, type
		// or meaning, and add exactly one migration step for the jump (see Migrate).
		constexpr int kSettingsFormatVersion = 1;

		// Format version of the file that was read. Saving stamps the higher of this and
		// kSettingsFormatVersion: a file from a newer build must never be rewritten as an older
		// format, or the next build would run its migrations over data that already has them.
		int s_fileVersion = kSettingsFormatVersion;

		// Keys this build understands. Anything else in the file is kept on save, so a file
		// written by a newer build survives a downgrade.
		bool IsKnownKey(std::string_view a_key)
		{
			static constexpr std::string_view kKeys[] = { "version", "hotkey", "modifierKey", "allowZeroStamina", "logging" };
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
			auto json = s_unknown;  // keep what this build does not understand...
			json.update(nlohmann::json{
				{ "hotkey", a_settings.hotkey },
				{ "modifierKey", a_settings.modifierKey },
				{ "allowZeroStamina", a_settings.allowZeroStamina },
				{ "logging", a_settings.logging },
			});
			json["version"] = s_fileVersion;  // ...and stamp the format we wrote (never lower than what we read)
			return json;
		}

		// Walks a document from the version it was written with up to kSettingsFormatVersion.
		// Version 1 is the first format, so there is nothing to convert yet: every later format
		// adds one step here, and the save that follows rewrites the file in the current shape.
		// A file from a newer build never reaches this (ApplyDocument checks the version first).
		void Migrate(nlohmann::json& a_json, int a_fromVersion)
		{
			(void)a_json;  // nothing to convert yet; the parameter is what each new step needs
			if (a_fromVersion < kSettingsFormatVersion) {
				PAW("config: no migration from format {} to {}, leaving the file as it is", a_fromVersion, kSettingsFormatVersion);
			}
		}

		struct Applied
		{
			int  fileVersion{ 1 };       // format the document was written with
			bool versioned{ false };     // the document carried a "version" key
			bool corrected{ false };     // Validate() had to repair the values
		};

		// Reads every key this build knows out of a parsed document into Config::Get(). Both the
		// real file and an imported one go through here, so the two paths cannot drift.
		Applied ApplyDocument(nlohmann::json& a_json)
		{
			Applied applied;
			auto&   s = Config::Get();

			// Format version first: a file without one predates versioning and is treated as 1.
			if (const auto value = ReadInt(a_json, "version")) {
				applied.fileVersion = *value;
				applied.versioned = true;
			}
			PA("config: file format version = {} (this build writes {})", applied.fileVersion, kSettingsFormatVersion);
			if (applied.fileVersion > kSettingsFormatVersion) {
				PAW("config: the file was written by a newer build; its unknown options are kept as they are");
			} else {
				Migrate(a_json, applied.fileVersion);
			}
			// Never stamp a newer file down: a downgrade plus one save would otherwise rewrite a
			// newer file as this format, and the next upgrade would migrate it twice.
			s_fileVersion = (std::max)(applied.fileVersion, kSettingsFormatVersion);

			// Everything this build knows is logged and applied below; anything else is kept
			// aside so saving does not silently drop an option from another build.
			s_unknown = nlohmann::json::object();
			for (const auto& [key, value] : a_json.items()) {
				if (IsKnownKey(key)) {
					PA("config:   {} = {}", key, value.dump());
				} else {
					s_unknown[key] = value;
					PAW("config:   keeping unknown option '{}' = {}", key, value.dump());
				}
			}

			if (const auto value = ReadInt(a_json, "hotkey")) {
				s.hotkey = *value;
			}
			if (const auto value = ReadInt(a_json, "modifierKey")) {
				s.modifierKey = *value;
			}
			if (const auto value = ReadBool(a_json, "allowZeroStamina")) {
				s.allowZeroStamina = *value;
			}
			if (const auto value = ReadBool(a_json, "logging")) {
				s.logging = *value;
			}

			applied.corrected = Config::Validate();
			Config::Touch();
			Log::SetEnabled(s.logging);
			return applied;
		}

		// The file name this plugin used before it was renamed. Reading it once keeps a bound
		// key and the toggles of that build alive instead of making the user rebind everything;
		// the old file is left on disk untouched, so going back still finds it.
		bool ImportLegacy(const std::filesystem::path& a_path)
		{
			const auto legacy = a_path.parent_path() / L"OneClickPowerAttack.json";

			std::error_code ec;
			if (!std::filesystem::exists(legacy, ec)) {
				return false;
			}

			try {
				std::ifstream file{ legacy, std::ios::binary };
				if (!file) {
					PAW("config: found {} but could not open it, keeping the defaults", legacy.string());
					return false;
				}
				auto json = nlohmann::json::parse(file);
				if (!json.is_object()) {
					PAW("config: {} is {}, not an object, keeping the defaults", legacy.string(), json.type_name());
					return false;
				}

				PAI("config: importing the settings left by the earlier build in {}", legacy.string());
				ApplyDocument(json);
				PAI("config: imported: {}", Config::Describe());
				return true;
			} catch (const std::exception& e) {
				PAW("config: importing {} threw: {}, keeping the defaults", legacy.string(), e.what());
				return false;
			}
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
			PAW("config: hotkey {} is not bindable, restoring the default 260 (mouse button 5)", s.hotkey);
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
			// First launch with this file name: adopt whatever an earlier build left beside the
			// DLL, then write it in the current shape. Both outcomes end in a real file.
			if (!ImportLegacy(path)) {
				PAW("config: no config file yet, writing the defaults");
			}
			Save();
			return;
		}

		try {
			std::ifstream file{ path, std::ios::binary };
			if (!file) {
				PAE("config: could not open the file, keeping the defaults");
				return;
			}
			auto json = nlohmann::json::parse(file);
			if (!json.is_object()) {
				PAE("config: root is {}, not an object, keeping the defaults", json.type_name());
				return;
			}

			const auto applied = ApplyDocument(json);
			PAI("config: loaded: {}", Describe());

			// Rewrite in the current format straight away when the file had no version, was
			// migrated or was repaired, so every later launch reads a file that needs neither.
			// A file from a *newer* build is left alone (ToJson keeps its version anyway).
			if (applied.corrected || !applied.versioned || applied.fileVersion < kSettingsFormatVersion) {
				PAI("config: rewriting the file in format {}", kSettingsFormatVersion);
				Save();
			}
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
