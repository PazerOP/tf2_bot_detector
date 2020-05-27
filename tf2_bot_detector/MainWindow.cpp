#include "MainWindow.h"
#include "ConsoleLines.h"
#include "NetworkStatus.h"
#include "RegexHelpers.h"
#include "ImGui_TF2BotDetector.h"
#include "PeriodicActions.h"
#include "Log.h"
#include "PathUtils.h"

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
	m_PeriodicActionManager(m_ActionManager)
{
	m_OpenTime = clock_t::now();

	m_PeriodicActionManager.Add<StatusUpdateAction>();

	m_ActionManager.AddPiggybackAction<GenericCommandAction>("net_status");

	m_WorldState.emplace(*this, m_Settings, m_Settings.m_TFDir / "console.log");
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

void MainWindow::OnDrawScoreboardContextMenu(const IPlayer& player)
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
	}
}

void MainWindow::OnDrawScoreboardColorPicker(const char* name, float color[4])
{
	if (ImGui::ColorEdit4(name, color, ImGuiColorEditFlags_NoInputs))
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

	OnDrawScoreboardColorPicker("You", m_Settings.m_Theme.m_Colors.m_ScoreboardYou); ImGui::SameLine();
	OnDrawScoreboardColorPicker("Cheater", m_Settings.m_Theme.m_Colors.m_ScoreboardCheater); ImGui::SameLine();
	OnDrawScoreboardColorPicker("Suspicious", m_Settings.m_Theme.m_Colors.m_ScoreboardSuspicious); ImGui::SameLine();
	OnDrawScoreboardColorPicker("Exploiter", m_Settings.m_Theme.m_Colors.m_ScoreboardExploiter); ImGui::SameLine();
	OnDrawScoreboardColorPicker("Racist", m_Settings.m_Theme.m_Colors.m_ScoreboardRacist); ImGui::SameLine();

	ImGui::NewLine();

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
			const IPlayer* printData[33]{};
			const size_t playerPrintDataCount = GeneratePlayerPrintData(std::begin(printData), std::end(printData));

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

			for (size_t i = 0; i < playerPrintDataCount; i++)
			{
				const auto& player = *printData[i];
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
					ImVec4 bgColor = [&]()
					{
						switch (player.GetTeam())
						{
						case TFTeam::Red: return ImVec4(1.0f, 0.5f, 0.5f, 0.5f);
						case TFTeam::Blue: return ImVec4(0.5f, 0.5f, 1.0f, 0.5f);
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
		//ImGui::EndChild();
	}

	ImGui::EndChild();
}

void MainWindow::OnDrawChat()
{
	ImGui::AutoScrollBox("##fileContents", { 0, 0 }, [&]()
		{
			ImGui::PushTextWrapPos();

			for (auto it = m_PrintingLines.rbegin(); it != m_PrintingLines.rend(); ++it)
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

			ForEachLogMsg([&](const LogMessage& msg)
				{
					const auto timestampRaw = clock_t::to_time_t(msg.m_Timestamp);
					std::tm timestamp{};
					localtime_s(&timestamp, &timestampRaw);

					ImGui::TextColored({ 0.25f, 1.0f, 0.25f, 0.25f }, "[%02i:%02i:%02i]",
						timestamp.tm_hour, timestamp.tm_min, timestamp.tm_sec);

					ImGui::SameLine();
					ImGui::TextColoredUnformatted({ msg.m_Color.r, msg.m_Color.g, msg.m_Color.b, msg.m_Color.a }, msg.m_Text);
				});

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

	//ImGui::SetNextWindowSize()
	if (ImGui::BeginPopupModal(POPUP_NAME, &s_Open))
	{
		const auto PrintErrorMsg = [](const std::string_view& msg)
		{
			if (msg.empty())
				return;

			ImGuiDesktop::ScopeGuards::StyleColor text(ImGuiCol_Text, { 1, 0, 0, 1 });
			ImGui::TextUnformatted(msg);
			ImGui::NewLine();
		};

		// Local steamid
		{
			std::string steamID;
			if (m_Settings.m_LocalSteamID.IsValid())
				steamID = m_Settings.m_LocalSteamID.str();

			std::string errorMsg;
			if (ImGui::InputTextWithHint("My Steam ID", "[U:1:1234567890]", &steamID))
			{
				try
				{
					m_Settings.m_LocalSteamID = SteamID(steamID);
					m_Settings.SaveFile();
				}
				catch (const std::invalid_argument& error)
				{
					errorMsg = error.what();
				}
			}

			PrintErrorMsg(errorMsg);
		}

		// TF game dir
		{
			std::string errorMsg;
			std::string pathStr = m_Settings.m_TFDir.string();
			if (ImGui::InputText("tf directory", &pathStr))
			{
				std::filesystem::path path(pathStr);
				using Result = ValidateTFDirectoryResult;
				switch (ValidateTFDirectory(path))
				{
				case Result::Valid:
					break;
				case Result::DoesNotExist:
					errorMsg << "Path not found!" << path;
					break;
				case Result::NotADirectory:
					errorMsg << "Not a directory!" << path;
					break;
				case Result::InvalidContents:
					errorMsg << "Doesn't look like a real tf directory!";
					break;
				}
			}

			PrintErrorMsg(errorMsg);
		}

		// Sleep when unfocused
		{
			if (ImGui::Checkbox("Sleep when unfocused", &m_Settings.m_SleepWhenUnfocused))
				m_Settings.SaveFile();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Slows program refresh rate when not focused to reduce CPU/GPU usage.");
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
			}, (int)m_ServerPingSamples.size());
	}

	//OnDrawNetGraph();
}

void MainWindow::OnDraw()
{
#if 0
	ImGui::Text("Current application time: %1.2f", std::chrono::duration<float>(std::chrono::steady_clock::now() - m_OpenTime).count());
	ImGui::NewLine();
	if (ImGui::Button("Quit"))
		SetShouldClose(true);
#endif

	ImGui::Checkbox("Pause", &m_Paused);
	ImGui::Value("Time (Compensated)", to_seconds<float>(GetCurrentTimestampCompensated() - m_OpenTime));

	ImGui::Text("Parsed line count: %zu", IsWorldValid() ? GetWorld().GetParsedLineCount() : 0);

	ImGui::Columns(2, "MainWindowSplit");

	OnDrawServerStats();
	OnDrawChat();

	ImGui::NextColumn();

	OnDrawScoreboard();
	OnDrawAppLog();
	ImGui::NextColumn();

	OnDrawSettingsPopup();
}

void MainWindow::OnDrawMenuBar()
{
	if (ImGui::BeginMenu("File"))
	{
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

	if (ImGui::MenuItem("Settings"))
		OpenSettingsPopup();

#if 0
	if (ImGui::BeginMenu("Settings"))
	{
		if (ImGui::MenuItem("Open"))

		if (ImGui::MenuItem("Sleep when unfocused", nullptr, &m_Settings.m_SleepWhenUnfocused))
			m_Settings.SaveFile();

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Slows program refresh rate when not focused to reduce CPU/GPU usage.");

		ImGui::EndMenu();
	}
#endif
}

void MainWindow::OnUpdate()
{
	if (!m_Paused && m_WorldState.has_value())
	{
		GetWorld().Update();

		m_PeriodicActionManager.Update(GetCurrentTimestampCompensated());
		m_ActionManager.Update(GetCurrentTimestampCompensated());
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
	if (parsed.ShouldPrint())
	{
		while (m_PrintingLines.size() > MAX_PRINTING_LINES)
			m_PrintingLines.pop_back();

		m_PrintingLines.insert(m_PrintingLines.begin(), &parsed);
		//if (m_PrintingLineCount >= std::size(m_PrintingLines))
		//	std::copy_backward(std::begin(m_PrintingLines), std::end(m_PrintingLines) - 1, std::begin(m_PrintingLines) + 1);
		//else
		//	std::copy(&m_PrintingLines[0], &m_PrintingLines[m_PrintingLineCount], &m_PrintingLines[1]);

		//m_PrintingLines[0] = &parsed;
		//m_PrintingLineCount++;
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

#if 0
	case ConsoleLineType::Chat:
	{
		auto& chatLine = static_cast<const ChatConsoleLine&>(parsed);
		if (auto count = std::count(chatLine.GetMessage().begin(), chatLine.GetMessage().end(), '\n'); count > 2)
		{
			// Cheater is clearing the chat
			if (auto steamID = world.FindSteamIDForName(chatLine.GetPlayerName()))
			{
				if (SetPlayerAttribute(*steamID, PlayerAttributes::Cheater))
					Log("Marked "s << *steamID << " as cheater (" << count << " newlines in chat message)");
			}
			else
			{
				DelayedChatBan ban{};
				ban.m_Timestamp = chatLine.GetTimestamp();
				ban.m_PlayerName = chatLine.GetPlayerName();
				m_DelayedBans.push_back(ban);

				Log("Unable to find steamid for player with name "s << std::quoted(chatLine.GetPlayerName()));
			}
		}

		// Kick for racism
		{
			auto begin = std::sregex_iterator(chatLine.GetMessage().begin(), chatLine.GetMessage().end(), s_WordRegex);
			auto end = std::sregex_iterator();

			for (auto it = begin; it != end; ++it)
			{
				const std::ssub_match& wordMatch = it->operator[](1);
				const std::string_view word(&*wordMatch.first, wordMatch.length());
				if (mh::case_insensitive_compare(word, "nigger"sv) || mh::case_insensitive_compare(word, "niggers"sv))
				{
					Log("Detected Bad Word in chat: "s << word);
					if (auto found = GetWorld().FindSteamIDForName(chatLine.GetPlayerName()))
					{
						if (SetPlayerAttribute(*found, PlayerAttributes::Racist))
							Log("Marked "s << *found << " as racist (" << std::quoted(word) << " in chat message)");
					}
					else
					{
						Log("Cannot mark "s << std::quoted(chatLine.GetPlayerName()) << " as racist: can't find SteamID for name");
					}
				}
			}
		}

		break;
	}
#endif

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

size_t MainWindow::GeneratePlayerPrintData(const IPlayer** begin, const IPlayer** end) const
{
	if (!IsWorldValid())
		return 0;

	assert(begin <= end);
	auto& world = GetWorld();
	assert(static_cast<size_t>(end - begin) >= world.GetLobbyMemberCount());

	std::fill(begin, end, nullptr);

	{
		auto* current = begin;
		for (const LobbyMember* member : world.GetLobbyMembers())
		{
			assert(member);
			*current = world.FindPlayer(member->m_SteamID);
			if (*current)
				current++;
		}

		if (current == begin)
		{
			// We seem to have either an empty lobby or we're playing on a community server.
			// Just find the most recent status updates.
			for (const IPlayer* playerData : world.GetPlayers())
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
				return killsResult < 0;

			return lhs->GetScores().m_Deaths < rhs->GetScores().m_Deaths;
		});

	return static_cast<size_t>(end - begin);
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
