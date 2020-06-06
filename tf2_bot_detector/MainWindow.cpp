#include "MainWindow.h"
#include "ConsoleLines.h"
#include "NetworkStatus.h"
#include "RegexHelpers.h"
#include "ImGui_TF2BotDetector.h"
#include "PeriodicActions.h"
#include "Log.h"
#include "PathUtils.h"
#include "VoiceBanUtils.h"

#include <imgui_desktop/ScopeGuards.h>
#include <imgui_desktop/ImGuiHelpers.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <implot.h>
#include <mh/math/interpolation.hpp>
#include <mh/text/case_insensitive_string.hpp>

#include <cassert>
#include <chrono>
#include <string>
#include <regex>

using namespace tf2_bot_detector;
using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;

MainWindow::MainWindow() :
	ImGuiDesktop::Window(800, 600, "TF2 Bot Detector"),
	m_ActionManager(m_Settings),
        m_VoiceBanUtils(m_Settings)
{
	m_OpenTime = clock_t::now();

	m_ActionManager.AddPeriodicAction<StatusUpdateAction>();
	m_ActionManager.AddPeriodicAction<ConfigAction>();
	//m_ActionManager.AddPiggybackAction<GenericCommandAction>("net_status");
}

MainWindow::~MainWindow()
{
}

static float GetRemainingColumnWidth(float contentRegionWidth, int column_index = -1)
{
	const auto origContentWidth = contentRegionWidth;

	if (column_index == -1)
		column_index = ImGui::GetColumnIndex();

	assert(column_index >= 0);

	const auto columnCount = ImGui::GetColumnsCount();
	for (int i = 0; i < columnCount; i++)
	{
		if (i != column_index)
			contentRegionWidth -= ImGui::GetColumnWidth(i);
	}

	return contentRegionWidth - 3;//ImGui::GetStyle().ItemSpacing.x;
}

void MainWindow::OnDrawScoreboardContextMenu(IPlayer& player)
{
	if (auto popupScope = ImGui::BeginPopupContextItemScope("PlayerContextMenu"))
	{
		ImGuiDesktop::ScopeGuards::StyleColor textColor(ImGuiCol_Text, { 1, 1, 1, 1 });

		const auto steamID = player.GetSteamID();
		if (ImGui::MenuItem("Copy SteamID", nullptr, false, steamID.IsValid()))
			ImGui::SetClipboardText(steamID.str().c_str());

		const auto& world = GetWorld();
		auto& modLogic = GetModLogic();

		if (ImGui::BeginMenu("Votekick",
			(world.GetTeamShareResult(steamID, m_Settings.m_LocalSteamID) == TeamShareResult::SameTeams) && world.FindUserID(steamID)))
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
						Log("Manually marked "s << player << (existingMarked ? "NOT " : "") << PlayerAttributes(i));
				}
			}

			ImGui::EndMenu();
		}

