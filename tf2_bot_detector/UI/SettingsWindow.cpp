#include "SettingsWindow.h"
#include "ImGui_TF2BotDetector.h"
#include "Config/Settings.h"
#include "UI/MainWindow.h"
#include "Util/PathUtils.h"

#include <mh/error/ensure.hpp>

using namespace tf2_bot_detector;

SettingsWindow::SettingsWindow(ImGuiDesktop::Application& app, Settings& settings, MainWindow& mainWindow) :
	ImGuiDesktop::Window(app, 800, 600, "Settings"),
	m_Settings(settings),
	m_MainWindow(mainWindow)
{
	ShowWindow();
}

void SettingsWindow::OnDraw()
{
	if (ImGui::TreeNode("Autodetected Settings Overrides"))
	{
		// Steam dir
		if (InputTextSteamDirOverride("Steam directory", m_Settings.m_SteamDirOverride, true))
			m_Settings.SaveFile();

		// TF game dir override
		if (InputTextTFDirOverride("tf directory", m_Settings.m_TFDirOverride, FindTFDir(m_Settings.GetSteamDir()), true))
			m_Settings.SaveFile();

		// Local steamid
		if (InputTextSteamIDOverride("My Steam ID", m_Settings.m_LocalSteamIDOverride, true))
			m_Settings.SaveFile();

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Logging"))
	{
#ifdef TF2BD_ENABLE_DISCORD_INTEGRATION
		if (ImGui::Checkbox("Discord Rich Presence", &m_Settings.m_Logging.m_DiscordRichPresence))
			m_Settings.SaveFile();
#endif
		if (ImGui::Checkbox("RCON Packets", &m_Settings.m_Logging.m_RCONPackets))
			m_Settings.SaveFile();

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Moderation"))
	{
		// Auto temp mute
		{
			if (ImGui::Checkbox("Auto temp mute", &m_Settings.m_AutoTempMute))
				m_Settings.SaveFile();
			ImGui::SetHoverTooltip("Automatically, temporarily mute ingame chat messages if we think someone else in the server is running the tool.");
		}

		// Auto votekick delay
		{
			if (ImGui::SliderFloat("Auto votekick delay", &m_Settings.m_AutoVotekickDelay, 0, 30, "%1.1f seconds"))
				m_Settings.SaveFile();
			ImGui::SetHoverTooltip("Delay between a player being registered as fully connected and us expecting them to be ready to vote on an issue.\n\n"
				"This is needed because players can't vote until they have joined a team and picked a class. If we call a vote before enough people are ready, it might fail.");
		}

		// Send warnings for connecting cheaters
		{
			if (ImGui::Checkbox("Chat message warnings for connecting cheaters", &m_Settings.m_AutoChatWarningsConnecting))
				m_Settings.SaveFile();

			ImGui::SetHoverTooltip("Automatically sends a chat message if a cheater has joined the lobby,"
				" but is not yet in the game. Only has an effect if \"Enable Chat Warnings\""
				" is enabled (upper left of main window).\n"
				"\n"
				"Looks like: \"Heads up! There are N known cheaters joining the other team! Names unknown until they fully join.\"");
		}

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Performance"))
	{
		// Sleep when unfocused
		{
			if (ImGui::Checkbox("Sleep when unfocused", &m_Settings.m_SleepWhenUnfocused))
				m_Settings.SaveFile();
			ImGui::SetHoverTooltip("Slows program refresh rate when not focused to reduce CPU/GPU usage.");
		}

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Service Integrations"))
	{
		if (ImGui::Checkbox("Discord integrations", &m_Settings.m_Discord.m_EnableRichPresence))
			m_Settings.SaveFile();

#ifdef _DEBUG
		if (ImGui::Checkbox("Lazy Load API Data", &m_Settings.m_LazyLoadAPIData))
			m_Settings.SaveFile();
		ImGui::SetHoverTooltip("If enabled, waits until data is actually needed by the UI before requesting it, saving system resources. Otherwise, instantly loads all data from integration APIs as soon as a player joins the server.");
#endif

		if (bool allowInternet = m_Settings.m_AllowInternetUsage.value_or(false);
			ImGui::Checkbox("Allow internet connectivity", &allowInternet))
		{
			m_Settings.m_AllowInternetUsage = allowInternet;
			m_Settings.SaveFile();
		}

		ImGui::EnabledSwitch(m_Settings.m_AllowInternetUsage.value_or(false), [&](bool enabled)
			{
				ImGui::NewLine();
				if (std::string key = m_Settings.GetSteamAPIKey();
					InputTextSteamAPIKey("Steam API Key", key, true))
				{
					m_Settings.SetSteamAPIKey(key);
					m_Settings.SaveFile();
				}
				ImGui::NewLine();

				if (auto mode = enabled ? m_Settings.m_ReleaseChannel : ReleaseChannel::None;
					Combo("Automatic update checking", mode))
				{
					m_Settings.m_ReleaseChannel = mode;
					m_Settings.SaveFile();
				}
			}, "Requires \"Allow internet connectivity\"");

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("UI"))
	{
		float& fontGlobalScale = ImGui::GetIO().FontGlobalScale;
		if (ImGui::Button("Reset"))
		{
			m_Settings.m_Theme.m_GlobalScale = fontGlobalScale = 1;
			m_Settings.SaveFile();
		}
		ImGui::SameLineNoPad();
		if (ImGui::SliderFloat("Global UI Scale", &fontGlobalScale, 0.5f, 2.0f,
			"%1.2f", ImGuiSliderFlags_AlwaysClamp))
		{
			m_Settings.m_Theme.m_GlobalScale = fontGlobalScale;
			m_Settings.SaveFile();
		}

		const auto GetFontComboString = [](Font f)
		{
			switch (f)
			{
			default:
				LogError("Unknown font {}", mh::enum_fmt(f));
				[[fallthrough]];
			case Font::ProggyClean_13px: return "Proggy Clean, 13px";
			case Font::ProggyClean_26px: return "Proggy Clean, 26px";
			case Font::ProggyTiny_10px:  return "Proggy Tiny, 10px";
			case Font::ProggyTiny_20px:  return "Proggy Tiny, 20px";
			}
		};

		if (ImGui::BeginCombo("Font", GetFontComboString(m_Settings.m_Theme.m_Font)))
		{
			const auto FontSelectable = [&](Font f)
			{
				ImFont* fontPtr = mh_ensure(m_MainWindow.GetFontPointer(f));
				if (!fontPtr)
					return;

				if (ImGui::Selectable(GetFontComboString(f), m_Settings.m_Theme.m_Font == f))
				{
					//static bool s_HasPushedFont
					ImGui::GetIO().FontDefault = fontPtr;
					m_Settings.m_Theme.m_Font = f;
					m_Settings.SaveFile();
				}
			};

			// FIXME
			FontSelectable(Font::ProggyTiny_10px);
			FontSelectable(Font::ProggyTiny_20px);
			FontSelectable(Font::ProggyClean_13px);
			FontSelectable(Font::ProggyClean_26px);

			ImGui::EndCombo();
		}

		ImGui::TreePop();
	}

	ImGui::NewLine();

	if (AutoLaunchTF2Checkbox(m_Settings.m_AutoLaunchTF2))
		m_Settings.SaveFile();
}
