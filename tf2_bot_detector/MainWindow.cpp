#include "MainWindow.h"
#include "ConsoleLines.h"
#include "RegexCharConv.h"

#include <imgui_desktop/ScopeGuards.h>
#include <imgui_desktop/ImGuiHelpers.h>
#include <imgui.h>
#include <mh/math/interpolation.hpp>

#include <cassert>
#include <chrono>
#include <regex>

using namespace tf2_bot_detector;
using namespace std::chrono_literals;

static const std::filesystem::path s_CheaterListFile("playerlist_cheaters.txt");
static const std::filesystem::path s_SuspiciousListFile("playerlist_suspicious.txt");
static const std::filesystem::path s_ExploiterListFile("playerlist_exploiters.txt");

MainWindow::MainWindow() :
	ImGuiDesktop::Window(800, 600, "TF2 Bot Detector")
{
	m_OpenTime = std::chrono::steady_clock::now();
	m_CheaterList.LoadFile(s_CheaterListFile);
	m_SuspiciousList.LoadFile(s_SuspiciousListFile);
	m_ExploiterList.LoadFile(s_ExploiterListFile);
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

void MainWindow::OnDraw()
{
#if 0
	ImGui::Text("Current application time: %1.2f", std::chrono::duration<float>(std::chrono::steady_clock::now() - m_OpenTime).count());
	ImGui::NewLine();
	if (ImGui::Button("Quit"))
		SetShouldClose(true);
#endif

	ImGui::Columns(2, "MainWindowSplit");
	//ImGui::InputTextMultiline("##fileContents", const_cast<char*>(m_FileText.data()), m_FileText.size(), { -1, -1 }, ImGuiInputTextFlags_ReadOnly);
	if (ImGui::BeginChild("##fileContents", { 0, 0 }, true, ImGuiWindowFlags_AlwaysVerticalScrollbar))
	{
		ImGui::PushTextWrapPos();

		for (auto it = &m_PrintingLines[m_PrintingLineCount - 1]; it != &m_PrintingLines[-1]; --it)
		{
			assert(*it);
			(*it)->Print();
		}

		ImGui::PopTextWrapPos();
		ImGui::SetScrollHereY(1.0f);
		ImGui::EndChild();
	}

	ImGui::NextColumn();

	if (ImGui::BeginChild("Scoreboard", { 0, 0 }, true))
	{
		static ImVec2 s_LastFrameSize;
		const bool scoreboardResized = [&]()
		{
			const auto thisFrameSize = ImGui::GetWindowSize();
			const bool changed = s_LastFrameSize != thisFrameSize;
			s_LastFrameSize = thisFrameSize;
			return changed;
		}();

		//if (ImGui::BeginChildFrame(ImGui::GetID("ScoreboardFrame"), { 0, 0 }))
		{
			const auto frameWidth = ImGui::GetItemRectSize().x;
			PlayerPrintData printData[33]{};
			const size_t playerPrintDataCount = GeneratePlayerPrintData(std::begin(printData), std::end(printData));

			ImGui::Columns(5, "PlayersColumns");

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

				// SteamID header and column setup
				{
					ImGui::TextUnformatted("Steam ID");
					if (scoreboardResized)
					{
						nameColumnWidth -= 125;// +ImGui::GetStyle().ItemSpacing.x * 2;
						ImGui::SetColumnWidth(1, nameColumnWidth - ImGui::GetStyle().ItemSpacing.x * 2);
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

				char buf[32];
				if (auto result = mh::to_chars(buf, player.m_UserID))
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

					if (m_CheaterList.IsPlayerIncluded(player.m_SteamID))
						bgColor = mh::lerp(TimeSine(), bgColor, ImVec4(1, 0, 1, 1));
					else if (m_SuspiciousList.IsPlayerIncluded(player.m_SteamID))
						bgColor = mh::lerp(TimeSine(), bgColor, ImVec4(1, 1, 0, 1));
					else if (m_ExploiterList.IsPlayerIncluded(player.m_SteamID))
						bgColor = mh::lerp(TimeSine(), bgColor, ImVec4(0, 1, 1, 1));

					ImGuiDesktop::ScopeGuards::StyleColor styleColorScope(ImGuiCol_Header, bgColor);

					bgColor.w = 0.8f;
					ImGuiDesktop::ScopeGuards::StyleColor styleColorScopeHovered(ImGuiCol_HeaderHovered, bgColor);

					bgColor.w = 1.0f;
					ImGuiDesktop::ScopeGuards::StyleColor styleColorScopeActive(ImGuiCol_HeaderActive, bgColor);
					ImGui::Selectable(buf, true, ImGuiSelectableFlags_SpanAllColumns); ImGui::NextColumn();
				}

				if (ImGui::BeginPopupContextItem("PlayerContextMenu"))
				{
					if (ImGui::Selectable("Copy SteamID"))
						ImGui::SetClipboardText(player.m_SteamID.str().c_str());

					{
						const bool existingIsCheater = m_CheaterList.IsPlayerIncluded(player.m_SteamID);
						if (ImGui::MenuItem("Mark as cheater", nullptr, existingIsCheater))
						{
							m_CheaterList.IncludePlayer(player.m_SteamID, !existingIsCheater);
							m_CheaterList.SaveFile(s_CheaterListFile);
						}
					}

					{
						const bool existingIsSuspicious = m_SuspiciousList.IsPlayerIncluded(player.m_SteamID);
						if (ImGui::MenuItem("Mark as suspicious", nullptr, existingIsSuspicious))
						{
							m_SuspiciousList.IncludePlayer(player.m_SteamID, !existingIsSuspicious);
							m_SuspiciousList.SaveFile(s_SuspiciousListFile);
						}
					}

					{
						const bool existingIsExploiter = m_ExploiterList.IsPlayerIncluded(player.m_SteamID);
						if (ImGui::MenuItem("Mark as exploiter", nullptr, existingIsExploiter))
						{
							m_ExploiterList.IncludePlayer(player.m_SteamID, !existingIsExploiter);
							m_ExploiterList.SaveFile(s_ExploiterListFile);
						}
					}

					ImGui::EndPopup();
				}

				// player names column
				{
					if (player.m_Name.empty())
					{
						ImGuiDesktop::ScopeGuards::StyleColor textColor(ImGuiCol_Text, ImVec4(1, 1, 0, 0.5f));
						ImGui::TextUnformatted("<Unknown>");
					}
					else
					{
						ImGui::TextUnformatted(player.m_Name.c_str());
					}

					ImGui::NextColumn();
				}

				ImGui::Text("%u", player.m_Scores.m_Kills); ImGui::NextColumn();
				ImGui::Text("%u", player.m_Scores.m_Deaths); ImGui::NextColumn();

				ImGui::TextUnformatted(player.m_SteamID.str().c_str()); ImGui::NextColumn();
			}

			ImGui::EndChildFrame();
		}

		ImGui::EndChild();
	}
}

static std::regex s_TimestampRegex(R"regex(\n?(\d\d)\/(\d\d)\/(\d\d\d\d) - (\d\d):(\d\d):(\d\d): )regex", std::regex::optimize);
void MainWindow::OnUpdate()
{
	if (!m_File)
	{
		{
			FILE* temp = _fsopen("C:\\Program Files (x86)\\Steam\\steamapps\\common\\Team Fortress 2\\tf\\console.log", "r", _SH_DENYNO);
			m_File.reset(temp);
		}

		if (m_File)
			m_ConsoleLines.clear();
	}

	if (m_File)
	{
		char buf[4096];
		size_t readCount;
		using clock = std::chrono::steady_clock;
		const auto startTime = clock::now();
		bool consoleLinesUpdated = false;
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
					if (m_CurrentTimestamp)
					{
						const auto timestamp = std::mktime(&m_CurrentTimestamp.value());
						assert(timestamp >= 0);

						const auto prefix = match.prefix();
						//const auto suffix = match.suffix();
						auto parsed = IConsoleLine::ParseConsoleLine(
							std::string_view(&*prefix.first, prefix.length()), timestamp);

						if (parsed)
						{
							OnConsoleLineParsed(parsed.get());
							m_ConsoleLines.push_back(std::move(parsed));
							consoleLinesUpdated = true;
						}
					}

					std::tm time{};
					from_chars_throw(match[1], time.tm_mon);
					time.tm_mon -= 1;

					from_chars_throw(match[2], time.tm_mday);
					from_chars_throw(match[3], time.tm_year);
					time.tm_year -= 1900;

					from_chars_throw(match[4], time.tm_hour);
					from_chars_throw(match[5], time.tm_min);
					from_chars_throw(match[6], time.tm_sec);

					m_CurrentTimestamp = time;
					regexBegin = match[0].second;
				}

				m_FileLineBuf.erase(m_FileLineBuf.begin(), regexBegin);
			}

			if (auto elapsed = clock::now() - startTime; elapsed > 10ms)
				break;

		} while (readCount > 0);

		if (readCount > 0)
			QueueUpdate();

		if (consoleLinesUpdated)
			UpdatePrintingLines();
	}
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

