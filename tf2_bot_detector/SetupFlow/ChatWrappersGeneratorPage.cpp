#include "ChatWrappersGeneratorPage.h"
#include "Config/Settings.h"

using namespace tf2_bot_detector;

bool ChatWrappersGeneratorPage::ValidateSettings(const Settings& settings) const
{
	return settings.m_Unsaved.m_ChatMsgWrappers.is_valid();
}

auto ChatWrappersGeneratorPage::OnDraw(const DrawState& ds) -> OnDrawResult
{
	return OnDrawResult::EndDrawing;
}

void ChatWrappersGeneratorPage::Init(const Settings& settings)
{
}

bool ChatWrappersGeneratorPage::CanCommit() const
{
	return true;
}

void ChatWrappersGeneratorPage::Commit(Settings& settings)
{
	auto tfDir = settings.GetTFDir();
	settings.m_Unsaved.m_ChatMsgWrappers = std::async([tfDir] { return RandomizeChatWrappers(tfDir); });
}
