#include "NetworkSettingsPage.h"
#include "Config/Settings.h"
#include "ImGui_TF2BotDetector.h"
#include "Version.h"

using namespace tf2_bot_detector;

template<typename T>
static bool InternalValidateSettings(const T& settings)
{
	if (!settings.m_AllowInternetUsage)
		return false;
	if (settings.m_AllowInternetUsage.value() && settings.m_ProgramUpdateCheckMode == ProgramUpdateCheckMode::Unknown)
		return false;
	if (settings.m_ProgramUpdateCheckMode == ProgramUpdateCheckMode::Releases && VERSION.m_Preview != 0)
		return false;

	return true;
}

bool NetworkSettingsPage::ValidateSettings(const Settings& settings) const
{
	return InternalValidateSettings(settings);
}

auto NetworkSettingsPage::OnDraw(const DrawState& ds) -> OnDrawResult
{
	ImGui::TextUnformatted("This tool can optionally connect to the internet to automatically update.");

	ImGui::Indent();
	{
		if (bool allow = m_Settings.m_AllowInternetUsage.value_or(false); ImGui::Checkbox("Allow Internet Connectivity", &allow))
			m_Settings.m_AllowInternetUsage = allow;

		if (m_Settings.m_AllowInternetUsage.value_or(false))
			ImGui::TextColoredUnformatted({ 1, 1, 0, 1 }, "If you use antivirus software, connecting to the internet may trigger warnings.");
	}
	ImGui::Unindent();
	ImGui::NewLine();

	const bool enabled = m_Settings.m_AllowInternetUsage.value_or(false);
	ImGui::EnabledSwitch(enabled, [&](bool enabled)
		{
			ImGui::BeginGroup();
			ImGui::TextUnformatted("This tool can also check for updated functionality and bugfixes on startup.");
			ImGui::Indent();
			{
				auto mode = enabled ? m_Settings.m_ProgramUpdateCheckMode : ProgramUpdateCheckMode::Disabled;
				if (Combo("##SetupFlow_UpdateCheckingMode", mode))
					m_Settings.m_ProgramUpdateCheckMode = mode;

				if (m_Settings.m_ProgramUpdateCheckMode == ProgramUpdateCheckMode::Disabled)
					ImGui::TextUnformatted("You can always check for updates manually via the Help menu.");
			}
			ImGui::Unindent();
			ImGui::EndGroup();
		}, "Requires \"Allow Internet Connectivity\"");

	return OnDrawResult::ContinueDrawing;
}

void NetworkSettingsPage::Init(const Settings& settings)
{
	m_Settings.m_AllowInternetUsage = settings.m_AllowInternetUsage.value_or(true);
	m_Settings.m_ProgramUpdateCheckMode = settings.m_ProgramUpdateCheckMode;
	if (m_Settings.m_ProgramUpdateCheckMode == ProgramUpdateCheckMode::Unknown)
		m_Settings.m_ProgramUpdateCheckMode = ProgramUpdateCheckMode::Releases;
}

bool NetworkSettingsPage::CanCommit() const
{
	return InternalValidateSettings(m_Settings);
}

void NetworkSettingsPage::Commit(Settings& settings)
{
	settings.m_AllowInternetUsage = m_Settings.m_AllowInternetUsage;
	settings.m_ProgramUpdateCheckMode = m_Settings.m_ProgramUpdateCheckMode;
}
