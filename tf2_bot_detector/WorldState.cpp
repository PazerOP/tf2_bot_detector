#include "WorldState.h"
#include "Actions/Actions.h"
#include "Config/Settings.h"
#include "ConsoleLog/ConsoleLines.h"
#include "ConsoleLog/ConsoleLogParser.h"
#include "GameData/UserMessageType.h"
#include "Log.h"
#include "RegexHelpers.h"

#include <mh/future.hpp>

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace tf2_bot_detector;

WorldState::WorldState(const Settings& settings) :
	m_Settings(&settings)
{
	AddConsoleLineListener(this);
}

WorldState::~WorldState()
{
	RemoveConsoleLineListener(this);
}

void WorldState::Update()
{
	ApplyPlayerSummaries();
}

void WorldStateConLog::AddConsoleLineListener(IConsoleLineListener* listener)
{
	m_ConsoleLineListeners.insert(listener);
}

void WorldStateConLog::RemoveConsoleLineListener(IConsoleLineListener* listener)
{
	m_ConsoleLineListeners.erase(listener);
}

void WorldStateConLog::AddConsoleOutputChunk(const std::string_view& chunk)
{
	size_t last = 0;
	for (auto i = chunk.find('\n', 0); i != chunk.npos; i = chunk.find('\n', last))
	{
		auto line = chunk.substr(last, i - last);
		AddConsoleOutputLine(line);
		last = i + 1;
	}
}

void WorldStateConLog::AddConsoleOutputLine(const std::string_view& line)
{
	auto parsed = IConsoleLine::ParseConsoleLine(line, GetWorldState().GetCurrentTime());
	if (parsed)
	{
		for (auto listener : m_ConsoleLineListeners)
			listener->OnConsoleLineParsed(GetWorldState(), *parsed);
	}
	else
	{
		for (auto listener : m_ConsoleLineListeners)
			listener->OnConsoleLineUnparsed(GetWorldState(), line);
	}
}

WorldState& WorldStateConLog::GetWorldState()
{
	return *static_cast<WorldState*>(this);
}

void WorldState::UpdateTimestamp(const ConsoleLogParser& parser)
{
	m_CurrentTimestamp = parser.GetCurrentTimestamp();
}

void WorldState::AddWorldEventListener(IWorldEventListener* listener)
{
	m_EventBroadcaster.m_EventListeners.insert(listener);
}

void WorldState::RemoveWorldEventListener(IWorldEventListener* listener)
{
	m_EventBroadcaster.m_EventListeners.erase(listener);
}

void WorldState::EventBroadcaster::OnTimestampUpdate(WorldState& world)
{
	for (auto listener : m_EventListeners)
		listener->OnTimestampUpdate(world);
}

void WorldState::EventBroadcaster::OnPlayerStatusUpdate(WorldState& world, const IPlayer& player)
{
	for (auto listener : m_EventListeners)
		listener->OnPlayerStatusUpdate(world, player);
}

void WorldState::EventBroadcaster::OnChatMsg(WorldState& world,
	IPlayer& player, const std::string_view& msg)
{
	for (auto listener : m_EventListeners)
		listener->OnChatMsg(world, player, msg);
}

