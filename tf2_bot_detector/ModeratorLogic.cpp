#include "ModeratorLogic.h"
#include "Actions/Actions.h"
#include "Actions/RCONActionManager.h"
#include "Config/PlayerListJSON.h"
#include "Config/Rules.h"
#include "Config/Settings.h"
#include "Log.h"
#include "WorldState.h"

#include <mh/text/case_insensitive_string.hpp>
#include <mh/text/string_insertion.hpp>

#include <codecvt>
#include <cuchar>
#include <fstream>
#include <iomanip>
#include <locale>
#include <random>
#include <regex>

using namespace tf2_bot_detector;
using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;

void ModeratorLogic::Update()
{
	ProcessPlayerActions();
}

void ModeratorLogic::OnRuleMatch(const ModerationRule& rule, const IPlayer& player)
{
	for (PlayerAttributes attribute : rule.m_Actions.m_Mark)
	{
		if (SetPlayerAttribute(player, attribute))
			Log("Marked "s << player << " with " << attribute << " due to rule match with " << std::quoted(rule.m_Description));
	}
	for (PlayerAttributes attribute : rule.m_Actions.m_Unmark)
	{
		if (SetPlayerAttribute(player, attribute, false))
			Log("Unmarked "s << player << " with " << attribute << " due to rule match with " << std::quoted(rule.m_Description));
	}
}

void ModeratorLogic::OnPlayerStatusUpdate(WorldState& world, const IPlayer& player)
{
	ProcessDelayedBans(world.GetCurrentTime(), player);

	const auto name = player.GetNameUnsafe();
	const auto steamID = player.GetSteamID();

	if (m_Settings->m_AutoMark)
	{
		for (const ModerationRule& rule : m_Rules.GetRules())
		{
			if (!rule.Match(player))
				continue;

			OnRuleMatch(rule, player);
		}
	}
}

static bool IsCheaterConnectedWarning(const std::string_view& msg)
{
	static const std::regex s_IngameWarning(
		R"regex(Attention! There (?:is a|are \d+) cheaters? on the other team named .*\. Please kick them!)regex",
		std::regex::optimize);

	return std::regex_match(msg.begin(), msg.end(), s_IngameWarning);
}

void ModeratorLogic::OnChatMsg(WorldState& world, IPlayer& player, const std::string_view& msg)
{
	bool botMsgDetected = IsCheaterConnectedWarning(msg);
	// Check if it is a moderation message from someone else
	if (m_Settings->m_AutoTempMute &&
		!m_PlayerList.HasPlayerAttribute(player, { PlayerAttributes::Cheater, PlayerAttributes::Exploiter }))
	{
		if (auto localPlayer = GetLocalPlayer();
			localPlayer && (player.GetSteamID() != localPlayer->GetSteamID()))
		{
			static const std::regex s_ConnectingWarning(
				R"regex(Heads up! There (?:is a|are \d+) known cheaters? joining the other team! Names? unknown until they fully join\.)regex",
				std::regex::optimize);

			if (botMsgDetected || std::regex_match(msg.begin(), msg.end(), s_ConnectingWarning))
			{
				botMsgDetected = true;
				Log("Detected message from "s << player << " as another instance of TF2BD: "s << std::quoted(msg));
				SetUserRunningTool(player, true);

				if (player.GetUserID() < localPlayer->GetUserID())
				{
					assert(!IsBotLeader());
					Log("Deferring all cheater warnings for a bit because we think "s << player << " is running TF2BD");
					m_NextCheaterWarningTime = m_NextConnectingCheaterWarningTime =
						world.GetCurrentTime() + CHEATER_WARNING_INTERVAL_NONLOCAL;
				}
			}
		}
	}

	if (m_Settings->m_AutoMark && !botMsgDetected)
	{
		for (const ModerationRule& rule : m_Rules.GetRules())
		{
			if (!rule.Match(player, msg))
				continue;

			OnRuleMatch(rule, player);
			DebugLog("Chat message rule match: "s << std::quoted(msg));
		}
	}
}

