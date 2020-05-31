#include "SetupFlow.h"
#include "PathUtils.h"
#include "Settings.h"
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

	const auto PrintErrorMsg = [](const std::string_view& msg)
	{
		if (msg.empty())
			return;

		ImGuiDesktop::ScopeGuards::StyleColor text(ImGuiCol_Text, { 1, 0, 0, 1 });
		ImGui::TextUnformatted(msg);
		ImGui::NewLine();
	};

	ImGui::PushTextWrapPos();

	ImGui::TextUnformatted("Welcome to TF2 Bot Detector! Some setup is required before first use."sv);

	ImGui::NewLine();

	// 1. Steam ID
	ImGui::TextUnformatted("1. Your Steam ID"sv);
	ImGui::NewLine();
	ImGui::Indent();
	ImGui::TextUnformatted("Your Steam ID is needed to identify who can be votekicked (same team) and who is on the other team. You can find your Steam ID using sites like steamidfinder.com.");
	{
		std::string steamID;
		if (settings.m_LocalSteamID.IsValid())
			steamID = settings.m_LocalSteamID.str();

		std::string errorMsg;
		const bool modified = ImGui::InputTextWithHint("##SetupFlow_SteamID", "[U:1:1234567890]", &steamID);
		{
			if (steamID.empty())
			{
				ScopeGuards::TextColor textColor({ 1, 1, 0, 1 });
				ImGui::TextUnformatted("Cannot be empty"sv);
			}
			else
			{
				try
				{
					if (modified)
					{
						settings.m_LocalSteamID = SteamID(steamID);
						settings.SaveFile();
					}

					ScopeGuards::TextColor textColor({ 0, 1, 0, 1 });
					ImGui::TextUnformatted("Looks good!");
				}
				catch (const std::invalid_argument& error)
				{
					ScopeGuards::TextColor textColor({ 1, 0, 0, 1 });
					ImGui::TextUnformatted(error.what());
				}
			}
		}

		PrintErrorMsg(errorMsg);
	}
	ImGui::Unindent();
	ImGui::NewLine();

	// 2. TF directory
	ImGui::TextUnformatted("2. Location of your tf directory");
	ImGui::NewLine();
	ImGui::Indent();
	ImGui::TextUnformatted("Your tf directory is needed so this tool knows where to read console output from. It is also needed to automatically run commands in the game.");
	{
		std::string errorMsg;
		std::string pathStr = settings.m_TFDir.string();

		constexpr char EXAMPLE_TF_DIR[] = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Team Fortress 2\\tf";

		bool modified = false;
		if (pathStr.empty())
		{
			pathStr = EXAMPLE_TF_DIR;
			modified = true;
		}

		modified = ImGui::InputTextWithHint("##SetupFlow_TFDir", EXAMPLE_TF_DIR, &pathStr) || modified;
		{
			const std::filesystem::path path(pathStr);
			if (pathStr.empty())
			{
				ScopeGuards::TextColor textColor({ 1, 1, 0, 1 });
				ImGui::TextUnformatted("Cannot be empty"sv);
			}
			else if (const TFDirectoryValidator validationResult(path); !validationResult)
			{
				ScopeGuards::TextColor textColor({ 1, 0, 0, 1 });
				ImGui::TextUnformatted(validationResult.m_Message);
			}
			else
			{
				ScopeGuards::TextColor textColor({ 0, 1, 0, 1 });
				ImGui::TextUnformatted("Looks good!"sv);

				if (modified)
				{
					settings.m_TFDir = pathStr;
					settings.SaveFile();
				}
			}
		}
	}
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
