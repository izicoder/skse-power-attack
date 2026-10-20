#include "PCH.h"
#include "Attack.h"
#include "Config.h"
#include "Input.h"
#include "Log.h"
#include "UI.h"
#include "Version.h"

namespace
{
	void MessageHandler(SKSE::MessagingInterface::Message* a_message)
	{
		if (!a_message) {
			return;
		}

		try {
			switch (a_message->type) {
			case SKSE::MessagingInterface::kDataLoaded:
				{
					PAI("msg: kDataLoaded - bringing the plugin online");
					SPA::Config::Load();
					SPA::Attack::Init(SKSE::GetTaskInterface());
					SPA::Input::Register();
					SPA::UI::Register();
					PAI("msg: SKSE Power Attack v{} is live", SPA_VERSION);
				}
				break;
			case SKSE::MessagingInterface::kPreLoadGame:
				PAI("msg: kPreLoadGame - a save is about to load");
				SPA::Input::ResetState();
				SPA::Attack::Reset();
				break;
			case SKSE::MessagingInterface::kPostLoadGame:
				PA("msg: kPostLoadGame");
				SPA::Input::ResetState();
				break;
			case SKSE::MessagingInterface::kNewGame:
				PA("msg: kNewGame");
				SPA::Input::ResetState();
				SPA::Attack::Reset();
				break;
			default:
				PA("msg: type {}", a_message->type);
				break;
			}
		} catch (const std::exception& e) {
			PAE("msg: handler for type {} threw: {}", a_message->type, e.what());
		} catch (...) {
			PAE("msg: handler for type {} threw a non-std exception", a_message->type);
		}
	}
}

extern "C" DLLEXPORT bool SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	if (!a_skse) {
		return false;
	}

	SKSE::Init(a_skse);
	SPA::Log::Init();

	const auto messaging = SKSE::GetMessagingInterface();
	if (!messaging) {
		PAE("no messaging interface available, refusing to continue");
		return false;
	}

	messaging->RegisterListener(MessageHandler);
	PAI("messaging listener registered");
	return true;
}
