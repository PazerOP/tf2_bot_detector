#include "WorldState.h"
#include "Actions.h"
#include "Config/Settings.h"
#include "ConsoleLines.h"
#include "FileMods.h"
#include "Log.h"
#include "RegexHelpers.h"
#include "PlatformSpecific/Shitty.h"

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace tf2_bot_detector;

WorldState::WorldState(const Settings& settings, const std::filesystem::path& conLogFile) :
	m_Settings(&settings), m_FileName(conLogFile)
{
	RequireTF2NotRunning();

	m_ConsoleLineListeners.insert(this);

	m_ChatMsgWrappers = RandomizeChatWrappers(m_Settings->m_TFDir);
	//const char8_t c1[] = u8"        ";
	//char c2[sizeof(c1)];
	//memcpy(c2, c1, sizeof(c1));
	//const auto test = ToU8(c2);
	//m_ChatMsgWrappers.first = ToMB(L"        ");
	//m_ChatMsgWrappers.second = ToMB(L"        ");
}

void WorldState::Parse(bool& linesProcessed, bool& snapshotUpdated, bool& consoleLinesUpdated)
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

			auto parseEnd = m_FileLineBuf.cbegin();
			ParseChunk(parseEnd, linesProcessed, snapshotUpdated, consoleLinesUpdated);

			m_ParsedLineCount += std::count(m_FileLineBuf.cbegin(), parseEnd, '\n');
			m_FileLineBuf.erase(m_FileLineBuf.begin(), parseEnd);
		}

		if (auto elapsed = clock::now() - startTime; elapsed >= 50ms)
			break;

	} while (readCount > 0);
}

bool WorldState::ParseChatMessage(const std::string_view& lineStr, striter& parseEnd, std::unique_ptr<IConsoleLine>& parsed)
{
	for (int i = 0; i < (int)ChatCategory::COUNT; i++)
	{
		const auto category = ChatCategory(i);

		auto& type = m_ChatMsgWrappers.m_Types[i];
		if (lineStr.starts_with(type.m_Full.first))
		{
			auto searchBuf = std::string_view(m_FileLineBuf).substr(
				&*lineStr.begin() - m_FileLineBuf.data() + type.m_Full.first.size());

			if (auto found = searchBuf.find(type.m_Full.second); found != lineStr.npos)
			{
				if (found > 256)
				{
					LogError("Searched more than 256 characters ("s << found
						<< ") for the end of the chat msg string, something is terribly wrong!");
				}

				searchBuf = searchBuf.substr(0, found);

				auto nameBegin = searchBuf.find(type.m_Name.first);
				auto nameEnd = searchBuf.find(type.m_Name.second);
				auto msgBegin = searchBuf.find(type.m_Message.first);
				auto msgEnd = searchBuf.find(type.m_Message.second);

				if (nameBegin != searchBuf.npos && nameEnd != searchBuf.npos && msgBegin != searchBuf.npos && msgEnd != searchBuf.npos)
				{
					parsed = std::make_unique<ChatConsoleLine>(m_CurrentTimestamp.GetSnapshot(),
						std::string(searchBuf.substr(nameBegin, nameEnd - nameBegin)),
						std::string(searchBuf.substr(msgBegin, msgEnd - msgBegin)),
						IsDead(category), IsTeam(category));
				}
				else
				{
					if (nameBegin == searchBuf.npos)
						LogError("Failed to find name begin sequence in chat message of type "s << category);
					if (nameEnd == searchBuf.npos)
						LogError("Failed to find name end sequence in chat message of type "s << category);
					if (msgBegin == searchBuf.npos)
						LogError("Failed to find message begin sequence in chat message of type "s << category);
					if (msgEnd == searchBuf.npos)
						LogError("Failed to find message end sequence in chat message of type "s << category);
				}

				parseEnd += found + type.m_Full.second.size();
				return true;
			}
			else
			{
				LogError("Failed to locate chat message wrapper end");
				return false; // Not enough characters in m_FileLineBuf. Try again later.
			}
		}
	}

	return true;
}

