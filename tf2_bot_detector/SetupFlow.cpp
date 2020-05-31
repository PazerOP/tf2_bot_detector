#include "SetupFlow.h"
#include "Config/Settings.h"
#include "PathUtils.h"
#include "ImGui_TF2BotDetector.h"

#include <imgui.h>
#include <imgui_desktop/ScopeGuards.h>
#include <misc/cpp/imgui_stdlib.h>
#include <mh/text/string_insertion.hpp>

#include <optional>
#include <string_view>

using namespace std::string_view_literals;
using namespace tf2_bot_detector;
namespace ScopeGuards = ImGuiDesktop::ScopeGuards;

bool SetupFlow::OnUpdate(const Settings& settings)
{
	if (m_ShouldDraw)
		return true;

	m_ShouldDraw = !ValidateSettings(settings);
	return m_ShouldDraw;
}

bool SetupFlow::ValidateSettings(const Settings& settings)
{
	if (!settings.m_LocalSteamID.IsValid())
		return false;

	if (!TFDirectoryValidator(settings.m_TFDir))
		return false;

	return true;
}

bool SetupFlow::OnDraw(Settings& settings)
{
	if (!m_ShouldDraw)
		return false;

	ImGui::PushTextWrapPos();

	ImGui::TextUnformatted("Welcome to TF2 Bot Detector! Some setup is required before first use."sv);

	ImGui::NewLine();

	// 1. Steam ID
	ImGui::TextUnformatted("1. Your Steam ID"sv);
	ImGui::NewLine();
	ImGui::Indent();
	ImGui::TextUnformatted("Your Steam ID is needed to identify who can be votekicked (same team) and who is on the other team. You can find your Steam ID using sites like steamidfinder.com.");

	if (InputTextSteamID("##SetupFlow_SteamID", settings.m_LocalSteamID))
		settings.SaveFile();

	ImGui::Unindent();
	ImGui::NewLine();

	// 2. TF directory
	ImGui::TextUnformatted("2. Location of your tf directory");
	ImGui::NewLine();
	ImGui::Indent();
	ImGui::TextUnformatted("Your tf directory is needed so this tool knows where to read console output from. It is also needed to automatically run commands in the game.");

	if (InputTextTFDir("##SetupFlow_TFDir", settings.m_TFDir))
		settings.SaveFile();

	ImGui::Unindent();

	ImGui::NewLine();
	{
		const bool areSettingsValid = ValidateSettings(settings);

		std::optional<ScopeGuards::GlobalAlpha> disabled;
		if (!areSettingsValid)
			disabled.emplace(0.5f);

		if (ImGui::Button("Done") && areSettingsValid)
			m_ShouldDraw = false;
	}

	return true;
}