void MainWindow::OnConsoleLineParsed(IConsoleLine* parsed)
{
	switch (parsed->GetType())
	{
	case ConsoleLineType::LobbyHeader:
	{
		auto headerLine = static_cast<const LobbyHeaderLine*>(parsed);
		m_CurrentLobbyMembers.resize(headerLine->GetMemberCount());
		m_PendingLobbyMembers.resize(headerLine->GetPendingCount());
		break;
	}
	case ConsoleLineType::LobbyDestroyed:
	case ConsoleLineType::ClientReachedServerSpawn:
	{
		// Reset current lobby members/player statuses
		m_CurrentLobbyMembers.clear();
		m_PendingLobbyMembers.clear();
		m_CurrentPlayerData.clear();
		break;
	}

	case ConsoleLineType::LobbyMember:
	{
		auto memberLine = static_cast<const LobbyMemberLine*>(parsed);
		const auto& member = memberLine->GetLobbyMember();
		auto& vec = member.m_Pending ? m_PendingLobbyMembers : m_CurrentLobbyMembers;
		if (member.m_Index < vec.size())
			vec[member.m_Index] = member;

		const TFTeam tfTeam = member.m_Team == LobbyMemberTeam::Defenders ? TFTeam::Red : TFTeam::Blue;
		m_CurrentPlayerData[member.m_SteamID].m_Team = tfTeam;

		break;
	}
	case ConsoleLineType::PlayerStatus:
	{
		auto statusLine = static_cast<const ServerStatusPlayerLine*>(parsed);
		const auto& status = statusLine->GetPlayerStatus();
		m_CurrentPlayerData[status.m_SteamID].m_Status = status;
		break;
	}
	case ConsoleLineType::KillNotification:
	{
		auto killLine = static_cast<const KillNotificationLine*>(parsed);

		if (const auto attackerSteamID = FindSteamIDForName(killLine->GetAttackerName()))
			m_CurrentPlayerData[*attackerSteamID].m_Scores.m_Kills++;
		if (const auto victimSteamID = FindSteamIDForName(killLine->GetVictimName()))
			m_CurrentPlayerData[*victimSteamID].m_Scores.m_Deaths++;

		break;
	}
	}
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
		end = current;
	}

	for (auto it = begin; it != end; ++it)
	{
		if (auto found = m_CurrentPlayerData.find(it->m_SteamID); found != m_CurrentPlayerData.end())
		{
			it->m_UserID = found->second.m_Status.m_UserID;
			it->m_Name = found->second.m_Status.m_Name;
			it->m_Ping = found->second.m_Status.m_Ping;
			it->m_Scores = found->second.m_Scores;
			it->m_Team = found->second.m_Team;
		}
	}

	std::sort(begin, end, [](const PlayerPrintData& lhs, const PlayerPrintData& rhs)
		{
			// Intentionally reversed, we want descending kill order
			return (rhs.m_Scores.m_Kills < lhs.m_Scores.m_Kills);
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

void MainWindow::CustomDeleters::operator()(FILE* f) const
{
	fclose(f);
}
