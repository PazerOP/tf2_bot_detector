#include "ChatWrappersGeneratorPage.h"
#include "Config/Settings.h"
#include "ImGui_TF2BotDetector.h"

#include <mh/future.hpp>

using namespace tf2_bot_detector;

bool ChatWrappersGeneratorPage::ValidateSettings(const Settings& settings) const
{
	return settings.m_Unsaved.m_ChatMsgWrappers.has_value();
}

auto ChatWrappersGeneratorPage::OnDraw(const DrawState& ds) -> OnDrawResult
{
	ImGui::TextUnformatted("Generating chat message wrappers...");

	if (mh::is_future_ready(m_ChatWrappers))
		return OnDrawResult::EndDrawing;

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
