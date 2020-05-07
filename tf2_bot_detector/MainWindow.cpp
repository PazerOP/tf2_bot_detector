#include "MainWindow.h"
#include "ConsoleLines.h"
#include "RegexCharConv.h"
#include "ImGui_TF2BotDetector.h"
#include "Log.h"

#include <imgui_desktop/ScopeGuards.h>
#include <imgui_desktop/ImGuiHelpers.h>
#include <imgui.h>
#include <mh/math/interpolation.hpp>

#include <cassert>
#include <chrono>
#include <string>
#include <regex>

using namespace tf2_bot_detector;
using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;

static const std::filesystem::path s_CheaterListFile("playerlist_cheaters.txt");
static const std::filesystem::path s_SuspiciousListFile("playerlist_suspicious.txt");
static const std::filesystem::path s_ExploiterListFile("playerlist_exploiters.txt");

static constexpr SteamID s_PazerSID(76561198003911389ull);

MainWindow::MainWindow() :
	ImGuiDesktop::Window(800, 600, "TF2 Bot Detector"),
	m_CheaterList("playerlist_cheaters.txt"),
	m_SuspiciousList("playerlist_suspicious.txt"),
	m_ExploiterList("playerlist_exploiters.txt")
{
	m_OpenTime = clock_t::now();
	AddConsoleLineListener(this);
	AddConsoleLineListener(&m_ActionManager);
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

		const auto pazerTeam = FindLobbyMemberTeam(s_PazerSID);
		if (ImGui::BeginMenu("Votekick", pazerTeam && FindLobbyMemberTeam(steamID) == pazerTeam && FindUserID(steamID)))
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

		if (const bool existingIsCheater = m_CheaterList.IsPlayerIncluded(steamID);
			ImGui::MenuItem("Mark as cheater", nullptr, existingIsCheater))
		{
			m_CheaterList.IncludePlayer(steamID, !existingIsCheater);
			Log("Manually marked "s << steamID << " as" << (existingIsCheater ? " NOT" : "") << " a cheater.");
		}

		if (const bool existingIsSuspicious = m_SuspiciousList.IsPlayerIncluded(steamID);
			ImGui::MenuItem("Mark as suspicious", nullptr, existingIsSuspicious))
		{
			m_SuspiciousList.IncludePlayer(steamID, !existingIsSuspicious);
			Log("Manually marked "s << steamID << " as" << (existingIsSuspicious ? " NOT" : "") << " suspicious.");
		}

		if (const bool existingIsExploiter = m_ExploiterList.IsPlayerIncluded(steamID);
			ImGui::MenuItem("Mark as exploiter", nullptr, existingIsExploiter))
		{
			m_ExploiterList.IncludePlayer(steamID, !existingIsExploiter);
			Log("Manually marked "s << steamID << " as" << (existingIsExploiter ? " NOT" : "") << " an exploiter.");
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
			ImGui::Columns(6, "PlayersColumns");

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
				if (player.m_Name.empty())
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
						ImGui::TextRightAligned("?");
					else
						ImGui::TextRightAlignedF("%u:%02u", player.m_ConnectedTime / 60, player.m_ConnectedTime % 60);

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
					ImGui::TextColoredUnformatted({ msg.m_Color.r, msg.m_Color.g, msg.m_Color.b, msg.m_Color.a }, msg.m_Text);
				});

			ImGui::PopTextWrapPos();
		});
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

	ImGui::Text("Parsed line count: %zu", m_ParsedLineCount);

	ImGui::Columns(2, "MainWindowSplit");

	OnDrawChat(); ImGui::NextColumn();

	OnDrawScoreboard();
	OnDrawAppLog();
	ImGui::NextColumn();
}

static std::regex s_TimestampRegex(R"regex(\n(\d\d)\/(\d\d)\/(\d\d\d\d) - (\d\d):(\d\d):(\d\d): )regex", std::regex::optimize);
void MainWindow::OnUpdate()
{
	if (m_Paused)
		return;

	m_ActionManager.Update();

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
						assert(*m_CurrentTimestamp >= time_point_t{});

						const auto prefix = match.prefix();
						//const auto suffix = match.suffix();
						const std::string_view lineStr(&*prefix.first, prefix.length());
						auto parsed = IConsoleLine::ParseConsoleLine(lineStr, *m_CurrentTimestamp);

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
								listener->OnConsoleLineUnparsed(*m_CurrentTimestamp, lineStr);
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

					m_CurrentTimestamp = clock_t::from_time_t(std::mktime(&time));
					regexBegin = match[0].second;
				}

				m_ParsedLineCount += std::count(m_FileLineBuf.cbegin(), regexBegin, '\n');
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

