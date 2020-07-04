#include "ChatWrappersGeneratorPage.h"
#include "Config/Settings.h"
#include "ImGui_TF2BotDetector.h"
#include "Platform/Platform.h"

#include <mh/future.hpp>

using namespace tf2_bot_detector;

#ifdef _DEBUG
namespace tf2_bot_detector
{
	extern bool g_SkipOpenTF2Check;
}
#endif

bool ChatWrappersGeneratorPage::ValidateSettings(const Settings& settings) const
{
	return settings.m_Unsaved.m_ChatMsgWrappers.has_value();
}

auto ChatWrappersGeneratorPage::OnDraw(const DrawState& ds) -> OnDrawResult
{
	bool skipOpenTF2Check = false;

#ifdef _DEBUG
	skipOpenTF2Check = g_SkipOpenTF2Check;
#endif

	if (!skipOpenTF2Check && Processes::IsTF2Running())
	{
		ImGui::TextUnformatted("TF2 must be closed before opening TF2 Bot Detector.");
	}
	else
	{
		ImGui::TextUnformatted("Generating chat message wrappers...");

		if (mh::is_future_ready(m_ChatWrappers))
			return OnDrawResult::EndDrawing;
	}

	return OnDrawResult::ContinueDrawing;
}

void ChatWrappersGeneratorPage::Init(const Settings& settings)
{
	auto tfDir = settings.GetTFDir();
	m_ChatWrappers = std::async([tfDir] { return RandomizeChatWrappers(tfDir); });
}

bool ChatWrappersGeneratorPage::CanCommit() const
{
	return true;
}

void ChatWrappersGeneratorPage::Commit(Settings& settings)
{
	assert(mh::is_future_ready(m_ChatWrappers));
	settings.m_Unsaved.m_ChatMsgWrappers = m_ChatWrappers.get();
}