void ModeratorLogic::HandleFriendlyCheaters(uint8_t friendlyPlayerCount, uint8_t connectedFriendlyPlayerCount,
	const std::vector<const IPlayer*>& friendlyCheaters)
{
	if (!m_Settings->m_AutoVotekick)
		return;

	if (friendlyCheaters.empty())
		return; // Nothing to do

	constexpr float MIN_QUORUM = 0.6f;
	if (auto quorum = float(connectedFriendlyPlayerCount) / friendlyPlayerCount; quorum <= MIN_QUORUM)
	{
		LogWarning("Impossible to pass a successful votekick against "s << friendlyCheaters.size()
			<< " friendly cheaters: only " << int(quorum * 100)
			<< "% of players on our team are connected right now (need >= " << int(MIN_QUORUM * 100) << "%)");
		return;
	}

	if (auto ratio = float(friendlyPlayerCount) / friendlyCheaters.size(); ratio <= 0.501f)
	{
		Log("Impossible to pass a successful votekick against "s << friendlyCheaters.size()
			<< " friendly cheaters: our team is " << (100 - int(ratio * 100)) << "% cheaters");
		return;
	}

	// Votekick the first one that is actually connected
	for (const IPlayer* cheater : friendlyCheaters)
	{
		if (cheater->GetSteamID() == m_Settings->GetLocalSteamID())
			continue;

		if (cheater->GetConnectionState() == PlayerStatusState::Active)
		{
			if (InitiateVotekick(*cheater, KickReason::Cheating))
				break;
		}
	}
}

template<typename TIter>
static std::vector<std::string> GetJoinedStrings(const TIter& begin, const TIter& end, const std::string_view& separator)
{
	std::vector<std::string> retVal;
	std::string strBuf;
	for (auto it = begin; it != end; ++it)
	{
		if (it != begin)
			strBuf << separator;

		strBuf << *it;
		retVal.push_back(strBuf);
	}

	return retVal;
}

void ModeratorLogic::HandleEnemyCheaters(uint8_t enemyPlayerCount,
	const std::vector<IPlayer*>& enemyCheaters, const std::vector<IPlayer*>& connectingEnemyCheaters)
{
	if (enemyCheaters.empty() && connectingEnemyCheaters.empty())
		return;

	if (const auto cheaterCount = (enemyCheaters.size() + connectingEnemyCheaters.size());
		uint8_t(enemyPlayerCount / 2) <= cheaterCount)
	{
		Log("Impossible to pass a successful votekick against "s << cheaterCount << " enemy cheaters. Skipping all warnings.");
		return;
	}

	if (!enemyCheaters.empty())
		HandleConnectedEnemyCheaters(enemyCheaters);
	else if (!connectingEnemyCheaters.empty())
		HandleConnectingEnemyCheaters(connectingEnemyCheaters);
}

