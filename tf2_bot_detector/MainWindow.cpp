#include "MainWindow.h"
#include "ConsoleLog/ConsoleLines.h"
#include "Networking/GithubAPI.h"
#include "ConsoleLog/NetworkStatus.h"
#include "RegexHelpers.h"
#include "Platform/Platform.h"
#include "ImGui_TF2BotDetector.h"
#include "Actions/ActionGenerators.h"
#include "Log.h"
#include "PathUtils.h"
#include "Version.h"

#include <imgui_desktop/ScopeGuards.h>
#include <imgui_desktop/ImGuiHelpers.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <mh/math/interpolation.hpp>
#include <mh/text/case_insensitive_string.hpp>
#include <mh/text/stringops.hpp>

#include <cassert>
#include <chrono>
#include <string>
#include <regex>

using namespace tf2_bot_detector;
using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;

MainWindow::MainWindow() :
	ImGuiDesktop::Window(800, 600, ("TF2 Bot Detector v"s << VERSION).c_str())
{
	m_WorldState.AddConsoleLineListener(this);
	m_WorldState.AddWorldEventListener(this);

	DebugLog("Debug Info:"s
		<< "\n\tSteam dir:         " << m_Settings.GetSteamDir()
		<< "\n\tTF dir:            " << m_Settings.GetTFDir()
		<< "\n\tSteamID:           " << m_Settings.GetLocalSteamID()
		<< "\n\tVersion:           " << VERSION
		<< "\n\tIs CI Build:       " << std::boolalpha << (TF2BD_IS_CI_COMPILE ? true : false)
		<< "\n\tCompile Timestamp: " << __TIMESTAMP__
#ifdef _MSC_FULL_VER
		<< "\n\t-D _MSC_FULL_VER:  " << _MSC_FULL_VER
#endif
#if _M_X64
		<< "\n\t-D _M_X64:         " << _M_X64
#endif
#if _MT
		<< "\n\t-D _MT:            " << _MT
#endif
	);

	m_OpenTime = clock_t::now();

	m_ActionManager.AddPeriodicActionGenerator<StatusUpdateActionGenerator>();
	m_ActionManager.AddPeriodicActionGenerator<ConfigActionGenerator>();
	m_ActionManager.AddPeriodicActionGenerator<LobbyDebugActionGenerator>();
	//m_ActionManager.AddPiggybackAction<GenericCommandAction>("net_status");
}

MainWindow::~MainWindow()
{
}

void MainWindow::OnDrawScoreboardContextMenu(IPlayer& player)
{
	if (auto popupScope = ImGui::BeginPopupContextItemScope("PlayerContextMenu"))
	{
		ImGuiDesktop::ScopeGuards::StyleColor textColor(ImGuiCol_Text, { 1, 1, 1, 1 });

		const auto steamID = player.GetSteamID();
		if (ImGui::MenuItem("Copy SteamID", nullptr, false, steamID.IsValid()))
			ImGui::SetClipboardText(steamID.str().c_str());

		if (ImGui::BeginMenu("Go To"))
		{
			if (!m_Settings.m_GotoProfileSites.empty())
			{
				for (const auto& item : m_Settings.m_GotoProfileSites)
				{
					ImGuiDesktop::ScopeGuards::ID id(&item);
					if (ImGui::MenuItem(item.m_Name.c_str()))
						Shell::OpenURL(item.CreateProfileURL(player));
				}
			}
			else
			{
				ImGui::MenuItem("No sites configured", nullptr, nullptr, false);
			}

			ImGui::EndMenu();
		}

		const auto& world = GetWorld();
		auto& modLogic = GetModLogic();

		if (ImGui::BeginMenu("Votekick",
			(world.GetTeamShareResult(steamID, m_Settings.GetLocalSteamID()) == TeamShareResult::SameTeams) && world.FindUserID(steamID)))
		{
			if (ImGui::MenuItem("Cheating"))
				modLogic.InitiateVotekick(player, KickReason::Cheating);
			if (ImGui::MenuItem("Idle"))
				modLogic.InitiateVotekick(player, KickReason::Idle);
			if (ImGui::MenuItem("Other"))
				modLogic.InitiateVotekick(player, KickReason::Other);
			if (ImGui::MenuItem("Scamming"))
				modLogic.InitiateVotekick(player, KickReason::Scamming);

			ImGui::EndMenu();
		}

		ImGui::Separator();

		if (ImGui::BeginMenu("Mark"))
		{
			for (int i = 0; i < (int)PlayerAttributes::COUNT; i++)
			{
				const bool existingMarked = modLogic.HasPlayerAttribute(player, PlayerAttributes(i));

				std::string name;
				name << PlayerAttributes(i);
				if (ImGui::MenuItem(name.c_str(), nullptr, existingMarked))
				{
					if (modLogic.SetPlayerAttribute(player, PlayerAttributes(i), !existingMarked))
						Log("Manually marked "s << player << ' ' << (existingMarked ? "NOT" : "") << ' ' << PlayerAttributes(i));
				}
			}

			ImGui::EndMenu();
		}

#ifdef _DEBUG
		ImGui::Separator();

		if (bool isRunning = m_ModeratorLogic.IsUserRunningTool(player);
			ImGui::MenuItem("Is Running TFBD", nullptr, isRunning))
		{
			m_ModeratorLogic.SetUserRunningTool(player, !isRunning);
		}
#endif
	}
}

void MainWindow::OnDrawScoreboardColorPicker(const char* name, float color[4])
{
	if (ImGui::ColorEdit4(name, color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreview))
		m_Settings.SaveFile();
}