std::optional<SteamID> WorldState::FindSteamIDForName(const std::string_view& playerName) const
{
	std::optional<SteamID> retVal;
	time_point_t lastUpdated{};

	for (const auto& data : m_CurrentPlayerData)
	{
		if (data.second.GetStatus().m_Name == playerName && data.second.GetLastStatusUpdateTime() > lastUpdated)
		{
			retVal = data.second.GetSteamID();
			lastUpdated = data.second.GetLastStatusUpdateTime();
		}
	}

	return retVal;
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

std::optional<UserID_t> WorldState::FindUserID(const SteamID& id) const
{
	for (const auto& player : m_CurrentPlayerData)
	{
		if (player.second.GetSteamID() == id)
			return player.second.GetUserID();
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

IPlayer* WorldState::FindPlayer(const SteamID& id)
{
	return const_cast<IPlayer*>(std::as_const(*this).FindPlayer(id));
}

const IPlayer* WorldState::FindPlayer(const SteamID& id) const
{
	if (auto found = m_CurrentPlayerData.find(id); found != m_CurrentPlayerData.end())
		return &found->second;

	return nullptr;
}

size_t WorldState::GetApproxLobbyMemberCount() const
{
	return m_CurrentLobbyMembers.size() + m_PendingLobbyMembers.size();
}

cppcoro::generator<const IPlayer&> WorldState::GetLobbyMembers() const
{
	const auto GetPlayer = [&](const LobbyMember& member) -> const IPlayer*
	{
		assert(member != LobbyMember{});
		assert(member.m_SteamID.IsValid());

		if (auto found = m_CurrentPlayerData.find(member.m_SteamID); found != m_CurrentPlayerData.end())
		{
			[[maybe_unused]] const LobbyMember* testMember = found->second.GetLobbyMember();
			//assert(*testMember == member);
			return &found->second;
		}
		else
		{
			throw std::runtime_error("Missing player for lobby member!");
		}
	};

	for (const auto& member : m_CurrentLobbyMembers)
	{
		if (!member.IsValid())
			continue;

		if (auto found = GetPlayer(member))
			co_yield *found;
	}
	for (const auto& member : m_PendingLobbyMembers)
	{
		if (!member.IsValid())
			continue;

		if (std::any_of(m_CurrentLobbyMembers.begin(), m_CurrentLobbyMembers.end(),
			[&](const LobbyMember& member2) { return member.m_SteamID == member2.m_SteamID; }))
		{
			// Don't return two different instances with the same steamid.
			continue;
		}

		if (auto found = GetPlayer(member))
			co_yield *found;
	}
}

cppcoro::generator<IPlayer&> WorldState::GetLobbyMembers()
{
	for (const IPlayer& member : std::as_const(*this).GetLobbyMembers())
		co_yield const_cast<IPlayer&>(member);
}

cppcoro::generator<const IPlayer&> WorldState::GetPlayers() const
{
	for (const auto& pair : m_CurrentPlayerData)
		co_yield pair.second;
}

cppcoro::generator<IPlayer&> WorldState::GetPlayers()
{
	for (const IPlayer& player : std::as_const(*this).GetPlayers())
		co_yield const_cast<IPlayer&>(player);
}

void WorldState::OnConsoleLineParsed(WorldState& world, IConsoleLine& parsed)
{
	assert(&world == this);

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
	case ConsoleLineType::LobbyStatusFailed:
	{
		if (!m_CurrentLobbyMembers.empty() || !m_PendingLobbyMembers.empty())
		{
			m_CurrentLobbyMembers.clear();
			m_PendingLobbyMembers.clear();
			m_CurrentPlayerData.clear();
		}
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
			for (auto& player : m_CurrentPlayerData)
				player.second.m_ClientIndex = 0;

			QueuePlayerSummaryUpdate();
		}
		break;
	}
	case ConsoleLineType::ClientReachedServerSpawn:
	{
		// Reset current lobby members/player statuses
		//ClearLobbyState();
		m_IsLocalPlayerInitialized = false;
		m_IsVoteInProgress = false;
		DebugLogWarning("Client reached server spawn");
		break;
	}
	case ConsoleLineType::Chat:
	{
		auto& chatLine = static_cast<const ChatConsoleLine&>(parsed);
		if (auto sid = FindSteamIDForName(chatLine.GetPlayerName()))
		{
			if (auto player = FindPlayer(*sid))
			{
				m_EventBroadcaster.OnChatMsg(*this, *player, chatLine.GetMessage());
			}
			else
			{
				LogWarning("Dropped chat message with unknown IPlayer from "s
					<< std::quoted(chatLine.GetPlayerName()) << ": " << std::quoted(chatLine.GetMessage()));
			}
		}
		else
		{
			LogWarning("Dropped chat message with unknown SteamID from "s
				<< std::quoted(chatLine.GetPlayerName()) << ": " << std::quoted(chatLine.GetMessage()));
		}

		break;
	}
	case ConsoleLineType::ConfigExec:
	{
		auto& execLine = static_cast<const ConfigExecLine&>(parsed);
		const std::string_view& cfgName = execLine.GetConfigFileName();
		if (cfgName == "scout.cfg"sv ||
			cfgName == "sniper.cfg"sv ||
			cfgName == "soldier.cfg"sv ||
			cfgName == "demoman.cfg"sv ||
			cfgName == "medic.cfg"sv ||
			cfgName == "heavyweapons.cfg"sv ||
			cfgName == "pyro.cfg"sv ||
			cfgName == "spy.cfg"sv ||
			cfgName == "engineer.cfg"sv)
		{
			DebugLog("Spawned as "s << cfgName.substr(0, cfgName.size() - 3));
			m_IsLocalPlayerInitialized = true;
		}

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
		FindOrCreatePlayer(member.m_SteamID).m_Team = tfTeam;

		break;
	}
	case ConsoleLineType::Ping:
	{
		auto& pingLine = static_cast<const PingLine&>(parsed);
		if (auto found = FindSteamIDForName(pingLine.GetPlayerName()))
		{
			auto& playerData = FindOrCreatePlayer(*found);
			playerData.SetPing(pingLine.GetPing(), pingLine.GetTimestamp());
		}

		break;
	}
	case ConsoleLineType::PlayerStatus:
	{
		auto& statusLine = static_cast<const ServerStatusPlayerLine&>(parsed);
		auto newStatus = statusLine.GetPlayerStatus();
		auto& playerData = FindOrCreatePlayer(newStatus.m_SteamID);

		// Don't introduce stutter to our connection time view
		if (auto delta = (playerData.GetStatus().m_ConnectionTime - newStatus.m_ConnectionTime);
			delta < 2s && delta > -2s)
		{
			newStatus.m_ConnectionTime = playerData.GetStatus().m_ConnectionTime;
		}

		assert(playerData.GetStatus().m_SteamID == newStatus.m_SteamID);
		playerData.SetStatus(newStatus, statusLine.GetTimestamp());
		m_LastStatusUpdateTime = std::max(m_LastStatusUpdateTime, playerData.GetLastStatusUpdateTime());
		m_EventBroadcaster.OnPlayerStatusUpdate(*this, playerData);

		break;
	}
	case ConsoleLineType::PlayerStatusShort:
	{
		auto& statusLine = static_cast<const ServerStatusShortPlayerLine&>(parsed);
		const auto& status = statusLine.GetPlayerStatus();
		if (auto steamID = FindSteamIDForName(status.m_Name))
			FindOrCreatePlayer(*steamID).m_ClientIndex = status.m_ClientIndex;

		break;
	}
	case ConsoleLineType::KillNotification:
	{
		auto& killLine = static_cast<const KillNotificationLine&>(parsed);
		const auto localSteamID = m_Settings->GetLocalSteamID();
		const auto attackerSteamID = FindSteamIDForName(killLine.GetAttackerName());
		const auto victimSteamID = FindSteamIDForName(killLine.GetVictimName());

		if (attackerSteamID)
		{
			auto& attacker = FindOrCreatePlayer(*attackerSteamID);
			attacker.m_Scores.m_Kills++;

			if (victimSteamID == localSteamID)
				attacker.m_Scores.m_LocalKills++;
		}

		if (victimSteamID)
		{
			auto& victim = FindOrCreatePlayer(*victimSteamID);
			victim.m_Scores.m_Deaths++;

			if (attackerSteamID == localSteamID)
				victim.m_Scores.m_LocalDeaths++;
		}

		break;
	}
	case ConsoleLineType::SVC_UserMessage:
	{
		auto& userMsg = static_cast<const SVCUserMessageLine&>(parsed);
		switch (userMsg.GetUserMessageType())
		{
		case UserMessageType::VoteStart:
			m_IsVoteInProgress = true;
			break;
		case UserMessageType::VoteFailed:
		case UserMessageType::VotePass:
			m_IsVoteInProgress = false;
			break;
		}

		break;
	}

#if 0
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
#endif
	}
}

auto WorldState::FindOrCreatePlayer(const SteamID& id) -> PlayerExtraData&
{
	PlayerExtraData* data;
	if (auto found = m_CurrentPlayerData.find(id); found != m_CurrentPlayerData.end())
		data = &found->second;
	else
		data = &m_CurrentPlayerData.emplace(id, PlayerExtraData{ *this, id }).first->second;

	assert(data->GetSteamID() == id);
	return *data;
}

void WorldState::QueuePlayerSummaryUpdate()
{
	auto client = m_Settings->GetHTTPClient();
	if (!client)
		return;

	if (m_Settings->m_SteamAPIKey.empty())
		return;

	std::vector<SteamID> steamIDs;

	for (const IPlayer& member : GetLobbyMembers())
		steamIDs.push_back(member.GetSteamID());

	auto summary = SteamAPI::GetPlayerSummariesAsync(m_Settings->m_SteamAPIKey, std::move(steamIDs), *client);
	if (summary.valid())
		m_PlayerSummaryRequests.push_back(std::move(summary));
}

void WorldState::ApplyPlayerSummaries()
{
	for (auto it = m_PlayerSummaryRequests.begin(); it != m_PlayerSummaryRequests.end(); )
	{
		if (!mh::is_future_ready(*it))
		{
			++it;
			break;
		}

		auto summaryReqFuture = std::move(*it);
		it = m_PlayerSummaryRequests.erase(it);

		try
		{
			auto summaryReq = summaryReqFuture.get();
			for (SteamAPI::PlayerSummary& entry : summaryReq)
			{
				const auto steamID = entry.m_SteamID;
				FindOrCreatePlayer(steamID).m_PlayerSummary = std::move(entry);
			}

			DebugLog("Applied "s << summaryReq.size() << " player summaries from Steam web API");
		}
		catch (const std::exception& e)
		{
			LogError("Failed to apply player summaries from Steam web API: "s << e.what());
		}
	}
}

auto WorldState::GetTeamShareResult(const SteamID& id0, const SteamID& id1) const -> TeamShareResult
{
	return GetTeamShareResult(FindLobbyMemberTeam(id0), FindLobbyMemberTeam(id1));
}

WorldState::PlayerExtraData::PlayerExtraData(WorldState& world, SteamID id) :
	m_World(&world)
{
	m_Status.m_SteamID = id;
}

const LobbyMember* WorldState::PlayerExtraData::GetLobbyMember() const
{
	auto& world = GetWorld();
	const auto steamID = GetSteamID();
	for (const auto& member : world.m_CurrentLobbyMembers)
	{
		if (member.m_SteamID == steamID)
			return &member;
	}
	for (const auto& member : world.m_PendingLobbyMembers)
	{
		if (member.m_SteamID == steamID)
			return &member;
	}

	return nullptr;
}

std::optional<UserID_t> WorldState::PlayerExtraData::GetUserID() const
{
	if (m_Status.m_UserID > 0)
		return m_Status.m_UserID;

	return std::nullopt;
}

duration_t WorldState::PlayerExtraData::GetConnectedTime() const
{
	auto result = GetWorld().GetCurrentTime() - GetConnectionTime();
	//assert(result >= -1s);
	result = std::max<duration_t>(result, 0s);
	return result;
}

void WorldState::PlayerExtraData::SetStatus(PlayerStatus status, time_point_t timestamp)
{
	m_Status = std::move(status);
	m_PlayerNameSafe = CollapseNewlines(m_Status.m_Name);
	m_LastStatusUpdateTime = m_LastPingUpdateTime = timestamp;
}
void WorldState::PlayerExtraData::SetPing(uint16_t ping, time_point_t timestamp)
{
	m_Status.m_Ping = ping;
	m_LastPingUpdateTime = timestamp;
}

const std::any* WorldState::PlayerExtraData::FindDataStorage(const std::type_index& type) const
{
	if (auto found = m_UserData.find(type); found != m_UserData.end())
		return &found->second;

	return nullptr;
}

std::any& WorldState::PlayerExtraData::GetOrCreateDataStorage(const std::type_index& type)
{
	return m_UserData[type];
}
