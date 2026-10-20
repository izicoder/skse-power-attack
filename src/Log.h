#pragma once

#include "PCH.h"
#include "Version.h"

// trace: the workhorse - one of these on every single step
#define PA(...)  ::SPA::Log::Write(spdlog::level::trace, __FILE__, __LINE__, __VA_ARGS__)
// info: milestones that must stay visible
#define PAI(...) ::SPA::Log::Write(spdlog::level::info, __FILE__, __LINE__, __VA_ARGS__)
#define PAW(...) ::SPA::Log::Write(spdlog::level::warn, __FILE__, __LINE__, __VA_ARGS__)
#define PAE(...) ::SPA::Log::Write(spdlog::level::err, __FILE__, __LINE__, __VA_ARGS__)

// Every line goes through here so that the level is forced to trace, the file is flushed after
// every line (the log then ends with the last thing that ran), and a bad format string can never
// take the game down.
namespace SPA::Log
{
	inline std::string ModulePath()
	{
		HMODULE module = nullptr;
		::GetModuleHandleExW(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCWSTR>(&ModulePath),
			&module);

		wchar_t buffer[MAX_PATH * 8]{};
		const auto chars = ::GetModuleFileNameW(module, buffer, static_cast<DWORD>(std::size(buffer)));
		if (chars == 0) {
			return "<unknown module path>";
		}
		return std::filesystem::path{ std::wstring_view{ buffer, chars } }.string();
	}

	template <class... Args>
	void Write(spdlog::level::level_enum a_level, const char* a_file, int a_line, spdlog::format_string_t<Args...> a_fmt, Args&&... a_args)
	{
		try {
			spdlog::log(spdlog::source_loc{ a_file, a_line, "" }, a_level, a_fmt, std::forward<Args>(a_args)...);
		} catch (...) {
		}
		try {
			if (auto logger = spdlog::default_logger()) {
				logger->flush();
			}
		} catch (...) {
		}
	}

	inline void Init()
	{
		try {
			if (auto logger = spdlog::default_logger()) {
				logger->set_level(spdlog::level::trace);
				logger->flush_on(spdlog::level::trace);
			}
			spdlog::set_level(spdlog::level::trace);
			spdlog::flush_on(spdlog::level::trace);
		} catch (...) {
		}

		const char* runtime = REL::Module::IsVR() ? "VR" : (REL::Module::IsAE() ? "AE" : "SE");
		PAI("======================================================================");
		PAI("SKSE Power Attack {} starting", SPA_VERSION);
		PAI("skyrim runtime : {} {}", runtime, REL::Module::get().version().string("."));
		PAI("dll            : {}", ModulePath());
		PAI("======================================================================");
	}

	// Off silences the file completely; the plugin runs the same either way.
	inline void SetEnabled(bool a_enabled)
	{
		const auto level = a_enabled ? spdlog::level::trace : spdlog::level::off;
		try {
			if (auto logger = spdlog::default_logger()) {
				logger->set_level(level);
				logger->flush_on(level);
			}
			spdlog::set_level(level);
			spdlog::flush_on(level);
		} catch (...) {
		}
	}
}