void MainWindow::OnDrawScoreboard()
{
	static float frameWidth, contentWidth, windowContentWidth, windowWidth;
	bool forceRecalc = false;

	static float contentWidthMin = 500;
	static float extraWidth;
#if 0
	ImGui::Value("Work rect width", frameWidth);
	ImGui::Value("Content width", contentWidth);
	ImGui::Value("Window content width", windowContentWidth);
	ImGui::Value("Window width", windowWidth);

	forceRecalc |= ImGui::DragFloat("Content width min", &contentWidthMin);

	forceRecalc |= ImGui::DragFloat("Extra width", &extraWidth, 0.5f);
#endif

	// Horizontal scroller for color pickers
	{
		static float s_ColorScrollerHeight = 1;
		if (ImGui::BeginChild("ColorScroller", { 0, s_ColorScrollerHeight }, false, ImGuiWindowFlags_HorizontalScrollbar))
		{
			OnDrawScoreboardColorPicker("You", m_Settings.m_Theme.m_Colors.m_ScoreboardYou); ImGui::SameLine();
			OnDrawScoreboardColorPicker("Connecting", m_Settings.m_Theme.m_Colors.m_ScoreboardConnecting); ImGui::SameLine();
			OnDrawScoreboardColorPicker("Friendly", m_Settings.m_Theme.m_Colors.m_FriendlyTeam); ImGui::SameLine();
			OnDrawScoreboardColorPicker("Enemy", m_Settings.m_Theme.m_Colors.m_EnemyTeam); ImGui::SameLine();
			OnDrawScoreboardColorPicker("Cheater", m_Settings.m_Theme.m_Colors.m_ScoreboardCheater); ImGui::SameLine();
			OnDrawScoreboardColorPicker("Suspicious", m_Settings.m_Theme.m_Colors.m_ScoreboardSuspicious); ImGui::SameLine();
			OnDrawScoreboardColorPicker("Exploiter", m_Settings.m_Theme.m_Colors.m_ScoreboardExploiter); ImGui::SameLine();
			OnDrawScoreboardColorPicker("Racist", m_Settings.m_Theme.m_Colors.m_ScoreboardRacist); ImGui::SameLine();

			const auto xPos = ImGui::GetCursorPosX();

			ImGui::NewLine();

			s_ColorScrollerHeight = ImGui::GetCursorPosY();
			if (ImGui::GetWindowSize().x < xPos)
				s_ColorScrollerHeight += ImGui::GetStyle().ScrollbarSize;
		}
		ImGui::EndChild();
	}

	ImGui::SetNextWindowContentSizeConstraints(ImVec2(contentWidthMin, -1), ImVec2(-1, -1));
	//ImGui::SetNextWindowContentSize(ImVec2(500, 0));
	if (ImGui::BeginChild("Scoreboard", { 0, ImGui::GetContentRegionAvail().y / 2 }, true, ImGuiWindowFlags_HorizontalScrollbar))
	{
		//ImGui::SetNextWindowSizeConstraints(ImVec2(500, -1), ImVec2(INFINITY, -1));
		//if (ImGui::BeginChild("ScoreboardScrollRegion", { 0, 0 }, false, ImGuiWindowFlags_AlwaysAutoResize))
		{
			static ImVec2 s_LastFrameSize;
			const bool scoreboardResized = [&]()
			{
				const auto thisFrameSize = ImGui::GetWindowSize();
				const bool changed = s_LastFrameSize != thisFrameSize;
				s_LastFrameSize = thisFrameSize;
				return changed || forceRecalc;
			}();

			/*const auto*/ frameWidth = ImGui::GetWorkRectSize().x;
			/*const auto*/ contentWidth = ImGui::GetContentRegionMax().x;
			/*const auto*/ windowContentWidth = ImGui::GetWindowContentRegionWidth();
			/*const auto*/ windowWidth = ImGui::GetWindowWidth();
			ImGui::Columns(7, "PlayersColumns");

			// Columns setup
			{
				float nameColumnWidth = frameWidth;
				// UserID header and column setup
				{
					ImGui::TextUnformatted("User ID");
					if (scoreboardResized)
					{
						const float width = ImGui::GetItemRectSize().x + ImGui::GetStyle().ItemSpacing.x * 2;
						nameColumnWidth -= width;
						ImGui::SetColumnWidth(-1, width);
					}

					ImGui::NextColumn();
				}

				// Name header and column setup
				ImGui::TextUnformatted("Name"); ImGui::NextColumn();

				// Kills header and column setup
				{
					ImGui::TextUnformatted("Kills");
					if (scoreboardResized)
					{
						const float width = ImGui::GetItemRectSize().x + ImGui::GetStyle().ItemSpacing.x * 2;
						nameColumnWidth -= width;
						ImGui::SetColumnWidth(-1, width);
					}

					ImGui::NextColumn();
				}

				// Deaths header and column setup
				{
					ImGui::TextUnformatted("Deaths");
					if (scoreboardResized)
					{
						const float width = ImGui::GetItemRectSize().x + ImGui::GetStyle().ItemSpacing.x * 2;
						nameColumnWidth -= width;
						ImGui::SetColumnWidth(-1, width);
					}

					ImGui::NextColumn();
				}

				// Connection time header and column setup
				{
					ImGui::TextUnformatted("Time");
					if (scoreboardResized)
					{
						const float width = 60;
						nameColumnWidth -= width;
						ImGui::SetColumnWidth(-1, width);
					}

					ImGui::NextColumn();
				}

				// Ping
				{
					ImGui::TextUnformatted("Ping");
					if (scoreboardResized)
					{
						const float width = ImGui::GetItemRectSize().x + ImGui::GetStyle().ItemSpacing.x * 2;
						nameColumnWidth -= width;
						ImGui::SetColumnWidth(-1, width);
					}

					ImGui::NextColumn();
				}

				// SteamID header and column setup
				{
					ImGui::TextUnformatted("Steam ID");
					if (scoreboardResized)
					{
						nameColumnWidth -= 100;// +ImGui::GetStyle().ItemSpacing.x * 2;
						ImGui::SetColumnWidth(1, std::max(10.0f, nameColumnWidth - ImGui::GetStyle().ItemSpacing.x * 2 + extraWidth));
					}

					ImGui::NextColumn();
				}
				ImGui::Separator();
			}

			if (m_ConsoleLogParser)
			{
				for (IPlayer& player : m_ConsoleLogParser->GeneratePlayerPrintData())
				{
					const auto& playerName = player.GetNameSafe();
					ImGuiDesktop::ScopeGuards::ID idScope((int)player.GetSteamID().Lower32);
					ImGuiDesktop::ScopeGuards::ID idScope2((int)player.GetSteamID().Upper32);

					std::optional<ImGuiDesktop::ScopeGuards::StyleColor> textColor;
					if (player.GetConnectionState() != PlayerStatusState::Active || player.GetNameSafe().empty())
						textColor.emplace(ImGuiCol_Text, ImVec4(1, 1, 0, 0.5f));
					else if (player.GetSteamID() == m_Settings.GetLocalSteamID())
						textColor.emplace(ImGuiCol_Text, m_Settings.m_Theme.m_Colors.m_ScoreboardYou);

					char buf[32];
					if (!player.GetUserID().has_value())
					{
						buf[0] = '?';
						buf[1] = '\0';
					}
					else if (auto result = mh::to_chars(buf, player.GetUserID().value()))
						*result.ptr = '\0';
					else
						continue;

					// Selectable
					{
						ImVec4 bgColor = [&]() -> ImVec4
						{
							switch (m_ModeratorLogic.GetTeamShareResult(player))
							{
							case TeamShareResult::SameTeams:      return m_Settings.m_Theme.m_Colors.m_FriendlyTeam;
							case TeamShareResult::OppositeTeams:  return m_Settings.m_Theme.m_Colors.m_EnemyTeam;
							}

							switch (player.GetTeam())
							{
							case TFTeam::Red:   return ImVec4(1.0f, 0.5f, 0.5f, 0.5f);
							case TFTeam::Blue:  return ImVec4(0.5f, 0.5f, 1.0f, 0.5f);
							default: return ImVec4(0.5f, 0.5f, 0.5f, 0);
							}
						}();

						const auto& modLogic = GetModLogic();
						if (modLogic.HasPlayerAttribute(player, PlayerAttributes::Cheater))
							bgColor = mh::lerp(TimeSine(), bgColor, ImVec4(m_Settings.m_Theme.m_Colors.m_ScoreboardCheater));
						else if (modLogic.HasPlayerAttribute(player, PlayerAttributes::Suspicious))
							bgColor = mh::lerp(TimeSine(), bgColor, ImVec4(m_Settings.m_Theme.m_Colors.m_ScoreboardSuspicious));
						else if (modLogic.HasPlayerAttribute(player, PlayerAttributes::Exploiter))
							bgColor = mh::lerp(TimeSine(), bgColor, ImVec4(m_Settings.m_Theme.m_Colors.m_ScoreboardExploiter));
						else if (modLogic.HasPlayerAttribute(player, PlayerAttributes::Racist))
							bgColor = mh::lerp(TimeSine(), bgColor, ImVec4(m_Settings.m_Theme.m_Colors.m_ScoreboardRacist));

						ImGuiDesktop::ScopeGuards::StyleColor styleColorScope(ImGuiCol_Header, bgColor);

						bgColor.w = std::min(bgColor.w + 0.25f, 0.8f);
						ImGuiDesktop::ScopeGuards::StyleColor styleColorScopeHovered(ImGuiCol_HeaderHovered, bgColor);

						bgColor.w = std::min(bgColor.w + 0.5f, 1.0f);
						ImGuiDesktop::ScopeGuards::StyleColor styleColorScopeActive(ImGuiCol_HeaderActive, bgColor);
						ImGui::Selectable(buf, true, ImGuiSelectableFlags_SpanAllColumns);

						if (player.GetSteamID() != m_Settings.GetLocalSteamID() && ImGui::IsItemHovered())
						{
							ImGui::BeginTooltip();
							auto kills = player.GetScores().m_LocalKills;
							auto deaths = player.GetScores().m_LocalDeaths;
							//ImGui::Text("Your Thirst: %1.0f%%", kills == 0 ? float(deaths) * 100 : float(deaths) / kills * 100);
							ImGui::Text("Their Thirst: %1.0f%%", deaths == 0 ? float(kills) * 100 : float(kills) / deaths * 100);
							ImGui::EndTooltip();
						}

						ImGui::NextColumn();
					}

					OnDrawScoreboardContextMenu(player);

					// player names column
					{
						if (!playerName.empty())
							ImGui::TextUnformatted(playerName);
						else if (const SteamAPI::PlayerSummary* summary = player.GetPlayerSummary(); summary && !summary->m_Nickname.empty())
							ImGui::TextUnformatted(summary->m_Nickname);
						else
							ImGui::TextUnformatted("<Unknown>");

						// If their steamcommunity name doesn't match their ingame name
						if (auto summary = player.GetPlayerSummary();
							summary && !playerName.empty() && summary->m_Nickname != playerName)
						{
							ImGui::SameLine();
							ImGui::TextColored({ 1, 0, 0, 1 }, "(%s)", summary->m_Nickname.c_str());
						}

						ImGui::NextColumn();
					}

					// Kills column
					{
						if (playerName.empty())
							ImGui::TextRightAligned("?");
						else
							ImGui::TextRightAlignedF("%u", player.GetScores().m_Kills);

						ImGui::NextColumn();
					}

					// Deaths column
					{
						if (playerName.empty())
							ImGui::TextRightAligned("?");
						else
							ImGui::TextRightAlignedF("%u", player.GetScores().m_Deaths);

						ImGui::NextColumn();
					}

					// Connected time column
					{
						if (playerName.empty())
						{
							ImGui::TextRightAligned("?");
						}
						else
						{
							ImGui::TextRightAlignedF("%u:%02u",
								std::chrono::duration_cast<std::chrono::minutes>(player.GetConnectedTime()).count(),
								std::chrono::duration_cast<std::chrono::seconds>(player.GetConnectedTime()).count() % 60);
						}

						ImGui::NextColumn();
					}

					// Ping column
					{
						if (playerName.empty())
							ImGui::TextRightAligned("?");
						else
							ImGui::TextRightAlignedF("%u", player.GetPing());

						ImGui::NextColumn();
					}

					// Steam ID column
					{
						if (player.GetSteamID().Type != SteamAccountType::Invalid)
							textColor.reset(); // Draw steamid in normal color

						ImGui::TextUnformatted(player.GetSteamID().str());
						ImGui::NextColumn();
					}
				}
			}
		}
		//ImGui::EndChild();
	}

	ImGui::EndChild();
}