#ifdef _DEBUG
		if (IsWorldValid())
		{
			ImGui::Separator();

			auto& modLogic = GetModLogic();
			bool isRunning = modLogic.IsUserRunningTool(player);
			if (ImGui::MenuItem("Is Running TFBD", nullptr, isRunning))
				modLogic.SetUserRunningTool(player, !isRunning);
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

			if (m_WorldState)
			{
				for (IPlayer* playerPtr : m_WorldState->GeneratePlayerPrintData())
				{
					IPlayer& player = *playerPtr;
					ImGuiDesktop::ScopeGuards::ID idScope((int)player.GetSteamID().Lower32);
					ImGuiDesktop::ScopeGuards::ID idScope2((int)player.GetSteamID().Upper32);

					std::optional<ImGuiDesktop::ScopeGuards::StyleColor> textColor;
					if (player.GetConnectionState() != PlayerStatusState::Active || player.GetName().empty())
						textColor.emplace(ImGuiCol_Text, ImVec4(1, 1, 0, 0.5f));
					else if (player.GetSteamID() == m_Settings.m_LocalSteamID)
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
							if (IsWorldValid())
							{
								auto& modLogic = GetModLogic();
								switch (modLogic.GetTeamShareResult(player))
								{
								case TeamShareResult::SameTeams:      return m_Settings.m_Theme.m_Colors.m_FriendlyTeam;
								case TeamShareResult::OppositeTeams:  return m_Settings.m_Theme.m_Colors.m_EnemyTeam;
								}
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
						ImGui::NextColumn();
					}

					OnDrawScoreboardContextMenu(player);

					// player names column
					{
						if (player.GetName().empty())
							ImGui::TextUnformatted("<Unknown>");
						else
							ImGui::TextUnformatted(player.GetName());

						ImGui::NextColumn();
					}

					// Kills column
					{
						if (player.GetName().empty())
							ImGui::TextRightAligned("?");
						else
							ImGui::TextRightAlignedF("%u", player.GetScores().m_Kills);

						ImGui::NextColumn();
					}

					// Deaths column
					{
						if (player.GetName().empty())
							ImGui::TextRightAligned("?");
						else
							ImGui::TextRightAlignedF("%u", player.GetScores().m_Deaths);

						ImGui::NextColumn();
					}

					// Connected time column
					{
						if (player.GetName().empty())
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
						if (player.GetName().empty())
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
			if (!IsWorldValid())
				return;

			ImGui::PushTextWrapPos();

			for (auto it = m_WorldState->m_PrintingLines.rbegin(); it != m_WorldState->m_PrintingLines.rend(); ++it)
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

			for (const LogMessage* msgPtr : GetLogMsgs())
			{
				const LogMessage& msg = *msgPtr;
				const std::tm timestamp = ToTM(msg.m_Timestamp);

				ImGui::TextColored({ 0.25f, 1.0f, 0.25f, 0.25f }, "[%02i:%02i:%02i]",
					timestamp.tm_hour, timestamp.tm_min, timestamp.tm_sec);

				ImGui::SameLine();
				ImGui::TextColoredUnformatted({ msg.m_Color.r, msg.m_Color.g, msg.m_Color.b, msg.m_Color.a }, msg.m_Text);
			}

			ImGui::PopTextWrapPos();
		});
}

void MainWindow::OnDrawNetGraph()
{
	auto [startTime, endTime] = GetNetSamplesRange();
	if ((endTime - startTime) > (NET_GRAPH_DURATION + 10s))
		PruneNetSamples(startTime, endTime);

	auto delta = GetCurrentTimestampCompensated() - endTime;
	startTime += delta;
	endTime += delta;

	ImPlot::SetNextPlotLimitsX(-to_seconds<float>(NET_GRAPH_DURATION), 0, ImGuiCond_Always);

	constexpr int YAXIS_TIME = 0;
	constexpr int YAXIS_DATA = 1;

	const auto maxLatency = std::max(GetMaxValue(m_NetSamplesIn.m_Latency), GetMaxValue(m_NetSamplesOut.m_Latency));
	ImPlot::SetNextPlotLimitsY(0 - (maxLatency * 0.025f), maxLatency + (maxLatency * 0.025f), ImGuiCond_Always, YAXIS_TIME);

	const auto maxData = std::max(GetMaxValue(m_NetSamplesIn.m_Data), GetMaxValue(m_NetSamplesOut.m_Data));
	ImPlot::SetNextPlotLimitsY(0 - (maxData * 0.025f), maxData + (maxData * 0.025f), ImGuiCond_Always, YAXIS_DATA);

	constexpr ImPlotFlags plotFlags = ImPlotFlags_Default | ImPlotFlags_AntiAliased | ImPlotFlags_YAxis2;
	if (ImPlot::BeginPlot("NetGraph", "Time", nullptr, { -1, 0 }, plotFlags))
	{
		//PlotNetSamples("Ping In", m_NetSamplesIn.m_Latency, startTime, endTime, YAXIS_TIME);
		//PlotNetSamples("Ping Out", m_NetSamplesOut.m_Latency, startTime, endTime, YAXIS_TIME);
		PlotNetSamples("Data In", m_NetSamplesIn.m_Data, startTime, endTime, YAXIS_DATA);
		PlotNetSamples("Data Out", m_NetSamplesOut.m_Data, startTime, endTime, YAXIS_DATA);
		ImPlot::EndPlot();
	}
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
	if (ImGui::BeginPopupModal(POPUP_NAME, &s_Open))
	{
		// Local steamid
		if (InputTextSteamID("My Steam ID", m_Settings.m_LocalSteamID))
			m_Settings.SaveFile();

		// TF game dir
		if (InputTextTFDir("tf directory", m_Settings.m_TFDir))
			m_Settings.SaveFile();

		// Sleep when unfocused
		{
			if (ImGui::Checkbox("Sleep when unfocused", &m_Settings.m_SleepWhenUnfocused))
				m_Settings.SaveFile();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Slows program refresh rate when not focused to reduce CPU/GPU usage.");
		}

		// Auto temp mute
		{
			if (ImGui::Checkbox("Auto temp mute", &m_Settings.m_AutoTempMute))
				m_Settings.SaveFile();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Automatically, temporarily mute ingame chat messages if we think someone else in the server is running the tool.");
		}

                // Add cheaters to mute list
                {
                        ImGui::Separator();

                        if(ImGui::Button("Voice ban known cheaters in TF2")) {
                            ImGui::OpenPopup("Voice Ban Warning");
                        }

                        if (ImGui::BeginPopupModal("Voice Ban Warning"))
                        {
                            ImGui::Text("This button must be used before starting TF2; it will have no effect while TF2 is running. Continue?");
                            
                            if (ImGui::Button("Continue")) {
                                m_VoiceBanUtils.ReadVoiceBans();
                                m_VoiceBanUtils.BanCheaters();
                                m_VoiceBanUtils.WriteVoiceBans();

                                ImGui::CloseCurrentPopup();
                            }

                            if (ImGui::Button("Cancel"))
                                ImGui::CloseCurrentPopup();

                            ImGui::EndPopup();
                        }

                }

		ImGui::EndPopup();
	}
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

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%i of %i (%1.1f%%)", lastSample.m_UsedEdicts, lastSample.m_MaxEdicts, percent * 100);
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
	if (m_SetupFlow.OnDraw(m_Settings))
		return;

	ImGui::Columns(2, "MainWindowSplit");

	ImGui::Checkbox("Pause", &m_Paused); ImGui::SameLine();

	ImGui::Checkbox("Mute", &m_Settings.m_Unsaved.m_Muted); ImGui::SameLine();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Suppresses all in-game chat messages.");

	ImGui::Checkbox("Show Commands", &m_Settings.m_Unsaved.m_DebugShowCommands);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Prints out all game commands to the log.");

	ImGui::Value("Time (Compensated)", to_seconds<float>(GetCurrentTimestampCompensated() - m_OpenTime));

#ifdef _DEBUG
	if (IsWorldValid())
	{
		auto& modLogic = GetModLogic();
		auto leader = modLogic.GetBotLeader();
		ImGui::Value("Bot Leader", leader ? (""s << *leader).c_str() : "");
		ImGui::Value("Time to next connecting cheater warning", to_seconds(modLogic.TimeToConnectingCheaterWarning()));
		ImGui::Value("Time to next cheater warning", to_seconds(modLogic.TimeToCheaterWarning()));
	}
#endif

	if (IsWorldValid())
	{
		auto& world = GetWorld();
		const auto parsedLineCount = world.GetParsedLineCount();
		const auto parseProgress = world.GetParseProgress();

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

	OnDrawSettingsPopup();
}

void MainWindow::OnDrawMenuBar()
{
	if (m_SetupFlow.ShouldDraw())
		return;

#if 0
	if (ImGui::BeginMenu("File"))
	{
		ImGui::EndMenu();
	}
#endif

#ifdef _DEBUG
	if (ImGui::BeginMenu("Debug"))
	{
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
	static bool s_ImPlotDemoWindow = false;
#endif
	if (ImGui::BeginMenu("Window"))
	{
#ifdef _DEBUG
		ImGui::MenuItem("ImGui Demo Window", nullptr, &s_ImGuiDemoWindow);
		ImGui::MenuItem("ImPlot Demo Window", nullptr, &s_ImPlotDemoWindow);
#endif
		ImGui::EndMenu();
	}

#ifdef _DEBUG
	if (s_ImGuiDemoWindow)
		ImGui::ShowDemoWindow(&s_ImGuiDemoWindow);
	if (s_ImPlotDemoWindow)
		ImPlot::ShowDemoWindow(&s_ImPlotDemoWindow);
#endif
#endif

	if (ImGui::MenuItem("Settings"))
		OpenSettingsPopup();
}

bool MainWindow::HasMenuBar() const
{
	if (m_SetupFlow.ShouldDraw())
		return false;

	return true;
}

void MainWindow::OnUpdate()
{
	if (m_SetupFlow.OnUpdate(m_Settings))
	{
		m_WorldState.reset();
		return;
	}
	else if (!m_WorldState)
	{
		m_WorldState.emplace(*this, m_Settings, m_Settings.m_TFDir / "console.log");
	}

	if (!m_Paused && m_WorldState.has_value())
	{
		GetWorld().Update();
		m_ActionManager.Update();
	}
}

void MainWindow::OnTimestampUpdate(WorldState& world)
{
	SetLogTimestamp(world.GetCurrentTime());
}

void MainWindow::OnUpdate(WorldState& world, bool consoleLinesUpdated)
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
	if (parsed.ShouldPrint() && m_WorldState)
	{
		auto& ws = *m_WorldState;
		while (ws.m_PrintingLines.size() > ws.MAX_PRINTING_LINES)
			ws.m_PrintingLines.pop_back();

		ws.m_PrintingLines.insert(ws.m_PrintingLines.begin(), &parsed);
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

mh::generator<IPlayer*> MainWindow::WorldStateExtra::GeneratePlayerPrintData()
{
	IPlayer* printData[33]{};
	auto begin = std::begin(printData);
	auto end = std::end(printData);
	assert(begin <= end);
	auto& world = m_WorldState;
	assert(static_cast<size_t>(end - begin) >= world.GetApproxLobbyMemberCount());

	std::fill(begin, end, nullptr);

	{
		auto* current = begin;
		for (IPlayer* member : world.GetLobbyMembers())
		{
			assert(member);
			*current = member;
			if (*current)
				current++;
		}

		if (current == begin)
		{
			// We seem to have either an empty lobby or we're playing on a community server.
			// Just find the most recent status updates.
			for (IPlayer* playerData : world.GetPlayers())
			{
				assert(playerData);
				if (playerData->GetLastStatusUpdateTime() >= (m_LastStatusUpdateTime - 15s))
				{
					*current = playerData;
					current++;

					if (current >= end)
						break; // This might happen, but we're not in a lobby so everything has to be approximate
				}
			}
		}

		end = current;
	}

	std::sort(begin, end, [](const IPlayer* lhs, const IPlayer* rhs)
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
		co_yield *it;
}

void MainWindow::UpdateServerPing(time_point_t timestamp)
{
	if ((timestamp - m_LastServerPingSample) <= 7s)
		return;

	float totalPing = 0;
	uint16_t samples = 0;

	for (IPlayer* player : GetWorld().GetPlayers())
	{
		if (player->GetLastStatusUpdateTime() < (timestamp - 20s))
			continue;

		auto& data = player->GetOrCreateData<PlayerExtraData>(*player);
		totalPing += data.GetAveragePing();
		samples++;
	}

	m_ServerPingSamples.push_back({ timestamp, uint16_t(totalPing / samples) });
	m_LastServerPingSample = timestamp;

	while ((timestamp - m_ServerPingSamples.front().m_Timestamp) > 5min)
		m_ServerPingSamples.erase(m_ServerPingSamples.begin());
}

std::pair<time_point_t, time_point_t> MainWindow::GetNetSamplesRange() const
{
	time_point_t minTime = time_point_t::max();
	time_point_t maxTime = time_point_t::min();
	bool nonEmpty = false;

	const auto Check = [&](const std::map<time_point_t, AvgSample>& data)
	{
		if (data.empty())
			return;

		minTime = std::min(minTime, data.begin()->first);
		maxTime = std::max(maxTime, std::prev(data.end())->first);
		nonEmpty = true;
	};

	Check(m_NetSamplesIn.m_Latency);
	Check(m_NetSamplesIn.m_Loss);
	Check(m_NetSamplesIn.m_Packets);
	Check(m_NetSamplesIn.m_Data);

	Check(m_NetSamplesOut.m_Latency);
	Check(m_NetSamplesOut.m_Loss);
	Check(m_NetSamplesOut.m_Packets);
	Check(m_NetSamplesOut.m_Data);

	return { minTime, maxTime };
}

void MainWindow::PruneNetSamples(time_point_t& startTime, time_point_t& endTime)
{
	const auto Prune = [&](std::map<time_point_t, AvgSample>& data)
	{
		auto lower = data.lower_bound(endTime - NET_GRAPH_DURATION);
		if (lower == data.begin())
			return;

		data.erase(data.begin(), std::prev(lower));
	};

	Prune(m_NetSamplesIn.m_Latency);
	Prune(m_NetSamplesIn.m_Loss);
	Prune(m_NetSamplesIn.m_Packets);
	Prune(m_NetSamplesIn.m_Data);

	Prune(m_NetSamplesOut.m_Latency);
	Prune(m_NetSamplesOut.m_Loss);
	Prune(m_NetSamplesOut.m_Packets);
	Prune(m_NetSamplesOut.m_Data);

	const auto newRange = GetNetSamplesRange();
	startTime = newRange.first;
	endTime = newRange.second;
}

void MainWindow::PlotNetSamples(const char* label_id, const std::map<time_point_t, AvgSample>& data,
	time_point_t startTime, time_point_t endTime, int yAxis) const
{
	if (data.empty() || startTime == endTime)
		return;

	const auto count = data.size();
	ImVec2* points = reinterpret_cast<ImVec2*>(_malloca(sizeof(ImVec2) * data.size()));
	if (!points)
		throw std::runtime_error("Failed to allocate memory for points");

	{
		const auto startX = ImPlot::GetPlotLimits().X.Min;
		const auto dataStartTime = data.begin()->first;
		const auto dataEndTime = std::prev(data.end())->first;
		const auto startOffset = (startTime - endTime) + (dataStartTime - startTime);
		const float startOffsetFloat = to_seconds<float>(startOffset);
		size_t i = 0;

		for (const auto& entry : data)
		{
			points[i].x = startOffsetFloat + to_seconds<float>(entry.first - dataStartTime);
			points[i].y = entry.second.m_AvgValue;
			i++;
		}
	}

	ImPlot::SetPlotYAxis(yAxis);
	ImPlot::PlotLine(label_id, points, (int)data.size());
	_freea(points);
}

float MainWindow::GetMaxValue(const std::map<time_point_t, AvgSample>& data)
{
	float max = FLT_MIN;
	for (const auto& pair : data)
		max = std::max(max, pair.second.m_AvgValue);

	return max;
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

void MainWindow::AvgSample::AddSample(float value)
{
#if 0
	float delta = m_SampleCount / float(m_SampleCount + 1);
	m_AvgValue = mh::lerp(m_SampleCount / float(m_SampleCount + 1), value, m_AvgValue);
	m_SampleCount++;
#else
	auto old = m_AvgValue;
	m_AvgValue = ((m_AvgValue * m_SampleCount) + value) / (m_SampleCount + 1);
	m_SampleCount++;
#endif
}

MainWindow::WorldStateExtra::WorldStateExtra(MainWindow& window,
	const Settings& settings, const std::filesystem::path& conLogFile) :
	m_WorldState(conLogFile),
	m_ModeratorLogic(m_WorldState, settings, window.m_ActionManager)
{
	m_WorldState.AddConsoleLineListener(&window);
	m_WorldState.AddWorldEventListener(&window);
}

time_point_t MainWindow::GetCurrentTimestampCompensated() const
{
	if (IsWorldValid())
		return m_WorldState->m_WorldState.GetCurrentTime();
	else
		return m_OpenTime;
}