void ModeratorLogic::HandleConnectedEnemyCheaters(const std::vector<IPlayer*>& enemyCheaters)
{
	const auto now = clock_t::now();

	// There are enough people on the other team to votekick the cheater(s)
	std::string logMsg;
	logMsg << "Telling the other team about " << enemyCheaters.size() << " cheater(s) named ";

	const bool isBotLeader = IsBotLeader();
	bool needsWarning = false;
	std::vector<std::string> chatMsgCheaterNames;
	for (IPlayer* cheater : enemyCheaters)
	{
		if (cheater->GetNameSafe().empty())
			continue; // Theoretically this should never happen, but don't embarass ourselves

		logMsg << "\n\t" << cheater;
		chatMsgCheaterNames.emplace_back(cheater->GetNameSafe());

		auto& cheaterData = cheater->GetOrCreateData<PlayerExtraData>();

		if (isBotLeader)
		{
			// We're supposedly in charge
			DebugLog("We're bot leader: Triggered ACTIVE warning for "s << cheater);
			needsWarning = true;
		}
		else if (cheaterData.m_WarningDelayEnd.has_value())
		{
			if (now >= cheaterData.m_WarningDelayEnd)
			{
				DebugLog("We're not bot leader: Delay expired for ACTIVE cheater "s << cheater);
				needsWarning = true;
			}
			else
			{
				DebugLog("We're not bot leader: "s << to_seconds(cheaterData.m_WarningDelayEnd.value() - now)
					<< " seconds remaining for ACTIVE cheater " << cheater);
			}
		}
		else if (!cheaterData.m_WarningDelayEnd.has_value())
		{
			DebugLog("We're not bot leader: Starting delay for ACTIVE cheater "s << cheater);
			cheaterData.m_WarningDelayEnd = now + CHEATER_WARNING_DELAY;
		}
	}

	if (!needsWarning)
		return;

	if (chatMsgCheaterNames.size() > 0)
	{
		constexpr char FMT_ONE_CHEATER[] = "Attention! There is a cheater on the other team named %s. Please kick them!";
		constexpr char FMT_MULTIPLE_CHEATERS[] = "Attention! There are %u cheaters on the other team named %s. Please kick them!";
		//constexpr char FMT_MANY_CHEATERS[] =     "Attention! There are %u cheaters on the other team including %s. Please kick them!";
		constexpr size_t MAX_CHATMSG_LENGTH = 127;
		constexpr size_t MAX_NAMES_LENGTH_ONE = MAX_CHATMSG_LENGTH - std::size(FMT_ONE_CHEATER) - 1 - 2;
		constexpr size_t MAX_NAMES_LENGTH_MULTIPLE = MAX_CHATMSG_LENGTH - std::size(FMT_MULTIPLE_CHEATERS) - 1 - 1 - 2;
		//constexpr size_t MAX_NAMES_LENGTH_MANY = MAX_CHATMSG_LENGTH - std::size(FMT_MANY_CHEATERS) - 1 - 1 - 2;

		static_assert(MAX_NAMES_LENGTH_ONE >= 32);
		std::string chatMsg;
		if (chatMsgCheaterNames.size() == 1)
		{
			chatMsg.resize(256);
			chatMsg.resize(sprintf_s(chatMsg.data(), 256, FMT_ONE_CHEATER, chatMsgCheaterNames.front().c_str()));
		}
		else
		{
			assert(chatMsgCheaterNames.size() > 0);
			std::string cheaters;

			for (std::string cheaterNameStr : GetJoinedStrings(chatMsgCheaterNames.begin(), chatMsgCheaterNames.end(), ", "sv))
			{
				if (cheaters.empty() || cheaterNameStr.size() <= MAX_NAMES_LENGTH_MULTIPLE)
					cheaters = std::move(cheaterNameStr);
				else if (cheaterNameStr.size() > MAX_NAMES_LENGTH_MULTIPLE)
					break;
			}

			chatMsg.resize(256);
			chatMsg.resize(sprintf_s(chatMsg.data(), 256, FMT_MULTIPLE_CHEATERS, chatMsgCheaterNames.size(), cheaters.c_str()));
		}

		assert(chatMsg.size() <= 127);

		if (now >= m_NextCheaterWarningTime)
		{
			if (m_Settings->m_AutoChatWarnings && m_ActionManager->QueueAction<ChatMessageAction>(chatMsg))
			{
				Log(logMsg, { 1, 0, 0, 1 });
				m_NextCheaterWarningTime = now + CHEATER_WARNING_INTERVAL;
			}
		}
		else
		{
			DebugLog("HandleEnemyCheaters(): Skipping cheater warnings for "s << to_seconds(m_NextCheaterWarningTime - now) << " seconds");
		}
	}
}

void ModeratorLogic::HandleConnectingEnemyCheaters(const std::vector<IPlayer*>& connectingEnemyCheaters)
{
	const auto now = clock_t::now();
	if (now < m_NextConnectingCheaterWarningTime)
	{
		DebugLog("HandleEnemyCheaters(): Discarding connection warnings ("s
			<< to_seconds(m_NextConnectingCheaterWarningTime - now) << " seconds left)");

		// Assume someone else with a lower userid is in charge, discard warnings about
		// connecting enemy cheaters while it looks like they are doing stuff
		for (IPlayer* cheater : connectingEnemyCheaters)
			cheater->GetOrCreateData<PlayerExtraData>().m_PreWarnedOtherTeam = true;

		return;
	}

	const bool isBotLeader = IsBotLeader();
	bool needsWarning = false;
	for (IPlayer* cheater : connectingEnemyCheaters)
	{
		auto& cheaterData = cheater->GetOrCreateData<PlayerExtraData>();
		if (cheaterData.m_PreWarnedOtherTeam)
			continue; // Already included in a warning

		if (isBotLeader)
		{
			// We're supposedly in charge
			DebugLog("We're bot leader: Triggered connecting warning for "s << cheater);
			needsWarning = true;
			break;
		}
		else if (cheaterData.m_ConnectingWarningDelayEnd.has_value())
		{
			if (now >= cheaterData.m_ConnectingWarningDelayEnd)
			{
				DebugLog("We're not bot leader: Delay expired for connecting cheater "s << cheater);
				needsWarning = true;
				break;
			}
			else
			{
				DebugLog("We're not bot leader: "s << to_seconds(cheaterData.m_ConnectingWarningDelayEnd.value() - now)
					<< " seconds remaining for connecting cheater " << cheater);
			}
		}
		else if (!cheaterData.m_ConnectingWarningDelayEnd.has_value())
		{
			DebugLog("We're not bot leader: Starting delay for connecting cheater "s << cheater);
			cheaterData.m_ConnectingWarningDelayEnd = now + CHEATER_WARNING_DELAY;
		}
	}

	if (!needsWarning || !m_Settings->m_AutoChatWarnings || !m_Settings->m_AutoChatWarningsConnecting)
		return;

	char chatMsg[128];
	if (connectingEnemyCheaters.size() == 1)
	{
		strcpy_s(chatMsg, "Heads up! There is a known cheater joining the other team! Name unknown until they fully join.");
	}
	else
	{
		sprintf_s(chatMsg, "Heads up! There are %zu known cheaters joining the other team! Names unknown until they fully join.",
			connectingEnemyCheaters.size());
	}

	Log("Telling other team about "s << connectingEnemyCheaters.size() << " cheaters currently connecting");
	if (m_ActionManager->QueueAction<ChatMessageAction>(chatMsg))
	{
		for (IPlayer* cheater : connectingEnemyCheaters)
			cheater->GetOrCreateData<PlayerExtraData>().m_PreWarnedOtherTeam = true;
	}
}