void MainWindow::OnDrawChat()
{
	ImGui::AutoScrollBox("##fileContents", { 0, 0 }, [&]()
		{
			if (!m_ConsoleLogParser)
				return;

			ImGui::PushTextWrapPos();

			for (auto it = m_ConsoleLogParser->m_PrintingLines.rbegin(); it != m_ConsoleLogParser->m_PrintingLines.rend(); ++it)
			{
				assert(*it);
				(*it)->Print();
			}

			ImGui::PopTextWrapPos();
		});
}

void MainWindow::OnDrawAppLog()
{
	ImGui::AutoScrollBox("AppLog", { 0, 0 }, [&]()
		{
			ImGui::PushTextWrapPos();

			for (const LogMessage& msg : GetVisibleLogMsgs())
			{
				const std::tm timestamp = ToTM(msg.m_Timestamp);

				ImGuiDesktop::ScopeGuards::ID id(&msg);

				ImGui::BeginGroup();
				ImGui::TextColored({ 0.25f, 1.0f, 0.25f, 0.25f }, "[%02i:%02i:%02i]",
					timestamp.tm_hour, timestamp.tm_min, timestamp.tm_sec);

				ImGui::SameLine();
				ImGui::TextColoredUnformatted({ msg.m_Color.r, msg.m_Color.g, msg.m_Color.b, msg.m_Color.a }, msg.m_Text);
				ImGui::EndGroup();

				if (auto scope = ImGui::BeginPopupContextItemScope("AppLogContextMenu"))
				{
					if (ImGui::MenuItem("Copy"))
						ImGui::SetClipboardText(msg.m_Text.c_str());
				}
			}

			ImGui::PopTextWrapPos();
		});
}

