#include "MainWindow.h"
#include "ConsoleLines.h"
#include "NetworkStatus.h"
#include "RegexHelpers.h"
#include "ImGui_TF2BotDetector.h"
#include "PeriodicActions.h"
#include "Log.h"

#include <imgui_desktop/ScopeGuards.h>
#include <imgui_desktop/ImGuiHelpers.h>
#include <imgui.h>
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
	m_PeriodicActionManager(m_ActionManager),

	m_PlayerLists{
		PlayerList("playerlist_cheaters.txt"),
		PlayerList("playerlist_suspicious.txt"),
		PlayerList("playerlist_exploiters.txt"),
		PlayerList("playerlist_racism.txt"),
	}
{
	m_OpenTime = clock_t::now();
	AddConsoleLineListener(this);

	m_PeriodicActionManager.Add<StatusUpdateAction>();

	m_ActionManager.AddPiggybackAction<GenericCommandAction>("net_status");
}

MainWindow::~MainWindow()
{
}

void MainWindow::AddConsoleLineListener(IConsoleLineListener* listener)
{
	if (listener)
		m_ConsoleLineListeners.insert(listener);
}

bool MainWindow::RemoveConsoleLineListener(IConsoleLineListener* listener)
{
	return m_ConsoleLineListeners.erase(listener) > 0;
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

void MainWindow::OnDrawScoreboardContextMenu(const SteamID& steamID)
{
	if (auto popupScope = ImGui::BeginPopupContextItemScope("PlayerContextMenu"))
	{
		ImGuiDesktop::ScopeGuards::StyleColor textColor(ImGuiCol_Text, { 1, 1, 1, 1 });

		if (ImGui::MenuItem("Copy SteamID", nullptr, false, steamID.IsValid()))
			ImGui::SetClipboardText(steamID.str().c_str());

		if (ImGui::BeginMenu("Votekick", (GetTeamShareResult(steamID) == TeamShareResult::SameTeams) && FindUserID(steamID)))
		{
			if (ImGui::MenuItem("Cheating"))
				InitiateVotekick(steamID, KickReason::Cheating);
			if (ImGui::MenuItem("Idle"))
				InitiateVotekick(steamID, KickReason::Idle);
			if (ImGui::MenuItem("Other"))
				InitiateVotekick(steamID, KickReason::Other);
			if (ImGui::MenuItem("Scamming"))
				InitiateVotekick(steamID, KickReason::Scamming);

			ImGui::EndMenu();
		}

		ImGui::Separator();

		if (ImGui::BeginMenu("Mark"))
		{
			for (int i = 0; i < (int)PlayerMarkType::COUNT; i++)
			{
				const bool existingMarked = m_PlayerLists[i].IsPlayerIncluded(steamID);

				std::string name;
				name << PlayerMarkType(i);
				if (ImGui::MenuItem(name.c_str(), nullptr, existingMarked))
				{
					if (MarkPlayer(steamID, PlayerMarkType(i), !existingMarked))
						Log("Manually marked "s << steamID << (existingMarked ? "NOT " : "") << PlayerMarkType(i));
				}
			}

			ImGui::EndMenu();
		}
	}
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

			/*const auto*/ frameWidth = std::max(contentWidthMin, windowContentWidth);// ImGui::GetWorkRectSize().x;
			PlayerPrintData printData[33]{};
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
				const auto& player = printData[i];
				ImGuiDesktop::ScopeGuards::ID idScope((int)player.m_SteamID.Lower32);
				ImGuiDesktop::ScopeGuards::ID idScope2((int)player.m_SteamID.Upper32);

				std::optional<ImGuiDesktop::ScopeGuards::StyleColor> disabledCol;
				if (player.m_State != PlayerStatusState::Active || player.m_Name.empty())
					disabledCol.emplace(ImGuiCol_Text, ImVec4(1, 1, 0, 0.5f));

				char buf[32];
				if (player.m_UserID == 0)
				{
					buf[0] = '?';
					buf[1] = '\0';
				}
				else if (auto result = mh::to_chars(buf, player.m_UserID))
					*result.ptr = '\0';
				else
					continue;

				// Selectable
				{
					ImVec4 bgColor = [&]()
					{
						switch (player.m_Team)
						{
						case TFTeam::Red: return ImVec4(1.0f, 0.5f, 0.5f, 0.5f);
						case TFTeam::Blue: return ImVec4(0.5f, 0.5f, 1.0f, 0.5f);
						default: return ImVec4(0, 0, 0, 0);
						}
					}();

					if (IsPlayerMarked(player.m_SteamID, PlayerMarkType::Cheater))
						bgColor = mh::lerp(TimeSine(), bgColor, ImVec4(1, 0, 1, 1));
					else if (IsPlayerMarked(player.m_SteamID, PlayerMarkType::Suspicious))
						bgColor = mh::lerp(TimeSine(), bgColor, ImVec4(1, 1, 0, 1));
					else if (IsPlayerMarked(player.m_SteamID, PlayerMarkType::Exploiter))
						bgColor = mh::lerp(TimeSine(), bgColor, ImVec4(0, 1, 1, 1));
					else if (IsPlayerMarked(player.m_SteamID, PlayerMarkType::Racist))
						bgColor = mh::lerp(TimeSine(), bgColor, ImVec4(1, 1, 1, 1));

					ImGuiDesktop::ScopeGuards::StyleColor styleColorScope(ImGuiCol_Header, bgColor);

					bgColor.w = 0.8f;
					ImGuiDesktop::ScopeGuards::StyleColor styleColorScopeHovered(ImGuiCol_HeaderHovered, bgColor);

					bgColor.w = 1.0f;
					ImGuiDesktop::ScopeGuards::StyleColor styleColorScopeActive(ImGuiCol_HeaderActive, bgColor);
					ImGui::Selectable(buf, true, ImGuiSelectableFlags_SpanAllColumns);
					ImGui::NextColumn();
				}

				OnDrawScoreboardContextMenu(player.m_SteamID);

				// player names column
				{
					if (player.m_Name.empty())
						ImGui::TextUnformatted("<Unknown>");
					else
						ImGui::TextUnformatted(player.m_Name);

					ImGui::NextColumn();
				}

				// Kills column
				{
					if (player.m_Name.empty())
						ImGui::TextRightAligned("?");
					else
						ImGui::TextRightAlignedF("%u", player.m_Scores.m_Kills);

					ImGui::NextColumn();
				}

				// Deaths column
				{
					if (player.m_Name.empty())
						ImGui::TextRightAligned("?");
					else
						ImGui::TextRightAlignedF("%u", player.m_Scores.m_Deaths);

					ImGui::NextColumn();
				}

				// Connected time column
				{
					if (player.m_Name.empty())
					{
						ImGui::TextRightAligned("?");
					}
					else
					{
						assert(player.m_ConnectedTime >= 0s);
						ImGui::TextRightAlignedF("%u:%02u",
							std::chrono::duration_cast<std::chrono::minutes>(player.m_ConnectedTime).count(),
							std::chrono::duration_cast<std::chrono::seconds>(player.m_ConnectedTime).count() % 60);
					}

					ImGui::NextColumn();
				}

				// Ping column
				{
					if (player.m_Name.empty())
						ImGui::TextRightAligned("?");
					else
						ImGui::TextRightAlignedF("%u", player.m_Ping);

					ImGui::NextColumn();
				}

				// Steam ID column
				{
					if (player.m_SteamID.Type != SteamAccountType::Invalid)
						disabledCol.reset(); // Draw steamid in normal color

					ImGui::TextUnformatted(player.m_SteamID.str());
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

			for (auto it = &m_PrintingLines[m_PrintingLineCount - 1]; it != &m_PrintingLines[-1]; --it)
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

	ImGui::Text("Parsed line count: %zu", m_ParsedLineCount);

	ImGui::Columns(2, "MainWindowSplit");

	OnDrawServerStats();
	OnDrawChat();

	ImGui::NextColumn();

	OnDrawScoreboard();
	OnDrawAppLog();
	ImGui::NextColumn();
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

	if (ImGui::BeginMenu("Settings"))
	{
		ImGui::MenuItem("Sleep when unfocused", nullptr, &m_IsSleepingEnabled);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Slows program refresh rate when not focused to reduce CPU/GPU usage.");

		ImGui::EndMenu();
	}
}

static std::regex s_TimestampRegex(R"regex(\n(\d\d)\/(\d\d)\/(\d\d\d\d) - (\d\d):(\d\d):(\d\d):[ \n])regex", std::regex::optimize);
void MainWindow::OnUpdate()
{
	if (m_Paused)
		return;

	if (!m_File)
	{
		{
			FILE* temp = _fsopen("C:\\Program Files (x86)\\Steam\\steamapps\\common\\Team Fortress 2\\tf\\console.log", "r", _SH_DENYNO);
			m_File.reset(temp);
		}

		if (m_File)
			m_ConsoleLines.clear();
	}

	bool snapshotUpdated = false;
	const auto TrySnapshot = [&]()
	{
		if (!snapshotUpdated || !m_CurrentTimestamp.IsSnapshotValid())
		{
			m_CurrentTimestamp.Snapshot();
			snapshotUpdated = true;
		}
	};

	bool consoleLinesUpdated = false;
	if (m_File)
	{
		char buf[4096];
		size_t readCount;
		using clock = std::chrono::steady_clock;
		const auto startTime = clock::now();
		do
		{
			readCount = fread(buf, sizeof(buf[0]), std::size(buf), m_File.get());
			if (readCount > 0)
			{
				m_FileLineBuf.append(buf, readCount);

				auto regexBegin = m_FileLineBuf.cbegin();
				std::smatch match;
				while (std::regex_search(regexBegin, m_FileLineBuf.cend(), match, s_TimestampRegex))
				{
					if (m_CurrentTimestamp.IsRecordedValid())
					{
						// If we have a valid snapshot, that means that there was a previously parsed
						// timestamp. The contents of that line is the current timestamp match's prefix
						// (the previous timestamp match was erased from the string)

						TrySnapshot();
						SetLogTimestamp(m_CurrentTimestamp.GetSnapshot());

						const auto prefix = match.prefix();
						//const auto suffix = match.suffix();
						const std::string_view lineStr(&*prefix.first, prefix.length());
						auto parsed = IConsoleLine::ParseConsoleLine(lineStr, m_CurrentTimestamp.GetSnapshot());

						if (parsed)
						{
							for (auto listener : m_ConsoleLineListeners)
								listener->OnConsoleLineParsed(*parsed);

							m_ConsoleLines.push_back(std::move(parsed));
							consoleLinesUpdated = true;
						}
						else
						{
							for (auto listener : m_ConsoleLineListeners)
								listener->OnConsoleLineUnparsed(m_CurrentTimestamp.GetSnapshot(), lineStr);
						}
					}

					std::tm time{};
					time.tm_isdst = -1;
					from_chars_throw(match[1], time.tm_mon);
					time.tm_mon -= 1;

					from_chars_throw(match[2], time.tm_mday);
					from_chars_throw(match[3], time.tm_year);
					time.tm_year -= 1900;

					from_chars_throw(match[4], time.tm_hour);
					from_chars_throw(match[5], time.tm_min);
					from_chars_throw(match[6], time.tm_sec);

					m_CurrentTimestamp.SetRecorded(clock_t::from_time_t(std::mktime(&time)));
					regexBegin = match[0].second;
				}

				m_ParsedLineCount += std::count(m_FileLineBuf.cbegin(), regexBegin, '\n');
				m_FileLineBuf.erase(m_FileLineBuf.begin(), regexBegin);
			}

			if (auto elapsed = clock::now() - startTime; elapsed >= 100ms)
				break;

		} while (readCount > 0);

		if (readCount > 0)
			QueueUpdate();

		if (consoleLinesUpdated)
			UpdatePrintingLines();
	}

	TrySnapshot();
	SetLogTimestamp(m_CurrentTimestamp.GetSnapshot());

	ProcessPlayerActions();
	m_PeriodicActionManager.Update(m_CurrentTimestamp.GetSnapshot());
	m_ActionManager.Update(m_CurrentTimestamp.GetSnapshot());

	if (consoleLinesUpdated)
		UpdateServerPing(m_CurrentTimestamp.GetSnapshot());
}

bool MainWindow::IsSleepingEnabled() const
{
	return m_IsSleepingEnabled && !HasFocus();
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

static const std::regex s_WordRegex(R"regex((\w+))regex", std::regex::optimize);
void MainWindow::OnConsoleLineParsed(IConsoleLine& parsed)
{
	//const auto tsComp = GetCurrentTimestampCompensated();
	const auto ClearLobbyState = [&]
	{
		m_CurrentLobbyMembers.clear();
		m_PendingLobbyMembers.clear();
		m_CurrentPlayerData.clear();
	};

	switch (parsed.GetType())
	{
	case ConsoleLineType::LobbyHeader:
	{
		auto& headerLine = static_cast<const LobbyHeaderLine&>(parsed);
		m_CurrentLobbyMembers.resize(headerLine.GetMemberCount());
		m_PendingLobbyMembers.resize(headerLine.GetPendingCount());
		break;
	}
	case ConsoleLineType::LobbyChanged:
	{
		auto& lobbyChangedLine = static_cast<const LobbyChangedLine&>(parsed);
		const LobbyChangeType changeType = lobbyChangedLine.GetChangeType();

		if (changeType == LobbyChangeType::Created)
		{
			ClearLobbyState();
		}

		if (changeType == LobbyChangeType::Created || changeType == LobbyChangeType::Updated)
		{
			m_ActionManager.QueueAction(std::make_unique<LobbyUpdateAction>());
			// We can't trust the existing client indices
			for (auto& player : m_CurrentPlayerData)
				player.second.m_ClientIndex = 0;
		}
		break;
	}
	case ConsoleLineType::ClientReachedServerSpawn:
	{
		// Reset current lobby members/player statuses
		//ClearLobbyState();
		break;
	}

	case ConsoleLineType::VoiceReceive:
	{
		auto& voiceReceiveLine = static_cast<const VoiceReceiveLine&>(parsed);
		for (auto& player : m_CurrentPlayerData)
		{
			if (player.second.m_ClientIndex == (voiceReceiveLine.GetEntIndex() + 1))
			{
				auto& voice = player.second.m_Voice;
				if (voice.m_LastTransmission != parsed.GetTimestamp())
				{
					voice.m_TotalTransmissions += 1s; // This is fine because we know the resolution of our timestamps is 1 second
					voice.m_LastTransmission = parsed.GetTimestamp();
				}

				break;
			}
		}
		break;
	}

	case ConsoleLineType::Chat:
	{
		auto& chatLine = static_cast<const ChatConsoleLine&>(parsed);
		if (auto count = std::count(chatLine.GetMessage().begin(), chatLine.GetMessage().end(), '\n'); count > 2)
		{
			// Cheater is clearing the chat
			if (auto steamID = FindSteamIDForName(chatLine.GetPlayerName()))
			{
				if (MarkPlayer(*steamID, PlayerMarkType::Cheater))
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
					if (auto found = FindSteamIDForName(chatLine.GetPlayerName()))
					{
						if (MarkPlayer(*found, PlayerMarkType::Racist))
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

	case ConsoleLineType::LobbyMember:
	{
		auto& memberLine = static_cast<const LobbyMemberLine&>(parsed);
		const auto& member = memberLine.GetLobbyMember();
		auto& vec = member.m_Pending ? m_PendingLobbyMembers : m_CurrentLobbyMembers;
		if (member.m_Index < vec.size())
			vec[member.m_Index] = member;

		const TFTeam tfTeam = member.m_Team == LobbyMemberTeam::Defenders ? TFTeam::Red : TFTeam::Blue;
		m_CurrentPlayerData[member.m_SteamID].m_Team = tfTeam;

		break;
	}
	case ConsoleLineType::Ping:
	{
		auto& pingLine = static_cast<const PingLine&>(parsed);
		if (auto found = FindSteamIDForName(pingLine.GetPlayerName()))
		{
			auto& playerData = m_CurrentPlayerData[*found];
			playerData.m_Status.m_Ping = pingLine.GetPing();
			playerData.m_LastPingUpdateTime = pingLine.GetTimestamp();
		}

		break;
	}
	case ConsoleLineType::PlayerStatus:
	{
		auto& statusLine = static_cast<const ServerStatusPlayerLine&>(parsed);
		auto newStatus = statusLine.GetPlayerStatus();
		auto& playerData = m_CurrentPlayerData[newStatus.m_SteamID];

		// Don't introduce stutter to our connection time view
		if (auto delta = (playerData.m_Status.m_ConnectionTime - newStatus.m_ConnectionTime);
			delta < 2s && delta > -2s)
		{
			newStatus.m_ConnectionTime = playerData.m_Status.m_ConnectionTime;
		}

		playerData.m_Status = newStatus;
		playerData.m_LastStatusUpdateTime = playerData.m_LastPingUpdateTime = statusLine.GetTimestamp();
		m_LastStatusUpdateTime = std::max(m_LastStatusUpdateTime, playerData.m_LastStatusUpdateTime);
		ProcessDelayedBans(statusLine.GetTimestamp(), newStatus);

		if (newStatus.m_Name.find("MYG)T"sv) != newStatus.m_Name.npos)
		{
			if (MarkPlayer(newStatus.m_SteamID, PlayerMarkType::Cheater))
				Log("Marked "s << newStatus.m_SteamID << " as a cheater due to name (mygot advertisement)");
		}
		if (newStatus.m_Name.ends_with("\xE2\x80\x8F"sv))
		{
			if (MarkPlayer(newStatus.m_SteamID, PlayerMarkType::Cheater))
				Log("Marked "s << newStatus.m_SteamID << " as cheater due to name ending in common name-stealing characters");
		}

		break;
	}
	case ConsoleLineType::PlayerStatusShort:
	{
		auto& statusLine = static_cast<const ServerStatusShortPlayerLine&>(parsed);
		const auto& status = statusLine.GetPlayerStatus();
		if (auto steamID = FindSteamIDForName(status.m_Name))
			m_CurrentPlayerData[*steamID].m_ClientIndex = status.m_ClientIndex;

		break;
	}
	case ConsoleLineType::KillNotification:
	{
		auto& killLine = static_cast<const KillNotificationLine&>(parsed);

		if (const auto attackerSteamID = FindSteamIDForName(killLine.GetAttackerName()))
			m_CurrentPlayerData[*attackerSteamID].m_Scores.m_Kills++;
		if (const auto victimSteamID = FindSteamIDForName(killLine.GetVictimName()))
			m_CurrentPlayerData[*victimSteamID].m_Scores.m_Deaths++;

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

	case ConsoleLineType::NetDataTotal:
	{
		auto& netDataLine = static_cast<const NetDataTotalLine&>(parsed);
		auto ts = round_time_point(netDataLine.GetTimestamp(), 100ms);
		m_NetSamplesIn.m_Data[ts].AddSample(netDataLine.GetInKBps());
		m_NetSamplesOut.m_Data[ts].AddSample(netDataLine.GetOutKBps());
		break;
	}
	case ConsoleLineType::NetLatency:
	{
		auto& netLatencyLine = static_cast<const NetLatencyLine&>(parsed);
		auto ts = round_time_point(netLatencyLine.GetTimestamp(), 100ms);
		m_NetSamplesIn.m_Latency[ts].AddSample(netLatencyLine.GetInLatency());
		m_NetSamplesOut.m_Latency[ts].AddSample(netLatencyLine.GetOutLatency());
		break;
	}
	case ConsoleLineType::NetPacketsTotal:
	{
		auto& netPacketsLine = static_cast<const NetPacketsTotalLine&>(parsed);
		auto ts = round_time_point(netPacketsLine.GetTimestamp(), 100ms);
		m_NetSamplesIn.m_Packets[ts].AddSample(netPacketsLine.GetInPacketsPerSecond());
		m_NetSamplesOut.m_Packets[ts].AddSample(netPacketsLine.GetOutPacketsPerSecond());
		break;
	}
	case ConsoleLineType::NetLoss:
	{
		auto& netLossLine = static_cast<const NetLossLine&>(parsed);
		auto ts = round_time_point(netLossLine.GetTimestamp(), 100ms);
		m_NetSamplesIn.m_Loss[ts].AddSample(netLossLine.GetInLossPercent());
		m_NetSamplesOut.m_Loss[ts].AddSample(netLossLine.GetOutLossPercent());
		break;
	}
	}
}

void MainWindow::HandleFriendlyCheaters(uint8_t friendlyPlayerCount, const std::vector<SteamID>& friendlyCheaters)
{
	if (friendlyCheaters.empty())
		return; // Nothing to do

	if ((friendlyPlayerCount / 2) <= friendlyCheaters.size())
	{
		Log("Impossible to pass a successful votekick against "s << friendlyCheaters.size()
			<< " friendly cheaters, but we're trying anyway :/", { 1, 0.5f, 0 });
	}

	// Votekick the first one that is actually connected
	for (const auto& cheater : friendlyCheaters)
	{
		const auto& playerData = m_CurrentPlayerData[cheater];
		if (playerData.m_Status.m_State == PlayerStatusState::Active)
		{
			InitiateVotekick(friendlyCheaters[0], KickReason::Cheating);
			break;
		}
	}
}

void MainWindow::HandleEnemyCheaters(uint8_t enemyPlayerCount,
	const std::vector<SteamID>& enemyCheaters, const std::vector<PlayerExtraData*>& connectingEnemyCheaters)
{
	if (enemyCheaters.empty() && connectingEnemyCheaters.empty())
		return;

	if (const auto cheaterCount = (enemyCheaters.size() + connectingEnemyCheaters.size()); (enemyPlayerCount / 2) <= cheaterCount)
	{
		Log("Impossible to pass a successful votekick against "s << cheaterCount << " enemy cheaters. Skipping all warnings.");
		return;
	}

	if (!enemyCheaters.empty())
	{
		// There are enough people on the other team to votekick the cheater(s)
		std::string logMsg;
		logMsg << "Telling the other team about " << enemyCheaters.size() << " cheater(s) named ";

		std::string chatMsg;
		chatMsg << "Attention! There ";
		if (enemyCheaters.size() == 1)
			chatMsg << "is a cheater ";
		else
			chatMsg << "are " << enemyCheaters.size() << " cheaters ";

		chatMsg << "on the other team named ";
		for (size_t i = 0; i < enemyCheaters.size(); i++)
		{
			const auto& cheaterData = m_CurrentPlayerData[enemyCheaters[i]];
			if (cheaterData.m_Status.m_Name.empty())
				continue; // Theoretically this should never happen, but don't embarass ourselves

			if (i != 0)
			{
				chatMsg << ", ";
				logMsg << ", ";
			}

			chatMsg << std::quoted(cheaterData.m_Status.m_Name);
			logMsg << std::quoted(cheaterData.m_Status.m_Name) << " (" << enemyCheaters[i] << ')';
		}

		chatMsg << ". Please kick them!";

		if (const auto now = GetCurrentTimestampCompensated(); (now - m_LastCheaterWarningTime) > 10s)
		{
			GetCurrentTimestampCompensated();
			if (m_ActionManager.QueueAction(std::make_unique<ChatMessageAction>(chatMsg)))
			{
				Log(logMsg, { 1, 0, 0, 1 });
				m_LastCheaterWarningTime = now;
			}
		}
	}
	else if (!connectingEnemyCheaters.empty())
	{
		bool needsWarning = false;
		for (PlayerExtraData* cheaterData : connectingEnemyCheaters)
		{
			if (!cheaterData->m_PreWarnedOtherTeam)
			{
				needsWarning = true;
				break;
			}
		}

		if (needsWarning)
		{
			std::string chatMsg;
			chatMsg << "Heads up! There ";
			if (connectingEnemyCheaters.size() == 1)
				chatMsg << "is a known cheater ";
			else
				chatMsg << "are " << connectingEnemyCheaters.size() << " known cheaters ";

			chatMsg << "joining the other team! Name";
			if (connectingEnemyCheaters.size() > 1)
				chatMsg << 's';

			chatMsg << " unknown until they fully join.";

			Log("Telling other team about "s << connectingEnemyCheaters.size() << " cheaters currently connecting");
			GetCurrentTimestampCompensated();
			if (m_ActionManager.QueueAction(std::make_unique<ChatMessageAction>(chatMsg)))
			{
				for (PlayerExtraData* cheaterData : connectingEnemyCheaters)
					cheaterData->m_PreWarnedOtherTeam = true;
			}
		}
	}
}

void MainWindow::ProcessPlayerActions()
{
	const auto now = GetCurrentTimestampCompensated();
	if ((now - m_LastPlayerActionsUpdate) < 1s)
	{
		return;
	}
	else
	{
		m_LastPlayerActionsUpdate = now;
	}

	// Don't process actions if we're way out of date
	[[maybe_unused]] const auto dbgDeltaTime = to_seconds(clock_t::now() - now);
	if ((clock_t::now() - now) > 15s)
		return;

	const auto myTeam = TryGetMyTeam();
	if (!myTeam)
		return; // We don't know what team we're on, so we can't really take any actions.

	uint8_t totalEnemyPlayers = 0;
	uint8_t totalFriendlyPlayers = 0;
	std::vector<SteamID> enemyCheaters;
	std::vector<SteamID> friendlyCheaters;
	std::vector<PlayerExtraData*> connectingEnemyCheaters;

	const auto ProcessLobbyMember = [&](const LobbyMember& player)
	{
		const auto teamShareResult = GetTeamShareResult(*myTeam, player.m_SteamID);
		switch (teamShareResult)
		{
		case TeamShareResult::SameTeams:      totalFriendlyPlayers++; break;
		case TeamShareResult::OppositeTeams:  totalEnemyPlayers++; break;
		}

		auto& playerExtraData = m_CurrentPlayerData[player.m_SteamID];

		if (IsPlayerMarked(player.m_SteamID, PlayerMarkType::Cheater))
		{
			switch (teamShareResult)
			{
			case TeamShareResult::SameTeams:
				friendlyCheaters.push_back(player.m_SteamID);
				break;
			case TeamShareResult::OppositeTeams:
				if (playerExtraData.m_Status.m_Name.empty())
					connectingEnemyCheaters.push_back(&playerExtraData);
				else
					enemyCheaters.push_back(player.m_SteamID);

				break;
			}
		}
	};

	for (const auto& player : m_PendingLobbyMembers)
		ProcessLobbyMember(player);
	for (const auto& player : m_CurrentLobbyMembers)
		ProcessLobbyMember(player);

	HandleEnemyCheaters(totalEnemyPlayers, enemyCheaters, connectingEnemyCheaters);
	HandleFriendlyCheaters(totalFriendlyPlayers, friendlyCheaters);
}

void MainWindow::UpdatePrintingLines()
{
	m_PrintingLineCount = 0;
	for (auto it = m_ConsoleLines.rbegin(); it != m_ConsoleLines.rend(); ++it)
	{
		if (it->get()->ShouldPrint())
		{
			m_PrintingLines[m_PrintingLineCount++] = it->get();
			if (m_PrintingLineCount >= 512)
				break;
		}
	}
}

size_t MainWindow::GeneratePlayerPrintData(PlayerPrintData* begin, PlayerPrintData* end) const
{
	assert(begin <= end);
	const size_t totalLobbyMembersCount = m_CurrentLobbyMembers.size() + m_PendingLobbyMembers.size();
	assert(static_cast<size_t>(end - begin) >= totalLobbyMembersCount);

	std::fill(begin, end, PlayerPrintData{});

	{
		PlayerPrintData* current = begin;
		for (const auto& lobbyMember : m_CurrentLobbyMembers)
		{
			current->m_SteamID = lobbyMember.m_SteamID;
			current++;
		}
		for (const auto& lobbyMember : m_PendingLobbyMembers)
		{
			current->m_SteamID = lobbyMember.m_SteamID;
			current++;
		}

		if (current == begin)
		{
			// We seem to have either an empty lobby or we're playing on a community server.
			// Just find the most recent status updates.
			for (const auto& [steamID, playerData] : m_CurrentPlayerData)
			{
				if (playerData.m_LastStatusUpdateTime >= (m_LastStatusUpdateTime - 15s))
				{
					current->m_SteamID = steamID;
					current++;

					if (current >= end)
						break; // This might happen, but we're not in a lobby so everything has to be approximate
				}
			}
		}

		end = current;
	}

	const auto now = GetCurrentTimestampCompensated();
	for (auto it = begin; it != end; ++it)
	{
		if (auto found = m_CurrentPlayerData.find(it->m_SteamID); found != m_CurrentPlayerData.end())
		{
			it->m_UserID = found->second.m_Status.m_UserID;
			it->m_Name = found->second.m_Status.m_Name;
			it->m_Ping = found->second.m_Status.m_Ping;
			it->m_Scores = found->second.m_Scores;
			it->m_Team = found->second.m_Team;
			it->m_ConnectedTime = now - found->second.m_Status.m_ConnectionTime;
			auto test = to_seconds(it->m_ConnectedTime);
			assert(it->m_ConnectedTime >= 0s);
			it->m_State = found->second.m_Status.m_State;
		}
	}

	std::sort(begin, end, [](const PlayerPrintData& lhs, const PlayerPrintData& rhs)
		{
			// Intentionally reversed, we want descending kill order
			auto killsResult = rhs.m_Scores.m_Kills <=> lhs.m_Scores.m_Kills;
			if (std::is_lt(killsResult))
				return true;
			else if (std::is_gt(killsResult))
				return false;

			return lhs.m_Scores.m_Deaths < rhs.m_Scores.m_Deaths;
		});

	return static_cast<size_t>(end - begin);
}

std::optional<SteamID> MainWindow::FindSteamIDForName(const std::string_view& playerName) const
{
	for (const auto& data : m_CurrentPlayerData)
	{
		if (data.second.m_Status.m_Name == playerName)
			return data.first;
	}

	return std::nullopt;
}

std::optional<LobbyMemberTeam> MainWindow::FindLobbyMemberTeam(const SteamID& id) const
{
	for (const auto& member : m_CurrentLobbyMembers)
	{
		if (member.m_SteamID == id)
			return member.m_Team;
	}

	for (const auto& member : m_PendingLobbyMembers)
	{
		if (member.m_SteamID == id)
			return member.m_Team;
	}

	return std::nullopt;
}

std::optional<uint16_t> MainWindow::FindUserID(const SteamID& id) const
{
	for (const auto& player : m_CurrentPlayerData)
	{
		if (player.second.m_Status.m_SteamID == id)
			return player.second.m_Status.m_UserID;
	}

	return std::nullopt;
}

auto MainWindow::GetTeamShareResult(const SteamID& id) const -> TeamShareResult
{
	return GetTeamShareResult(TryGetMyTeam(), id);
}

auto MainWindow::GetTeamShareResult(const std::optional<LobbyMemberTeam>& team0,
	const SteamID& id1) const -> TeamShareResult
{
	return GetTeamShareResult(team0, FindLobbyMemberTeam(id1));
}

auto MainWindow::GetTeamShareResult(const std::optional<LobbyMemberTeam>& team0,
	const std::optional<LobbyMemberTeam>& team1) -> TeamShareResult
{
	if (!team0)
		return TeamShareResult::Neither;
	if (!team1)
		return TeamShareResult::Neither;

	if (*team0 == *team1)
		return TeamShareResult::SameTeams;
	else if (*team0 == OppositeTeam(*team1))
		return TeamShareResult::OppositeTeams;
	else
		throw std::runtime_error("Unexpected team value(s)");
}

auto MainWindow::GetTeamShareResult(const SteamID& id0, const SteamID& id1) const -> TeamShareResult
{
	return GetTeamShareResult(FindLobbyMemberTeam(id0), FindLobbyMemberTeam(id1));
}

void MainWindow::ProcessDelayedBans(time_point_t timestamp, const PlayerStatus& updatedStatus)
{
	for (size_t i = 0; i < m_DelayedBans.size(); i++)
	{
		const auto& ban = m_DelayedBans[i];
		const auto timeSince = timestamp - ban.m_Timestamp;

		const auto RemoveBan = [&]()
		{
			m_DelayedBans.erase(m_DelayedBans.begin() + i);
			i--;
		};

		if (timeSince > 10s)
		{
			RemoveBan();
			Log("Expiring delayed ban for user with name "s << std::quoted(ban.m_PlayerName)
				<< " (" << to_seconds(timeSince) << " second delay)");
			continue;
		}

		if (ban.m_PlayerName == updatedStatus.m_Name)
		{
			if (MarkPlayer(updatedStatus.m_SteamID, PlayerMarkType::Cheater))
			{
				Log("Applying delayed ban ("s << to_seconds(timeSince) << " second delay) to player "
					<< updatedStatus.m_SteamID);
			}

			RemoveBan();
			break;
		}
	}
}

PlayerList& MainWindow::GetPlayerList(PlayerMarkType type)
{
	return const_cast<PlayerList&>(std::as_const(*this).GetPlayerList(type));
}

const PlayerList& MainWindow::GetPlayerList(PlayerMarkType type) const
{
	return m_PlayerLists[std::underlying_type_t<PlayerMarkType>(type)];
}

void MainWindow::UpdateServerPing(time_point_t timestamp)
{
	if ((timestamp - m_LastServerPingSample) <= 7s)
		return;

	float totalPing = 0;
	uint16_t samples = 0;

	for (const auto& player : m_CurrentPlayerData)
	{
		const PlayerExtraData& data = player.second;
		if (data.m_LastStatusUpdateTime < (timestamp - 20s))
			continue;

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

bool MainWindow::MarkPlayer(const SteamID& id, PlayerMarkType markType, bool marked)
{
	switch (markType)
	{
	case PlayerMarkType::Cheater:
	{
		const bool retVal = GetPlayerList(PlayerMarkType::Cheater).IncludePlayer(id, marked);
		if (marked)
			GetPlayerList(PlayerMarkType::Suspicious).IncludePlayer(id, false);

		return retVal;
	}

	case PlayerMarkType::Suspicious:
		if (!marked || !GetPlayerList(PlayerMarkType::Cheater).IsPlayerIncluded(id))
			return GetPlayerList(PlayerMarkType::Suspicious).IncludePlayer(id);
		else
			return false;

	case PlayerMarkType::Exploiter:
		return GetPlayerList(PlayerMarkType::Exploiter).IncludePlayer(id, marked);
		break;

	case PlayerMarkType::Racist:
		return GetPlayerList(PlayerMarkType::Racist).IncludePlayer(id, marked);
		break;

	default:
		throw std::runtime_error("Unexpected/invalid PlayerMarkType");
	}
}

bool MainWindow::IsPlayerMarked(const SteamID& id, PlayerMarkType markType) const
{
	return GetPlayerList(markType).IsPlayerIncluded(id);
}

std::optional<LobbyMemberTeam> MainWindow::TryGetMyTeam() const
{
	static constexpr SteamID s_PazerSID(76561198003911389ull);
	return FindLobbyMemberTeam(s_PazerSID);
}

void MainWindow::InitiateVotekick(const SteamID& id, KickReason reason)
{
	const auto userID = FindUserID(id);
	if (!userID)
	{
		Log("Wanted to kick "s << id << ", but could not find userid");
		return;
	}

	if (m_ActionManager.QueueAction(std::make_unique<KickAction>(userID.value(), reason)))
		Log("InitiateVotekick on "s << id << ": " << reason);
}

void MainWindow::CustomDeleters::operator()(FILE* f) const
{
	fclose(f);
}

float MainWindow::PlayerExtraData::GetAveragePing() const
{
	unsigned totalPing = m_Status.m_Ping;
	unsigned samples = 1;

	for (const auto& entry : m_PingHistory)
	{
		totalPing += entry.m_Ping;
		samples++;
	}

	return totalPing / float(samples);
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

void MainWindow::CompensatedTS::SetRecorded(time_point_t recorded)
{
	assert(recorded.time_since_epoch() > 0s);
	m_Recorded = recorded;

	const auto now = clock_t::now();
	if (m_Snapshot && (now - *m_Snapshot) >= 1s)
		m_Snapshot.reset();

	if ((now - recorded) > 1250ms)
		m_Parsed = now;
	else
		m_Parsed = recorded;
}

void MainWindow::CompensatedTS::Snapshot()
{
	const auto now = clock_t::now();
	const auto extra = now - m_Parsed;
	[[maybe_unused]] const auto extraSeconds = to_seconds(extra);
	const time_point_t adjustedTS = m_Recorded.value() + extra;

	const auto error = adjustedTS - now;
	const auto errorSeconds = to_seconds(extra);
	assert(error <= 1s);

	auto newSnapshot = std::min(adjustedTS, now);

#if 0
	if (m_SnapshotUsed && m_PreviousSnapshot && newSnapshot < *m_PreviousSnapshot)
	{
		// going backwards in time!!!
		const auto delta = to_seconds(newSnapshot - *m_PreviousSnapshot);
		assert(delta >= -0.01);
		newSnapshot = *m_PreviousSnapshot;
	}
#endif

	m_Snapshot = newSnapshot;
	m_PreviousSnapshot = newSnapshot;
	m_SnapshotUsed = false;
}

time_point_t MainWindow::CompensatedTS::GetSnapshot() const
{
	m_SnapshotUsed = true;
	return m_Snapshot.value();
}