void WorldState::ParseChunk(striter& parseEnd, bool& linesProcessed, bool& snapshotUpdated, bool& consoleLinesUpdated)
{
	static const std::regex s_TimestampRegex(R"regex(\n(\d\d)\/(\d\d)\/(\d\d\d\d) - (\d\d):(\d\d):(\d\d):[ \n])regex", std::regex::optimize);

	std::smatch match;
	while (std::regex_search(parseEnd, m_FileLineBuf.cend(), match, s_TimestampRegex))
	{
		auto regexBegin = parseEnd;

		ParseLineResult result = ParseLineResult::Unparsed;
		bool skipTimestampParse = false;
		if (m_CurrentTimestamp.IsRecordedValid())
		{
			// If we have a valid snapshot, that means that there was a previously parsed
			// timestamp. The contents of that line is the current timestamp match's prefix
			// (the previous timestamp match was erased from the string)

			TrySnapshot(snapshotUpdated);
			linesProcessed = true;

			std::unique_ptr<IConsoleLine> parsed;

			const auto prefix = match.prefix();
			//const auto suffix = match.suffix();
			const std::string_view lineStr(&*prefix.first, prefix.length());

			if (ParseChatMessage(lineStr, regexBegin, parsed))
			{
				result = ParseLineResult::Modified;
			}
			else
			{
				return; // Try again later (not enough chars in buffer)
			}

			if (!parsed && result == ParseLineResult::Unparsed)
			{
				parsed = IConsoleLine::ParseConsoleLine(lineStr, m_CurrentTimestamp.GetSnapshot());
				if (parsed && parsed->GetType() == ConsoleLineType::Chat)
					LogError("Line was parsed as a chat message via old code path, this should never happen!");

				result = ParseLineResult::Success;
			}

			if (parsed)
			{
				if (result == ParseLineResult::Success || result == ParseLineResult::Modified)
				{
					for (auto listener : m_ConsoleLineListeners)
						listener->OnConsoleLineParsed(*this, *parsed);

					m_ConsoleLines.push_back(std::move(parsed));
					consoleLinesUpdated = true;
				}
			}
			else
			{
				for (auto listener : m_ConsoleLineListeners)
					listener->OnConsoleLineUnparsed(*this, lineStr);
			}
		}

		if (result != ParseLineResult::Modified)
		{
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
		else
		{
			m_CurrentTimestamp.InvalidateRecorded();
		}

		parseEnd = regexBegin;
	}
}

void WorldState::TrySnapshot(bool& snapshotUpdated)
{
	if ((!snapshotUpdated || !m_CurrentTimestamp.IsSnapshotValid()) && m_CurrentTimestamp.IsRecordedValid())
	{
		m_CurrentTimestamp.Snapshot();
		snapshotUpdated = true;
		m_EventBroadcaster.OnTimestampUpdate(*this);
	}
}

void WorldState::Update()
{
	if (!m_File)
	{
		// Try to truncate
		{
			std::error_code ec;
			const auto filesize = std::filesystem::file_size(m_FileName, ec);
			if (!ec)
			{
				std::filesystem::resize_file(m_FileName, 0, ec);
				if (ec)
					Log("Unable to truncate console log file, current size is "s << filesize);
				else
					Log("Truncated console log file");
			}
		}

		{
			FILE* temp = _fsopen(m_FileName.string().c_str(), "r", _SH_DENYNO);
			m_File.reset(temp);
		}

		if (m_File)
			m_ConsoleLines.clear();
	}

	bool snapshotUpdated = false;

	bool linesProcessed = false;
	bool consoleLinesUpdated = false;
	if (m_File)
	{
		Parse(linesProcessed, snapshotUpdated, consoleLinesUpdated);

		// Parse progress
		{
			const auto pos = ftell(m_File.get());
			const auto length = std::filesystem::file_size(m_FileName);
			m_ParseProgress = float(double(pos) / length);
		}
	}

	TrySnapshot(snapshotUpdated);

	if (linesProcessed)
		m_EventBroadcaster.OnUpdate(*this, consoleLinesUpdated);
}

void WorldState::AddWorldEventListener(IWorldEventListener* listener)
{
	m_EventBroadcaster.m_EventListeners.insert(listener);
}

void WorldState::RemoveWorldEventListener(IWorldEventListener* listener)
{
	m_EventBroadcaster.m_EventListeners.erase(listener);
}

void WorldState::AddConsoleLineListener(IConsoleLineListener* listener)
{
	m_ConsoleLineListeners.insert(listener);
}

void WorldState::RemoveConsoleLineListener(IConsoleLineListener* listener)
{
	m_ConsoleLineListeners.erase(listener);
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

mh::generator<const IPlayer*> WorldState::GetLobbyMembers() const
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

		co_yield GetPlayer(member);
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
		co_yield GetPlayer(member);
	}
}