void MainWindow::OnDrawSettingsPopup()
{
	static constexpr char POPUP_NAME[] = "Settings##Popup";

	static bool s_Open = false;
	if (m_SettingsPopupOpen)
	{
		m_SettingsPopupOpen = false;
		ImGui::OpenPopup(POPUP_NAME);
		s_Open = true;
	}

	ImGui::SetNextWindowSize({ 400, 400 }, ImGuiCond_Once);
	if (ImGui::BeginPopupModal(POPUP_NAME, &s_Open, ImGuiWindowFlags_HorizontalScrollbar))
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

		// Sleep when unfocused
		{
			if (ImGui::Checkbox("Sleep when unfocused", &m_Settings.m_SleepWhenUnfocused))
				m_Settings.SaveFile();
			ImGui::SetHoverTooltip("Slows program refresh rate when not focused to reduce CPU/GPU usage.");
		}

		if (AutoLaunchTF2Checkbox(m_Settings.m_AutoLaunchTF2))
			m_Settings.SaveFile();

		// Auto temp mute
		{
			if (ImGui::Checkbox("Auto temp mute", &m_Settings.m_AutoTempMute))
				m_Settings.SaveFile();
			ImGui::SetHoverTooltip("Automatically, temporarily mute ingame chat messages if we think someone else in the server is running the tool.");
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

		if (bool allowInternet = m_Settings.m_AllowInternetUsage.value_or(false);
			ImGui::Checkbox("Allow internet connectivity", &allowInternet))
		{
			m_Settings.m_AllowInternetUsage = allowInternet;
			m_Settings.SaveFile();
		}

		ImGui::EnabledSwitch(m_Settings.m_AllowInternetUsage.value_or(false), [&](bool enabled)
			{
				auto mode = enabled ? m_Settings.m_ProgramUpdateCheckMode : ProgramUpdateCheckMode::Disabled;

				if (Combo("Automatic update checking", mode))
				{
					m_Settings.m_ProgramUpdateCheckMode = mode;
					m_Settings.SaveFile();
				}
			}, "Requires \"Allow internet connectivity\"");

		ImGui::EndPopup();
	}
}