void ModeratorLogic::ProcessPlayerActions()
{
	const auto now = m_World->GetCurrentTime();
	if ((now - m_LastPlayerActionsUpdate) < 1s)
	{
		return;
	}
	else
	{
		m_LastPlayerActionsUpdate = now;
	}

	if (auto self = m_World->FindPlayer(m_Settings->GetLocalSteamID());
		(self && self->GetConnectionState() != PlayerStatusState::Active) ||
		!m_World->IsLocalPlayerInitialized())
	{
		DebugLog("Skipping ProcessPlayerActions() because we are not fully connected yet");
		return;
	}

	// Don't process actions if we're way out of date
	[[maybe_unused]] const auto dbgDeltaTime = to_seconds(clock_t::now() - now);
	if ((clock_t::now() - now) > 15s)
		return;

	const auto myTeam = TryGetMyTeam();
	if (!myTeam)
		return; // We don't know what team we're on, so we can't really take any actions.

	uint8_t totalEnemyPlayers = 0;
	uint8_t connectedEnemyPlayers = 0;
	uint8_t totalFriendlyPlayers = 0;
	uint8_t connectedFriendlyPlayers = 0;
	std::vector<IPlayer*> enemyCheaters;
	std::vector<const IPlayer*> friendlyCheaters;
	std::vector<IPlayer*> connectingEnemyCheaters;

	const bool isBotLeader = IsBotLeader();
	bool needsEnemyWarning = false;
	for (IPlayer& player : m_World->GetLobbyMembers())
	{
		const bool isPlayerConnected = player.GetConnectionState() == PlayerStatusState::Active;
		const bool isCheater = HasPlayerAttribute(player, PlayerAttributes::Cheater);
		const auto teamShareResult = m_World->GetTeamShareResult(*myTeam, player);
		if (teamShareResult == TeamShareResult::SameTeams)
		{
			if (isPlayerConnected)
			{
				connectedFriendlyPlayers++;

				if (isCheater)
					friendlyCheaters.push_back(&player);
			}

			totalFriendlyPlayers++;
		}
		else if (teamShareResult == TeamShareResult::OppositeTeams)
		{
			if (isPlayerConnected)
			{
				connectedEnemyPlayers++;

				if (isCheater && !player.GetNameSafe().empty())
					enemyCheaters.push_back(&player);
			}
			else
			{
				if (isCheater)
					connectingEnemyCheaters.push_back(&player);
			}

			totalEnemyPlayers++;
		}
	}

	HandleEnemyCheaters(totalEnemyPlayers, enemyCheaters, connectingEnemyCheaters);
	HandleFriendlyCheaters(totalFriendlyPlayers, connectedFriendlyPlayers, friendlyCheaters);
}