mh::generator<IPlayer*> WorldState::GetLobbyMembers()
{
	for (const IPlayer* member : std::as_const(*this).GetLobbyMembers())
		co_yield const_cast<IPlayer*>(member);
}

mh::generator<const IPlayer*> WorldState::GetPlayers() const
{
	for (const auto& pair : m_CurrentPlayerData)
		co_yield &pair.second;
}

mh::generator<IPlayer*> WorldState::GetPlayers()
{
	for (const IPlayer* player : std::as_const(*this).GetPlayers())
		co_yield const_cast<IPlayer*>(player);
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
		}
		break;
	}
	case ConsoleLineType::ClientReachedServerSpawn:
	{
		// Reset current lobby members/player statuses
		//ClearLobbyState();
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
			playerData.m_Status.m_Ping = pingLine.GetPing();
			playerData.m_LastPingUpdateTime = pingLine.GetTimestamp();
		}

		break;
	}
	case ConsoleLineType::PlayerStatus:
	{
		auto& statusLine = static_cast<const ServerStatusPlayerLine&>(parsed);
		auto newStatus = statusLine.GetPlayerStatus();
		auto& playerData = FindOrCreatePlayer(newStatus.m_SteamID);

		// Don't introduce stutter to our connection time view
		if (auto delta = (playerData.m_Status.m_ConnectionTime - newStatus.m_ConnectionTime);
			delta < 2s && delta > -2s)
		{
			newStatus.m_ConnectionTime = playerData.m_Status.m_ConnectionTime;
		}

		assert(playerData.m_Status.m_SteamID == newStatus.m_SteamID);
		playerData.m_Status = newStatus;
		playerData.m_LastStatusUpdateTime = playerData.m_LastPingUpdateTime = statusLine.GetTimestamp();
		m_LastStatusUpdateTime = std::max(m_LastStatusUpdateTime, playerData.m_LastStatusUpdateTime);
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

		if (const auto attackerSteamID = FindSteamIDForName(killLine.GetAttackerName()))
			FindOrCreatePlayer(*attackerSteamID).m_Scores.m_Kills++;
		if (const auto victimSteamID = FindSteamIDForName(killLine.GetVictimName()))
			FindOrCreatePlayer(*victimSteamID).m_Scores.m_Deaths++;

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
		data = &m_CurrentPlayerData.emplace(id, *this).first->second;

	assert(!data->m_Status.m_SteamID.IsValid() || data->m_Status.m_SteamID == id);
	data->m_Status.m_SteamID = id;
	assert(data->GetSteamID() == id);

	return *data;
}

auto WorldState::GetTeamShareResult(const SteamID& id0, const SteamID& id1) const -> TeamShareResult
{
	return GetTeamShareResult(FindLobbyMemberTeam(id0), FindLobbyMemberTeam(id1));
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
	assert(result >= -1s);
	result = std::max<duration_t>(result, 0s);
	return result;
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