void MainWindow::OnDrawUpdateCheckPopup()
{
	static constexpr char POPUP_NAME[] = "Check for Updates##Popup";

	static bool s_Open = false;
	if (m_UpdateCheckPopupOpen)
	{
		m_UpdateCheckPopupOpen = false;
		ImGui::OpenPopup(POPUP_NAME);
		s_Open = true;
	}

	ImGui::SetNextWindowSize({ 500, 300 }, ImGuiCond_Appearing);
	if (ImGui::BeginPopupModal(POPUP_NAME, &s_Open, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::PushTextWrapPos();
		ImGui::TextUnformatted("You have chosen to disable internet connectivity for TF2 Bot Detector. You can still manually check for updates below.");
		ImGui::TextColoredUnformatted({ 1, 1, 0, 1 }, "Reminder: if you use antivirus software, connecting to the internet may trigger warnings.");

		ImGui::EnabledSwitch(!m_UpdateInfo.valid(), [&]
			{
				if (ImGui::Button("Check for updates"))
					GetUpdateInfo();
			});

		ImGui::NewLine();

		if (mh::is_future_ready(m_UpdateInfo))
		{
			auto& updateInfo = m_UpdateInfo.get();

			if (updateInfo.IsUpToDate())
			{
				ImGui::TextColoredUnformatted({ 0.1f, 1, 0.1f, 1 }, "You are already running the latest version of TF2 Bot Detector.");
			}
			else if (updateInfo.IsPreviewAvailable())
			{
				ImGui::TextUnformatted("There is a new preview version available.");
				if (ImGui::Button("View on Github"))
					Shell::OpenURL(updateInfo.m_Preview->m_URL);
			}
			else if (updateInfo.IsReleaseAvailable())
			{
				ImGui::TextUnformatted("There is a new stable version available.");
				if (ImGui::Button("View on Github"))
					Shell::OpenURL(updateInfo.m_Stable->m_URL);
			}
			else if (updateInfo.IsError())
			{
				ImGui::TextColoredUnformatted({ 1, 0, 0, 1 }, "There was an error checking for updates.");
			}
		}
		else if (m_UpdateInfo.valid())
		{
			ImGui::TextUnformatted("Checking for updates...");
		}
		else
		{
			ImGui::TextUnformatted("Press \"Check for updates\" to check Github for a newer version.");
		}

		ImGui::EndPopup();
	}
}

void MainWindow::OpenUpdateCheckPopup()
{
	m_NotifyOnUpdateAvailable = false;
	m_UpdateCheckPopupOpen = true;
}

void MainWindow::OnDrawUpdateAvailablePopup()
{
	static constexpr char POPUP_NAME[] = "Update Available##Popup";

	static bool s_Open = false;
	if (m_UpdateAvailablePopupOpen)
	{
		m_UpdateAvailablePopupOpen = false;
		ImGui::OpenPopup(POPUP_NAME);
		s_Open = true;
	}

	if (ImGui::BeginPopupModal(POPUP_NAME, &s_Open, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("There is a new"s << (m_UpdateInfo.get().IsPreviewAvailable() ? " preview" : "")
			<< " version of TF2 Bot Detector available for download.");

		if (ImGui::Button("View on Github"))
			Shell::OpenURL(m_UpdateInfo.get().GetURL());

		ImGui::EndPopup();
	}
}

void MainWindow::OpenUpdateAvailablePopup()
{
	m_NotifyOnUpdateAvailable = false;
	m_UpdateAvailablePopupOpen = true;
}

void MainWindow::OnDrawAboutPopup()
{
	static constexpr char POPUP_NAME[] = "About##Popup";

	static bool s_Open = false;
	if (m_AboutPopupOpen)
	{
		m_AboutPopupOpen = false;
		ImGui::OpenPopup(POPUP_NAME);
		s_Open = true;
	}

	ImGui::SetNextWindowSize({ 600, 450 }, ImGuiCond_Appearing);
	if (ImGui::BeginPopupModal(POPUP_NAME, &s_Open))
	{
		ImGui::PushTextWrapPos();

		static const std::string ABOUT_TEXT =
			"TF2 Bot Detector v"s << VERSION << "\n"
			"\n"
			"Automatically detects and votekicks cheaters in Team Fortress 2 Casual.\n"
			"\n"
			"This program is free, open source software licensed under the MIT license. Full license text"
			" for this program and its dependencies can be found in the licenses subfolder next to this"
			" executable.";

		ImGui::TextUnformatted(ABOUT_TEXT);

		ImGui::NewLine();
		ImGui::Separator();
		ImGui::NewLine();

		ImGui::TextUnformatted("Credits");
		ImGui::Spacing();
		if (ImGui::TreeNode("Code/concept by Matt \"pazer\" Haynie"))
		{
			if (ImGui::Selectable("GitHub - PazerOP", false, ImGuiSelectableFlags_DontClosePopups))
				Shell::OpenURL("https://github.com/PazerOP");
			if (ImGui::Selectable("Twitter - @PazerFromSilver", false, ImGuiSelectableFlags_DontClosePopups))
				Shell::OpenURL("https://twitter.com/PazerFromSilver");

			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Artwork/icon by S-Purple"))
		{
			if (ImGui::Selectable("Twitter (NSFW)", false, ImGuiSelectableFlags_DontClosePopups))
				Shell::OpenURL("https://twitter.com/spurpleheart");

			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Documentation/moderation by Nicholas \"ClusterConsultant\" Flamel"))
		{
			if (ImGui::Selectable("GitHub - ClusterConsultant", false, ImGuiSelectableFlags_DontClosePopups))
				Shell::OpenURL("https://github.com/ClusterConsultant");

			ImGui::TreePop();
		}

		ImGui::NewLine();
		ImGui::Separator();
		ImGui::NewLine();

		if (const auto sponsors = m_SponsorsList.GetSponsors(); !sponsors.empty())
		{
			ImGui::TextUnformatted("Sponsors\n"
				"Huge thanks to the people sponsoring this project via GitHub Sponsors:");

			ImGui::NewLine();

			for (const auto& sponsor : sponsors)
			{
				ImGui::Bullet();
				ImGui::TextUnformatted(sponsor.m_Name);
				ImGui::SameLine();
				ImGui::TextUnformatted("-");
				ImGui::SameLine();
				ImGui::TextUnformatted(sponsor.m_Message);
			}

			ImGui::NewLine();
		}

		ImGui::TextUnformatted("If you're feeling generous, you can make a small donation to help support my work.");
		if (ImGui::Button("GitHub Sponsors"))
			Shell::OpenURL("https://github.com/sponsors/PazerOP");

		ImGui::EndPopup();
	}
}

#include <libzippp/libzippp.h>
void MainWindow::GenerateDebugReport()
{
	Log("Generating debug_report.zip...");
	{
		using namespace libzippp;
		ZipArchive archive("debug_report.zip");
		archive.open(ZipArchive::NEW);

		if (!archive.addFile("console.log", (m_Settings.GetTFDir() / "console.log").string()))
		{
			LogError("Failed to add console.log to debug report");
		}
		if (!archive.addFile("tf2bd.log", GetLogFilename().string()))
		{
			LogError("Failed to add tf2bd log to debug report");
		}

		if (auto err = archive.close(); err != LIBZIPPP_OK)
		{
			LogError("Failed to close debug report zip archive: close() returned "s << err);
			return;
		}
	}
	Log("Finished generating debug_report.zip.");
	Shell::ExploreToAndSelect("debug_report.zip");
}

void MainWindow::OnDrawServerStats()
{
	ImGui::PlotLines("Edicts", [&](int idx)
		{
			return m_EdictUsageSamples[idx].m_UsedEdicts;
		}, (int)m_EdictUsageSamples.size(), 0, nullptr, 0, 2048);

	if (!m_EdictUsageSamples.empty())
	{
		ImGui::SameLine(0, 4);

		auto& lastSample = m_EdictUsageSamples.back();
		char buf[32];
		const float percent = float(lastSample.m_UsedEdicts) / lastSample.m_MaxEdicts;
		sprintf_s(buf, "%i (%1.0f%%)", lastSample.m_UsedEdicts, percent * 100);
		ImGui::ProgressBar(percent, { -1, 0 }, buf);

		ImGui::SetHoverTooltip("%i of %i (%1.1f%%)", lastSample.m_UsedEdicts, lastSample.m_MaxEdicts, percent * 100);
	}

	if (!m_ServerPingSamples.empty())
	{
		char buf[64];
		sprintf_s(buf, "Average ping: %u", m_ServerPingSamples.back().m_Ping);
		ImGui::PlotLines(buf, [&](int idx)
			{
				return m_ServerPingSamples[idx].m_Ping;
			}, (int)m_ServerPingSamples.size(), 0, nullptr, 0);
	}

	//OnDrawNetGraph();
}

void MainWindow::OnDraw()
{
	OnDrawSettingsPopup();
	OnDrawUpdateAvailablePopup();
	OnDrawUpdateCheckPopup();
	OnDrawAboutPopup();

	{
		ISetupFlowPage::DrawState ds;
		ds.m_ActionManager = &m_ActionManager;
		ds.m_Settings = &m_Settings;

		if (m_SetupFlow.OnDraw(m_Settings, ds))
			return;
	}

	ImGui::Columns(2, "MainWindowSplit");

	{
		static float s_SettingsScrollerHeight = 1;
		if (ImGui::BeginChild("SettingsScroller", { 0, s_SettingsScrollerHeight }, false, ImGuiWindowFlags_HorizontalScrollbar))
		{
			ImGui::Checkbox("Pause", &m_Paused); ImGui::SameLine();

			auto& settings = m_Settings;
			const auto ModerationCheckbox = [&settings](const char* name, bool& value, const char* tooltip)
			{
				{
					ImGuiDesktop::ScopeGuards::TextColor text({ 1, 0.5f, 0, 1 }, !value);
					if (ImGui::Checkbox(name, &value))
						settings.SaveFile();
				}

				const char* orangeReason = "";
				if (!value)
					orangeReason = "\n\nThis label is orange to highlight the fact that it is currently disabled.";

				ImGui::SameLine();
				ImGui::SetHoverTooltip("%s%s", tooltip, orangeReason);
			};

			ModerationCheckbox("Enable Chat Warnings", m_Settings.m_AutoChatWarnings, "Enables chat message warnings about cheaters.");
			ModerationCheckbox("Enable Auto Votekick", m_Settings.m_AutoVotekick, "Automatically votekicks cheaters on your team.");
			ModerationCheckbox("Enable Auto-mark", m_Settings.m_AutoMark, "Automatically marks players matching the detection rules.");

			ImGui::Checkbox("Show Commands", &m_Settings.m_Unsaved.m_DebugShowCommands); ImGui::SameLine();
			ImGui::SetHoverTooltip("Prints out all game commands to the log.");

			const auto xPos = ImGui::GetCursorPosX();

			ImGui::NewLine();

			s_SettingsScrollerHeight = ImGui::GetCursorPosY();
			if (ImGui::GetWindowSize().x < xPos)
				s_SettingsScrollerHeight += ImGui::GetStyle().ScrollbarSize;
		}
		ImGui::EndChild();
	}

	ImGui::Value("Time (Compensated)", to_seconds<float>(GetCurrentTimestampCompensated() - m_OpenTime));

#ifdef _DEBUG
	{
		auto leader = m_ModeratorLogic.GetBotLeader();
		ImGui::Value("Bot Leader", leader ? (""s << *leader).c_str() : "");

		ImGui::TextUnformatted("Is vote in progress:");
		ImGui::SameLine();
		if (m_WorldState.IsVoteInProgress())
			ImGui::TextColoredUnformatted({ 1, 1, 0, 1 }, "YES");
		else
			ImGui::TextColoredUnformatted({ 0, 1, 0, 1 }, "NO");
	}
#endif

	ImGui::Value("Blackslisted user count", m_ModeratorLogic.GetBlacklistedPlayerCount());
	ImGui::Value("Rule count", m_ModeratorLogic.GetRuleCount());

	if (m_ConsoleLogParser)
	{
		auto& world = GetWorld();
		const auto parsedLineCount = m_ConsoleLogParser->m_Parser.GetParsedLineCount();
		const auto parseProgress = m_ConsoleLogParser->m_Parser.GetParseProgress();

		if (parseProgress < 0.95f)
		{
			char overlayStr[64];
			sprintf_s(overlayStr, "%1.2f %%", parseProgress * 100);
			ImGui::ProgressBar(parseProgress, { 0, 0 }, overlayStr);
			ImGui::SameLine(0, 4);
		}

		ImGui::Text("Parsed line count: %zu", parsedLineCount);
	}

	//OnDrawServerStats();
	OnDrawChat();

	ImGui::NextColumn();

	OnDrawScoreboard();
	OnDrawAppLog();
	ImGui::NextColumn();
}

void MainWindow::OnDrawMenuBar()
{
	const bool isInSetupFlow = m_SetupFlow.ShouldDraw();

	if (ImGui::BeginMenu("File"))
	{
		if (!isInSetupFlow)
		{
			if (ImGui::MenuItem("Reload Playerlists/Rules"))
				GetModLogic().ReloadConfigFiles();
			if (ImGui::MenuItem("Reload Settings"))
				m_Settings.LoadFile();
			if (ImGui::MenuItem("Generate Debug Report"))
				GenerateDebugReport();

			ImGui::Separator();
		}

		if (ImGui::MenuItem("Exit", "Alt+F4"))
			SetShouldClose(true);

		ImGui::EndMenu();
	}

#ifdef _DEBUG
	if (ImGui::BeginMenu("Debug"))
	{
		if (ImGui::MenuItem("Reload Localization Files"))
			m_ActionManager.QueueAction<GenericCommandAction>("cl_reload_localization_files");

		ImGui::Separator();

		if (ImGui::MenuItem("Crash"))
		{
			struct Test
			{
				int i;
			};

			Test* testPtr = nullptr;
			testPtr->i = 42;
		}
		ImGui::EndMenu();
	}

#ifdef _DEBUG
	static bool s_ImGuiDemoWindow = false;
#endif
	if (ImGui::BeginMenu("Window"))
	{
#ifdef _DEBUG
		ImGui::MenuItem("ImGui Demo Window", nullptr, &s_ImGuiDemoWindow);
#endif
		ImGui::EndMenu();
	}

#ifdef _DEBUG
	if (s_ImGuiDemoWindow)
		ImGui::ShowDemoWindow(&s_ImGuiDemoWindow);
#endif
#endif

	if (!isInSetupFlow)
	{
		if (ImGui::MenuItem("Settings"))
			OpenSettingsPopup();
	}

	if (ImGui::BeginMenu("Help"))
	{
		if (ImGui::MenuItem("Open GitHub"))
			Shell::OpenURL("https://github.com/PazerOP/tf2_bot_detector");
		if (ImGui::MenuItem("Open Discord"))
			Shell::OpenURL("https://discord.gg/W8ZSh3Z");

		ImGui::Separator();

		char buf[128];
		sprintf_s(buf, "Version: %s", VERSION_STRING);
		ImGui::MenuItem(buf, nullptr, false, false);

		if (m_Settings.m_AllowInternetUsage.value_or(false))
		{
			auto newVersion = GetUpdateInfo();
			if (!newVersion)
			{
				ImGui::MenuItem("Checking for new version...", nullptr, nullptr, false);
			}
			else if (newVersion->IsUpToDate())
			{
				ImGui::MenuItem("Up to date!", nullptr, nullptr, false);
			}
			else if (newVersion->IsReleaseAvailable())
			{
				ImGuiDesktop::ScopeGuards::TextColor green({ 0, 1, 0, 1 });
				if (ImGui::MenuItem("A new version is available"))
					Shell::OpenURL(newVersion->m_Stable->m_URL);
			}
			else if (newVersion->IsPreviewAvailable())
			{
				if (ImGui::MenuItem("A new preview is available"))
					Shell::OpenURL(newVersion->m_Preview->m_URL);
			}
			else
			{
				assert(newVersion->IsError());
				ImGui::MenuItem("Error occurred checking for new version.", nullptr, nullptr, false);
			}
		}
		else
		{
			if (ImGui::MenuItem("Check for updates..."))
				OpenUpdateCheckPopup();
		}

		ImGui::Separator();

		if (ImGui::MenuItem("About TF2 Bot Detector"))
			OpenAboutPopup();

		ImGui::EndMenu();
	}
}

GithubAPI::NewVersionResult* MainWindow::GetUpdateInfo()
{
	if (!m_UpdateInfo.valid())
	{
		if (auto client = m_Settings.GetHTTPClient())
			m_UpdateInfo = GithubAPI::CheckForNewVersion(*client);
		else
			return nullptr;
	}

	if (mh::is_future_ready(m_UpdateInfo))
		return const_cast<GithubAPI::NewVersionResult*>(&m_UpdateInfo.get());

	return nullptr;
}

void MainWindow::HandleUpdateCheck()
{
	if (!m_NotifyOnUpdateAvailable)
		return;

	if (!m_Settings.m_AllowInternetUsage.value_or(false))
		return;

	const bool checkPreviews = m_Settings.m_ProgramUpdateCheckMode == ProgramUpdateCheckMode::Previews;
	const bool checkReleases = checkPreviews || m_Settings.m_ProgramUpdateCheckMode == ProgramUpdateCheckMode::Releases;
	if (!checkPreviews && !checkReleases)
		return;

	auto result = GetUpdateInfo();
	if (!result)
		return;

	if ((result->IsPreviewAvailable() && checkPreviews) ||
		(result->IsReleaseAvailable() && checkReleases))
	{
		OpenUpdateAvailablePopup();
	}
}

void MainWindow::OnUpdate()
{
	if (m_Paused)
		return;

#ifdef _WIN32
	m_HijackActionManager.Update();
#endif

	m_WorldState.Update();
	m_ActionManager.Update();

	HandleUpdateCheck();

	if (m_SetupFlow.OnUpdate(m_Settings))
	{
		m_ConsoleLogParser.reset();
		return;
	}
	else if (!m_ConsoleLogParser)
	{
		m_ConsoleLogParser.emplace(*this);
	}
	else if (m_ConsoleLogParser)
	{
		m_ConsoleLogParser->m_Parser.Update();
		m_ModeratorLogic.Update();
	}
}

void MainWindow::OnConsoleLogChunkParsed(WorldState& world, bool consoleLinesUpdated)
{
	assert(&world == &GetWorld());

	QueueUpdate();

	if (consoleLinesUpdated)
		UpdateServerPing(GetCurrentTimestampCompensated());
}

bool MainWindow::IsSleepingEnabled() const
{
	return m_Settings.m_SleepWhenUnfocused && !HasFocus();
}

bool MainWindow::IsTimeEven() const
{
	const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(clock_t::now() - m_OpenTime);
	return !(seconds.count() % 2);
}

float MainWindow::TimeSine(float interval, float min, float max) const
{
	const auto elapsed = (clock_t::now() - m_OpenTime) % std::chrono::duration_cast<clock_t::duration>(std::chrono::duration<float>(interval));
	const auto progress = std::chrono::duration<float>(elapsed).count() / interval;
	return mh::remap(std::sin(progress * 6.28318530717958647693f), -1.0f, 1.0f, min, max);
}

void MainWindow::OnConsoleLineParsed(WorldState& world, IConsoleLine& parsed)
{
	if (parsed.ShouldPrint() && m_ConsoleLogParser)
	{
		auto& parser = *m_ConsoleLogParser;
		while (parser.m_PrintingLines.size() > parser.MAX_PRINTING_LINES)
			parser.m_PrintingLines.pop_back();

		parser.m_PrintingLines.push_front(parsed.shared_from_this());
	}

	switch (parsed.GetType())
	{
	case ConsoleLineType::LobbyChanged:
	{
		auto& lobbyChangedLine = static_cast<const LobbyChangedLine&>(parsed);
		const LobbyChangeType changeType = lobbyChangedLine.GetChangeType();

		if (changeType == LobbyChangeType::Created || changeType == LobbyChangeType::Updated)
			m_ActionManager.QueueAction<LobbyUpdateAction>();

		break;
	}
	case ConsoleLineType::EdictUsage:
	{
		auto& usageLine = static_cast<const EdictUsageLine&>(parsed);
		m_EdictUsageSamples.push_back({ usageLine.GetTimestamp(), usageLine.GetUsedEdicts(), usageLine.GetTotalEdicts() });

		while (m_EdictUsageSamples.front().m_Timestamp < (usageLine.GetTimestamp() - 5min))
			m_EdictUsageSamples.erase(m_EdictUsageSamples.begin());

		break;
	}
	}
}

MainWindow::ConsoleLogParserExtra::ConsoleLogParserExtra(MainWindow& parent) :
	m_Parent(&parent),
	m_Parser(parent.m_WorldState, parent.m_Settings, parent.m_Settings.GetTFDir() / "console.log")
{
}

cppcoro::generator<IPlayer&> MainWindow::ConsoleLogParserExtra::GeneratePlayerPrintData()
{
	IPlayer* printData[33]{};
	auto begin = std::begin(printData);
	auto end = std::end(printData);
	assert(begin <= end);
	auto& world = m_Parent->m_WorldState;
	assert(static_cast<size_t>(end - begin) >= world.GetApproxLobbyMemberCount());

	std::fill(begin, end, nullptr);

	{
		auto* current = begin;
		for (IPlayer& member : world.GetLobbyMembers())
		{
			*current = &member;
			current++;
		}

		if (current == begin)
		{
			// We seem to have either an empty lobby or we're playing on a community server.
			// Just find the most recent status updates.
			for (IPlayer& playerData : world.GetPlayers())
			{
				if (playerData.GetLastStatusUpdateTime() >= (m_Parent->GetWorld().GetLastStatusUpdateTime() - 15s))
				{
					*current = &playerData;
					current++;

					if (current >= end)
						break; // This might happen, but we're not in a lobby so everything has to be approximate
				}
			}
		}

		end = current;
	}

	std::sort(begin, end, [](const IPlayer* lhs, const IPlayer* rhs) -> bool
		{
			assert(lhs);
			assert(rhs);
			//if (!lhs && !rhs)
			//	return false;
			//if (auto result = !!rhs <=> !!lhs; !std::is_eq(result))
			//	return result < 0;

			// Intentionally reversed, we want descending kill order
			if (auto killsResult = rhs->GetScores().m_Kills <=> lhs->GetScores().m_Kills; !std::is_eq(killsResult))
				return std::is_lt(killsResult);

			if (auto deathsResult = lhs->GetScores().m_Deaths <=> rhs->GetScores().m_Deaths; !std::is_eq(deathsResult))
				return std::is_lt(deathsResult);

			// Sort by ascending userid
			{
				auto luid = lhs->GetUserID();
				auto ruid = rhs->GetUserID();
				if (luid && ruid)
				{
					if (auto result = *luid <=> *ruid; !std::is_eq(result))
						return std::is_lt(result);
				}
			}

			return false;
		});

	for (auto it = begin; it != end; ++it)
		co_yield **it;
}

void MainWindow::UpdateServerPing(time_point_t timestamp)
{
	if ((timestamp - m_LastServerPingSample) <= 7s)
		return;

	float totalPing = 0;
	uint16_t samples = 0;

	for (IPlayer& player : GetWorld().GetPlayers())
	{
		if (player.GetLastStatusUpdateTime() < (timestamp - 20s))
			continue;

		auto& data = player.GetOrCreateData<PlayerExtraData>(player);
		totalPing += data.GetAveragePing();
		samples++;
	}

	m_ServerPingSamples.push_back({ timestamp, uint16_t(totalPing / samples) });
	m_LastServerPingSample = timestamp;

	while ((timestamp - m_ServerPingSamples.front().m_Timestamp) > 5min)
		m_ServerPingSamples.erase(m_ServerPingSamples.begin());
}

time_point_t MainWindow::GetLastStatusUpdateTime() const
{
	return GetWorld().GetLastStatusUpdateTime();
}

float MainWindow::PlayerExtraData::GetAveragePing() const
{
	//throw std::runtime_error("TODO");
#if 1
	unsigned totalPing = m_Parent->GetPing();
	unsigned samples = 1;

	for (const auto& entry : m_PingHistory)
	{
		totalPing += entry.m_Ping;
		samples++;
	}

	return totalPing / float(samples);
#endif
}

time_point_t MainWindow::GetCurrentTimestampCompensated() const
{
	return m_WorldState.GetCurrentTime();
}