void MainWindow::OnConsoleLineParsed(IConsoleLine& parsed)
{
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
				if (m_CheaterList.IncludePlayer(*steamID))
					Log("Marked "s << *steamID << " as cheater (" << count << " newlines in chat message");
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

		// TODO: autokick for racism

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
	case ConsoleLineType::PlayerStatus:
	{
		auto& statusLine = static_cast<const ServerStatusPlayerLine&>(parsed);
		const auto& status = statusLine.GetPlayerStatus();
		auto& playerData = m_CurrentPlayerData[status.m_SteamID];
		playerData.m_Status = status;
		ProcessDelayedBans(statusLine.GetTimestamp(), status);

		if (status.m_Name.ends_with("\xE2\x80\x8F"sv))
		{
			if (MarkPlayer(status.m_SteamID, PlayerMarkType::Suspicious))
				Log("Marked "s << status.m_SteamID << " as suspicious due to name ending in common name-stealing characters");
		}

		if (IsPlayerMarked(status.m_SteamID, PlayerMarkType::Cheater))
		{
			if (const auto curtime = clock_t::now(); (curtime - statusLine.GetTimestamp()) > 10s)
			{
				Log("Cheater detected, but is "s << to_seconds<float>(curtime - statusLine.GetTimestamp()) << " old, ignoring.");
			}
			else
			{
				const auto pazerTeam = FindLobbyMemberTeam(s_PazerSID);
				if (!pazerTeam.has_value())
				{
					Log("Cheater found ("s << std::quoted(status.m_Name) << "), but can't find pazer's steam ID in the lobby");
				}
				else if (pazerTeam == FindLobbyMemberTeam(status.m_SteamID))
				{
					Log("Cheater on YOUR team: "s << std::quoted(status.m_Name), { 1, 1, 0, 1 });
					InitiateVotekick(status.m_SteamID, KickReason::Cheating);
				}
				else
				{
					Log("Telling other team about cheater named "s << std::quoted(status.m_Name) << "... (" << status.m_SteamID << ')', { 1, 0, 0, 1 });
					m_ActionManager.QueueAction(std::make_unique<ChatMessageAction>(
						"Attention! There is a cheater on the other team with the name "s <<
						std::quoted(status.m_Name, '\'') << ". Please kick them!"));
				}
			}
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
			it->m_ConnectedTime = found->second.m_Status.m_ConnectedTime;
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
			if (m_CheaterList.IncludePlayer(updatedStatus.m_SteamID))
			{
				Log("Applying delayed ban ("s << to_seconds(timeSince) << " second delay) to player "
					<< updatedStatus.m_SteamID);
			}

			RemoveBan();
			break;
		}
	}
}

bool MainWindow::MarkPlayer(const SteamID& id, PlayerMarkType markType)
{
	switch (markType)
	{
	case PlayerMarkType::Cheater:
	{
		const bool retVal = m_CheaterList.IncludePlayer(id);
		m_SuspiciousList.IncludePlayer(id, false);
		return retVal;
	}

	case PlayerMarkType::Suspicious:
		if (!m_CheaterList.IsPlayerIncluded(id))
			return m_SuspiciousList.IncludePlayer(id);

		return false;

	case PlayerMarkType::Exploiter:
		return m_ExploiterList.IncludePlayer(id);
		break;

	default:
		throw std::runtime_error("Unexpected/invalid PlayerMarkType");
	}
}

bool MainWindow::IsPlayerMarked(const SteamID& id, PlayerMarkType markType)
{
	switch (markType)
	{
	case PlayerMarkType::Cheater:    return m_CheaterList.IsPlayerIncluded(id);
	case PlayerMarkType::Exploiter:  return m_ExploiterList.IsPlayerIncluded(id);
	case PlayerMarkType::Suspicious: return m_SuspiciousList.IsPlayerIncluded(id);

	default:
		throw std::runtime_error("Invalid PlayerMarkType");
	}
}

void MainWindow::InitiateVotekick(const SteamID& id, KickReason reason)
{
	Log("InitiateVotekick on "s << id << ": " << reason);

	const auto userID = FindUserID(id);
	if (!userID)
	{
		Log("Wanted to kick "s << id << ", but could not find userid");
		return;
	}

	auto action = std::make_unique<KickAction>(userID.value(), reason);
	m_ActionManager.QueueAction(std::move(action));
}

void MainWindow::CustomDeleters::operator()(FILE* f) const
{
	fclose(f);
}