bool ModeratorLogic::SetPlayerAttribute(const IPlayer& player, PlayerAttributes attribute, bool set)
{
	bool attributeChanged = false;

	m_PlayerList.ModifyPlayer(player.GetSteamID(), [&](PlayerListData& data)
		{
			attributeChanged = data.m_Attributes.HasAttribute(attribute) != set;

			data.m_Attributes.SetAttribute(attribute, set);

			if (!data.m_LastSeen)
				data.m_LastSeen.emplace();

			data.m_LastSeen->m_Time = m_World->GetCurrentTime();

			if (const auto& name = player.GetNameUnsafe(); !name.empty())
				data.m_LastSeen->m_PlayerName = name;

			return ModifyPlayerAction::Modified;
		});

	return attributeChanged;
}

bool ModeratorLogic::HasPlayerAttribute(const SteamID& id, PlayerAttributes markType) const
{
	// ModeratorLogic::HasPlayerAttribute is an important abstraction since later we
	// might have multiple player lists
	return m_PlayerList.HasPlayerAttribute(id, markType);
}

std::optional<LobbyMemberTeam> ModeratorLogic::TryGetMyTeam() const
{
	return m_World->FindLobbyMemberTeam(m_Settings->GetLocalSteamID());
}

TeamShareResult ModeratorLogic::GetTeamShareResult(const SteamID& id) const
{
	return m_World->GetTeamShareResult(id, m_Settings->GetLocalSteamID());
}

const IPlayer* ModeratorLogic::GetLocalPlayer() const
{
	return m_World->FindPlayer(m_Settings->GetLocalSteamID());
}

bool ModeratorLogic::IsBotLeader() const
{
	auto leader = GetBotLeader();
	return leader && (leader->GetSteamID() == m_Settings->GetLocalSteamID());
}

const IPlayer* ModeratorLogic::GetBotLeader() const
{
	auto localPlayer = GetLocalPlayer();
	if (!localPlayer)
		return nullptr;

	UserID_t localUserID;
	if (auto id = localPlayer->GetUserID())
		localUserID = *id;
	else
		return nullptr;

	const auto now = m_World->GetCurrentTime();
	for (const IPlayer& player : m_World->GetPlayers())
	{
		if (player.GetTimeSinceLastStatusUpdate() > 20s)
			continue;

		if (player.GetSteamID() == localPlayer->GetSteamID())
			continue;

		if (player.GetUserID() >= localUserID)
			continue;

		if (IsUserRunningTool(player))
			return &player;
	}

	return localPlayer;
}

duration_t ModeratorLogic::TimeToConnectingCheaterWarning() const
{
	return m_NextConnectingCheaterWarningTime - m_World->GetCurrentTime();
}

duration_t ModeratorLogic::TimeToCheaterWarning() const
{
	return m_NextCheaterWarningTime - m_World->GetCurrentTime();
}

bool ModeratorLogic::IsUserRunningTool(const SteamID& id) const
{
	return m_PlayersRunningTool.contains(id);
}
void ModeratorLogic::SetUserRunningTool(const SteamID& id, bool isRunningTool)
{
	if (isRunningTool)
		m_PlayersRunningTool.insert(id);
	else
		m_PlayersRunningTool.erase(id);
}

void ModeratorLogic::ReloadConfigFiles()
{
	m_PlayerList.LoadFiles();
	m_Rules.LoadFiles();
}

ModeratorLogic::ModeratorLogic(WorldState& world, const Settings& settings, RCONActionManager& actionManager) :
	m_World(&world),
	m_Settings(&settings),
	m_ActionManager(&actionManager),
	m_PlayerList(settings),
	m_Rules(settings)
{
	m_World->AddConsoleLineListener(this);
	m_World->AddWorldEventListener(this);
}

bool ModeratorLogic::InitiateVotekick(const IPlayer& player, KickReason reason)
{
	const auto userID = player.GetUserID();
	if (!userID)
	{
		Log("Wanted to kick "s << player << ", but could not find userid");
		return false;
	}

	if (m_ActionManager->QueueAction<KickAction>(userID.value(), reason))
		Log("InitiateVotekick on "s << player << ": " << reason);

	return true;
}

void ModeratorLogic::ProcessDelayedBans(time_point_t timestamp, const IPlayer& updatedStatus)
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

		if (ban.m_PlayerName == updatedStatus.GetNameUnsafe())
		{
			if (SetPlayerAttribute(updatedStatus, PlayerAttributes::Cheater))
			{
				Log("Applying delayed ban ("s << to_seconds(timeSince) << " second delay) to player " << updatedStatus);
			}

			RemoveBan();
			break;
		}
	}
}
