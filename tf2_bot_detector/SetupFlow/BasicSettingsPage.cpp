#include "BasicSettingsPage.h"
#include "Util/PathUtils.h"
#include "UI/ImGui_TF2BotDetector.h"

using namespace std::string_view_literals;
using namespace tf2_bot_detector;

template<typename T>
static bool InternalValidateSettings(const T& settings)
{
	if (auto steamDir = settings.GetSteamDir(); steamDir.empty() || !ValidateSteamDir(steamDir))
		return false;

	if (auto tfDir = settings.GetTFDir(); tfDir.empty() || !ValidateTFDir(tfDir))
		return false;

	if (!settings.GetLocalSteamID().IsValid())
		return false;

	return true;
}

auto BasicSettingsPage::ValidateSettings(const Settings& settings) const -> ValidateSettingsResult
{
	return InternalValidateSettings(settings) ? ValidateSettingsResult::Success : ValidateSettingsResult::TriggerOpen;
}

auto BasicSettingsPage::OnDraw(const DrawState& ds) -> OnDrawResult
{
	ImGui::TextFmt("Due to your configuration, some settings could not be automatically detected.");
	ImGui::NewLine();

	// 1. Steam directory
	bool any = false;
	//if (auto autodetected = GetCurrentSteamDir(); autodetected.empty())
	{
		any = true;
		ImGui::TextFmt("Location of your Steam directory:");
		ImGui::NewLine();
		ImGuiDesktop::ScopeGuards::Indent indent;
		ImGui::TextFmt("Your Steam directory is needed to execute commands in TF2, read console logs, and determine your current Steam ID3.");

		InputTextSteamDirOverride("##SetupFlow_SteamDir", m_Settings.m_SteamDirOverride);

		ImGui::NewLine();
	}

	//if (FindTFDir(m_Settings.GetSteamDir()).empty())
	{
		any = true;
		ImGui::TextFmt("Location of your tf directory:");
		ImGui::NewLine();
		ImGui::Indent();
		ImGui::TextFmt("Your tf directory is needed so this tool knows where to read console output from. It is also needed to automatically run commands in the game.");

		InputTextTFDirOverride("##SetupFlow_TFDir", m_Settings.m_TFDirOverride, FindTFDir(m_Settings.GetSteamDir()));

		ImGui::Unindent();
	}

	// 2. Steam ID
	//if (!GetCurrentActiveSteamID().IsValid())
	{
		any = true;
		ImGui::TextFmt("Your Steam ID3:"sv);
		ImGui::NewLine();
		ImGui::Indent();
		ImGui::TextFmt("Your Steam ID3 is needed to identify who can be votekicked (same team) and who is on the other team. You can find your Steam ID3 using sites like steamidfinder.com.");

		InputTextSteamIDOverride("##SetupFlow_SteamID", m_Settings.m_LocalSteamIDOverride);

		ImGui::Unindent();
		ImGui::NewLine();
	}

	return any ? OnDrawResult::ContinueDrawing : OnDrawResult::EndDrawing;
}

void BasicSettingsPage::Init(const InitState& is)
{
	m_Settings = is.m_Settings;
}

bool BasicSettingsPage::CanCommit() const
{
	return InternalValidateSettings(m_Settings);
}

void BasicSettingsPage::Commit(const CommitState& cs)
{
	static_cast<AutoDetectedSettings&>(cs.m_Settings) = m_Settings;
}
