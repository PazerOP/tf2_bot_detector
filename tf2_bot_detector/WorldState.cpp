#include "WorldState.h"
#include "Actions.h"
#include "ConsoleLines.h"
#include "RegexHelpers.h"

#include <mh/text/string_insertion.hpp>

#include <regex>

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace tf2_bot_detector;

WorldState::WorldState(const std::filesystem::path& conLogFile) :
	m_FileName(conLogFile)
{
	m_ConsoleLineListeners.insert(this);
}

void WorldState::Update()
{
	if (!m_File)
	{
		{
			FILE* temp = _fsopen(m_FileName.string().c_str(), "r", _SH_DENYNO);
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
			m_EventBroadcaster.OnTimestampUpdate(*this);
		}
	};

	bool linesProcessed = false;
	bool consoleLinesUpdated = false;
	if (m_File)
	{
		char buf[4096];
		size_t readCount;
		using clock = std::chrono::steady_clock;
		const auto startTime = clock::now();
		static const std::regex s_TimestampRegex(R"regex(\n(\d\d)\/(\d\d)\/(\d\d\d\d) - (\d\d):(\d\d):(\d\d):[ \n])regex", std::regex::optimize);
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
						linesProcessed = true;

						const auto prefix = match.prefix();
						//const auto suffix = match.suffix();
						const std::string_view lineStr(&*prefix.first, prefix.length());
						auto parsed = IConsoleLine::ParseConsoleLine(lineStr, m_CurrentTimestamp.GetSnapshot());

						if (parsed)
						{
							for (auto listener : m_ConsoleLineListeners)
								listener->OnConsoleLineParsed(*this, *parsed);

							m_ConsoleLines.push_back(std::move(parsed));
							consoleLinesUpdated = true;
						}
						else
						{
							for (auto listener : m_ConsoleLineListeners)
								listener->OnConsoleLineUnparsed(*this, lineStr);
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

			if (auto elapsed = clock::now() - startTime; elapsed >= 50ms)
				break;

		} while (readCount > 0);

#if 0
		if (readCount > 0)
			QueueUpdate();

		if (consoleLinesUpdated)
			UpdatePrintingLines();
#endif
	}

	if (linesProcessed)
		m_EventBroadcaster.OnUpdate(*this, consoleLinesUpdated);

	TrySnapshot();

#if 0
	SetLogTimestamp(m_CurrentTimestamp.GetSnapshot());

	ProcessPlayerActions();
	m_PeriodicActionManager.Update(m_CurrentTimestamp.GetSnapshot());
	m_ActionManager.Update(m_CurrentTimestamp.GetSnapshot());

	if (consoleLinesUpdated)
		UpdateServerPing(m_CurrentTimestamp.GetSnapshot());
#endif
}

void WorldState::CustomDeleters::operator()(FILE* f) const
{
	fclose(f);
}

void WorldState::EventBroadcaster::OnUpdate(WorldState& world, bool consoleLinesUpdated)
{
	for (auto listener : m_EventListeners)
		listener->OnUpdate(world, consoleLinesUpdated);
}

void WorldState::EventBroadcaster::OnTimestampUpdate(WorldState& world)
{
	for (auto listener : m_EventListeners)
		listener->OnTimestampUpdate(world);
}

void WorldState::EventBroadcaster::OnPlayerStatusUpdate(WorldState& world, const PlayerRef& player)
{
	for (auto listener : m_EventListeners)
		listener->OnPlayerStatusUpdate(world, player);
}

void WorldState::EventBroadcaster::OnChatMsg(WorldState& world,
	const PlayerRef& player, const std::string_view& msg)
{
	for (auto listener : m_EventListeners)
		listener->OnChatMsg(world, player, msg);
}

std::optional<SteamID> WorldState::FindSteamIDForName(const std::string_view& playerName) const
{
	for (const auto& data : m_CurrentPlayerData)
	{
		if (data.second.m_Status.m_Name == playerName)
			return data.first;
	}

	return std::nullopt;
}

std::optional<LobbyMemberTeam> WorldState::FindLobbyMemberTeam(const SteamID& id) const
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

std::optional<uint16_t> WorldState::FindUserID(const SteamID& id) const
{
	for (const auto& player : m_CurrentPlayerData)
	{
		if (player.second.m_Status.m_SteamID == id)
			return player.second.m_Status.m_UserID;
	}

	return std::nullopt;
}

auto WorldState::GetTeamShareResult(const std::optional<LobbyMemberTeam>& team0,
	const SteamID& id1) const -> TeamShareResult
{
	return GetTeamShareResult(team0, FindLobbyMemberTeam(id1));
}

auto WorldState::GetTeamShareResult(const std::optional<LobbyMemberTeam>& team0,
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

void WorldState::OnConsoleLineParsed(IConsoleLine& parsed)
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
			// We can't trust the existing client indices
			//for (auto& player : m_CurrentPlayerData)
			//	player.second.m_ClientIndex = 0;
		}
		break;
	}
	case ConsoleLineType::ClientReachedServerSpawn:
	{
		// Reset current lobby members/player statuses
		//ClearLobbyState();
		break;
	}

#if 0
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
#endif

	

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
		if (auto found = GetWorld().FindSteamIDForName(pingLine.GetPlayerName()))
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
			if (SetPlayerAttribute(newStatus.m_SteamID, PlayerAttributes::Cheater))
				Log("Marked "s << newStatus.m_SteamID << " as a cheater due to name (mygot advertisement)");
		}
		if (newStatus.m_Name.ends_with("\xE2\x80\x8F"sv))
		{
			if (SetPlayerAttribute(newStatus.m_SteamID, PlayerAttributes::Cheater))
				Log("Marked "s << newStatus.m_SteamID << " as cheater due to name ending in common name-stealing characters");
		}

		break;
	}
	case ConsoleLineType::PlayerStatusShort:
	{
		auto& statusLine = static_cast<const ServerStatusShortPlayerLine&>(parsed);
		const auto& status = statusLine.GetPlayerStatus();
		if (auto steamID = world.FindSteamIDForName(status.m_Name))
			m_CurrentPlayerData[*steamID].m_ClientIndex = status.m_ClientIndex;

		break;
	}
	case ConsoleLineType::KillNotification:
	{
		auto& killLine = static_cast<const KillNotificationLine&>(parsed);

		if (const auto attackerSteamID = world.FindSteamIDForName(killLine.GetAttackerName()))
			m_CurrentPlayerData[*attackerSteamID].m_Scores.m_Kills++;
		if (const auto victimSteamID = world.FindSteamIDForName(killLine.GetVictimName()))
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

auto WorldState::GetTeamShareResult(const SteamID& id0, const SteamID& id1) const -> TeamShareResult
{
	return GetTeamShareResult(FindLobbyMemberTeam(id0), FindLobbyMemberTeam(id1));
}
