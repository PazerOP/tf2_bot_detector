#include "ModeratorLogic.h"
#include "Actions/Actions.h"
#include "Actions/RCONActionManager.h"
#include "Config/PlayerListJSON.h"
#include "Config/Rules.h"
#include "Config/Settings.h"
#include "ConsoleLog/ConsoleLineListener.h"
#include "ConsoleLog/IConsoleLine.h"
#include "ConsoleLog/ConsoleLines.h"
#include "GameData/UserMessageType.h"
#include "IPlayer.h"
#include "Log.h"
#include "PlayerStatus.h"
#include "WorldEventListener.h"
#include "WorldState.h"

#include <mh/algorithm/algorithm_generic.hpp>
#include <mh/algorithm/multi_compare.hpp>
#include <mh/text/case_insensitive_string.hpp>
#include <mh/text/fmtstr.hpp>
#include <mh/text/string_insertion.hpp>

#include <iomanip>
#include <map>
#include <regex>
#include <unordered_set>

using namespace tf2_bot_detector;
using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;

namespace
{
	class ModeratorLogic final : public IModeratorLogic, AutoConsoleLineListener, AutoWorldEventListener
	{
	public:
		ModeratorLogic(IWorldState& world, const Settings& settings, IRCONActionManager& actionManager);

		void Update() override;

		struct Cheater
		{
			Cheater(IPlayer& player, PlayerMarks marks) : m_Player(player), m_Marks(std::move(marks)) {}

			std::reference_wrapper<IPlayer> m_Player;
			PlayerMarks m_Marks;

			IPlayer* operator->() const { return &m_Player.get(); }
		};

		PlayerMarks GetPlayerAttributes(const SteamID& id) const override;
		PlayerMarks HasPlayerAttributes(const SteamID& id, const PlayerAttributesList& attributes, AttributePersistence persistence) const override;
		bool InitiateVotekick(const IPlayer& player, KickReason reason, const PlayerMarks* marks = nullptr) override;

		bool SetPlayerAttribute(const IPlayer& id, PlayerAttribute markType, AttributePersistence persistence, bool set = true) override;

		std::optional<LobbyMemberTeam> TryGetMyTeam() const;
		TeamShareResult GetTeamShareResult(const SteamID& id) const override;
		const IPlayer* GetLocalPlayer() const;

		// Are we the leader of the bots? AKA do we have the lowest userID of
		// the pool of players we think are running the bot?
		bool IsBotLeader() const;
		const IPlayer* GetBotLeader() const override;

		duration_t TimeToConnectingCheaterWarning() const;
		duration_t TimeToCheaterWarning() const;

		bool IsUserRunningTool(const SteamID& id) const override;
		void SetUserRunningTool(const SteamID& id, bool isRunningTool = true) override;

		size_t GetBlacklistedPlayerCount() const override { return m_PlayerList.GetPlayerCount(); }
		size_t GetRuleCount() const override { return m_Rules.GetRuleCount(); }

		void ReloadConfigFiles() override;

	private:
		IWorldState* m_World = nullptr;
		const Settings* m_Settings = nullptr;
		IRCONActionManager* m_ActionManager = nullptr;

		struct PlayerExtraData
		{
			// If this is a known cheater, warn them ahead of time that the player is connecting, but only once
			// (we don't know the cheater's name yet, so don't spam if they can't do anything about it yet)
			bool m_PreWarnedOtherTeam = false;

			// If we're not the bot leader, prevent this player from triggering
			// any warnings (but still participates in other warnings!!!)
			std::optional<time_point_t> m_ConnectingWarningDelayEnd;
			std::optional<time_point_t> m_WarningDelayEnd;

			struct
			{
				time_point_t m_LastTransmission{};
				duration_t m_TotalTransmissions{};
			} m_Voice;
		};

		// Steam IDs of players that we think are running the tool.
		std::unordered_set<SteamID> m_PlayersRunningTool;

		void OnPlayerStatusUpdate(IWorldState& world, const IPlayer& player) override;
		void OnChatMsg(IWorldState& world, IPlayer& player, const std::string_view& msg) override;

		void OnRuleMatch(const ModerationRule& rule, const IPlayer& player);

		// How long inbetween accusations
		static constexpr duration_t CHEATER_WARNING_INTERVAL = std::chrono::seconds(20);

		// The soonest we can make an accusation after having seen an accusation in chat from a bot leader.
		// This must be longer than CHEATER_WARNING_INTERVAL.
		static constexpr duration_t CHEATER_WARNING_INTERVAL_NONLOCAL = CHEATER_WARNING_INTERVAL + std::chrono::seconds(10);

		// How long we wait between determining someone is cheating and actually accusing them.
		// This delay exists to give a bot leader a chance to make an accusation.
		static constexpr duration_t CHEATER_WARNING_DELAY = std::chrono::seconds(10);

		time_point_t m_NextConnectingCheaterWarningTime{};  // The soonest we can warn about cheaters connecting to the server
		time_point_t m_NextCheaterWarningTime{};            // The soonest we can warn about connected cheaters on the other team
		time_point_t m_LastPlayerActionsUpdate{};
		void ProcessPlayerActions();
		void HandleFriendlyCheaters(uint8_t friendlyPlayerCount, uint8_t connectedFriendlyPlayerCount,
			const std::vector<Cheater>& friendlyCheaters);
		void HandleEnemyCheaters(uint8_t enemyPlayerCount, const std::vector<Cheater>& enemyCheaters,
			const std::vector<Cheater>& connectingEnemyCheaters);
		void HandleConnectedEnemyCheaters(const std::vector<Cheater>& enemyCheaters);
		void HandleConnectingEnemyCheaters(const std::vector<Cheater>& connectingEnemyCheaters);

		// Minimum interval between callvote commands (the 150 comes from the default value of sv_vote_creation_timer)
		static constexpr duration_t MIN_VOTEKICK_INTERVAL = std::chrono::seconds(150);
		time_point_t m_LastVoteCallTime{}; // Last time we called a votekick on someone
		duration_t GetTimeSinceLastCallVote() const { return tfbd_clock_t::now() - m_LastVoteCallTime; }

		PlayerListJSON m_PlayerList;
		ModerationRules m_Rules;
	};

	template<typename CharT, typename Traits>
	static std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const ModeratorLogic::Cheater& cheater)
	{
		assert(cheater.m_Marks);
		os << cheater.m_Player.get() << " (marked in ";

		const auto markCount = cheater.m_Marks.m_Marks.size();
		for (size_t i = 0; i < markCount; i++)
		{
			const auto& mark = cheater.m_Marks.m_Marks[i];

			if (i != 0)
				os << ", ";

			if (i == (markCount - 1) && markCount > 1)
				os << "and ";

			os << std::quoted(mark.m_FileName);
		}

		os << ')';

		return os;
	}
}

std::unique_ptr<IModeratorLogic> IModeratorLogic::Create(IWorldState& world,
	const Settings& settings, IRCONActionManager& actionManager)
{
	return std::make_unique<ModeratorLogic>(world, settings, actionManager);
}

void ModeratorLogic::Update()
{
	ProcessPlayerActions();
}

void ModeratorLogic::OnRuleMatch(const ModerationRule& rule, const IPlayer& player)
{
	for (PlayerAttribute attribute : rule.m_Actions.m_Mark)
	{
		if (SetPlayerAttribute(player, attribute, AttributePersistence::Saved))
			Log("Marked {} with {:v} due to rule match with {}", player, mh::enum_fmt(attribute), std::quoted(rule.m_Description));
	}
	for (PlayerAttribute attribute : rule.m_Actions.m_TransientMark)
	{
		if (SetPlayerAttribute(player, attribute, AttributePersistence::Transient))
			Log("[TRANSIENT] Marked {} with {:v} due to rule match with {}", player, mh::enum_fmt(attribute), std::quoted(rule.m_Description));
	}
	for (PlayerAttribute attribute : rule.m_Actions.m_Unmark)
	{
		if (SetPlayerAttribute(player, attribute, AttributePersistence::Saved, false))
			Log("Unmarked {} with {:v} due to rule match with {}", player, mh::enum_fmt(attribute), std::quoted(rule.m_Description));
	}
}

void ModeratorLogic::OnPlayerStatusUpdate(IWorldState& world, const IPlayer& player)
{
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

void ModeratorLogic::OnChatMsg(IWorldState& world, IPlayer& player, const std::string_view& msg)
{
	bool botMsgDetected = IsCheaterConnectedWarning(msg);
	// Check if it is a moderation message from someone else
	if (m_Settings->m_AutoTempMute &&
		!m_PlayerList.HasPlayerAttributes(player, { PlayerAttribute::Cheater, PlayerAttribute::Exploiter }))
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
				Log("Detected message from {} as another instance of TF2BD: {}", player, std::quoted(msg));
				SetUserRunningTool(player, true);

				if (player.GetUserID() < localPlayer->GetUserID())
				{
					assert(!IsBotLeader());
					Log("Deferring all cheater warnings for a bit because we think {} is running TF2BD", player);
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
			Log("Chat message rule match for {}: {}", rule.m_Description, std::quoted(msg));
		}
	}
}

static bool CanPassVote(size_t totalPlayerCount, size_t cheaterCount, float* cheaterRatio = nullptr)
{
	if (cheaterRatio)
		*cheaterRatio = float(cheaterCount) / totalPlayerCount;

	return (cheaterCount * 2) < totalPlayerCount;
}

void ModeratorLogic::HandleFriendlyCheaters(uint8_t friendlyPlayerCount, uint8_t connectedFriendlyPlayerCount,
	const std::vector<Cheater>& friendlyCheaters)
{
	if (!m_Settings->m_AutoVotekick)
		return;

	if (friendlyCheaters.empty())
		return; // Nothing to do

	constexpr float MIN_QUORUM = 0.61f;
	if (auto quorum = float(connectedFriendlyPlayerCount) / friendlyPlayerCount; quorum <= MIN_QUORUM)
	{
		LogWarning("Impossible to pass a successful votekick against "s << friendlyCheaters.size()
			<< " friendly cheaters: only " << int(quorum * 100)
			<< "% of players on our team are connected right now (need >= " << int(MIN_QUORUM * 100) << "%)");
		return;
	}

	if (float cheaterRatio; !CanPassVote(friendlyPlayerCount, friendlyCheaters.size(), &cheaterRatio))
	{
		LogWarning("Impossible to pass a successful votekick against "s << friendlyCheaters.size()
			<< " friendly cheaters: our team is " << int(cheaterRatio * 100) << "% cheaters");
		return;
	}

	// Votekick the first one that is actually connected
	for (const Cheater& cheater : friendlyCheaters)
	{
		if (cheater->GetSteamID() == m_Settings->GetLocalSteamID())
			continue;

		if (cheater->GetConnectionState() == PlayerStatusState::Active)
		{
			if (InitiateVotekick(cheater.m_Player, KickReason::Cheating, &cheater.m_Marks))
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
	const std::vector<Cheater>& enemyCheaters, const std::vector<Cheater>& connectingEnemyCheaters)
{
	if (enemyCheaters.empty() && connectingEnemyCheaters.empty())
		return;

	const auto cheaterCount = enemyCheaters.size() + connectingEnemyCheaters.size();
	if (float cheaterRatio; !CanPassVote(enemyPlayerCount, cheaterCount, &cheaterRatio))
	{
		LogWarning("Impossible to pass a successful votekick against "s << cheaterCount
			<< " enemy cheaters (enemy team is " << int(cheaterRatio * 100) << "% cheaters). Skipping all warnings.");
		return;
	}

	if (!enemyCheaters.empty())
		HandleConnectedEnemyCheaters(enemyCheaters);
	else if (!connectingEnemyCheaters.empty())
		HandleConnectingEnemyCheaters(connectingEnemyCheaters);
}

void ModeratorLogic::HandleConnectedEnemyCheaters(const std::vector<Cheater>& enemyCheaters)
{
	const auto now = tfbd_clock_t::now();

	// There are enough people on the other team to votekick the cheater(s)
	std::string logMsg = mh::format("Telling the other team about {} cheater(s) named ", enemyCheaters.size());

	const bool isBotLeader = IsBotLeader();
	bool needsWarning = false;
	std::vector<std::string> chatMsgCheaterNames;
	std::multimap<std::string, Cheater> cheaterDebugWarnings;
	for (auto& cheater : enemyCheaters)
	{
		if (cheater->GetNameSafe().empty())
			continue; // Theoretically this should never happen, but don't embarass ourselves

		mh::format_to(std::back_inserter(logMsg), "\n\t{}", cheater);
		chatMsgCheaterNames.emplace_back(cheater->GetNameSafe());

		auto& cheaterData = cheater->GetOrCreateData<PlayerExtraData>();

		if (isBotLeader)
		{
			// We're supposedly in charge
			cheaterDebugWarnings.emplace("We're bot leader: Triggered ACTIVE warning for ", cheater);
			needsWarning = true;
		}
		else if (cheaterData.m_WarningDelayEnd.has_value())
		{
			if (now >= cheaterData.m_WarningDelayEnd)
			{
				cheaterDebugWarnings.emplace("We're not bot leader: Delay expired for ACTIVE cheater(s) ", cheater);
				needsWarning = true;
			}
			else
			{
				cheaterDebugWarnings.emplace(
					mh::format("We're not bot leader: {} seconds remaining for ACTIVE cheater(s) ", to_seconds(cheaterData.m_WarningDelayEnd.value() - now)),
					cheater);
			}
		}
		else if (!cheaterData.m_WarningDelayEnd.has_value())
		{
			cheaterDebugWarnings.emplace("We're not bot leader: Starting delay for ACTIVE cheater(s) ", cheater);
			cheaterData.m_WarningDelayEnd = now + CHEATER_WARNING_DELAY;
		}
	}

	mh::for_each_multimap_group(cheaterDebugWarnings,
		[&](const std::string_view& msgBase, auto cheatersBegin, auto cheatersEnd)
		{
			mh::fmtstr<2048> msgFmt;
			msgFmt.puts(msgBase);

			for (auto it = cheatersBegin; it != cheatersEnd; ++it)
			{
				if (it != cheatersBegin)
					msgFmt.puts(", ");

				msgFmt.fmt("{}", it->second);
			}
		});

	if (!needsWarning)
		return;

	if (chatMsgCheaterNames.size() > 0)
	{
		constexpr char FMT_ONE_CHEATER[] = "Attention! There is a cheater on the other team named \"{}\". Please kick them!";
		constexpr char FMT_MULTIPLE_CHEATERS[] = "Attention! There are {} cheaters on the other team named {}. Please kick them!";
		//constexpr char FMT_MANY_CHEATERS[] =     "Attention! There are %u cheaters on the other team including %s. Please kick them!";
		constexpr size_t MAX_CHATMSG_LENGTH = 127;
		constexpr size_t MAX_NAMES_LENGTH_ONE = MAX_CHATMSG_LENGTH - std::size(FMT_ONE_CHEATER) - 1 - 2;
		constexpr size_t MAX_NAMES_LENGTH_MULTIPLE = MAX_CHATMSG_LENGTH - std::size(FMT_MULTIPLE_CHEATERS) - 1 - 1 - 2;
		//constexpr size_t MAX_NAMES_LENGTH_MANY = MAX_CHATMSG_LENGTH - std::size(FMT_MANY_CHEATERS) - 1 - 1 - 2;

		static_assert(MAX_NAMES_LENGTH_ONE >= 32);
		mh::fmtstr<MAX_CHATMSG_LENGTH + 1> chatMsg;
		if (chatMsgCheaterNames.size() == 1)
		{
			chatMsg.fmt(FMT_ONE_CHEATER, chatMsgCheaterNames.front());
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

			chatMsg.fmt(FMT_MULTIPLE_CHEATERS, chatMsgCheaterNames.size(), cheaters);
		}

		assert(chatMsg.size() <= 127);

		if (now >= m_NextCheaterWarningTime)
		{
			if (m_Settings->m_AutoChatWarnings && m_ActionManager->QueueAction<ChatMessageAction>(chatMsg))
			{
				Log({ 1, 0, 0, 1 }, logMsg);
				m_NextCheaterWarningTime = now + CHEATER_WARNING_INTERVAL;
			}
		}
		else
		{
			DebugLog("HandleEnemyCheaters(): Skipping cheater warnings for "s << to_seconds(m_NextCheaterWarningTime - now) << " seconds");
		}
	}
}

void ModeratorLogic::HandleConnectingEnemyCheaters(const std::vector<Cheater>& connectingEnemyCheaters)
{
	if (!m_Settings->m_AutoChatWarnings || !m_Settings->m_AutoChatWarningsConnecting)
		return;  // user has disabled this functionality

	const auto now = tfbd_clock_t::now();
	if (now < m_NextConnectingCheaterWarningTime)
	{
		DebugLog("HandleEnemyCheaters(): Discarding connection warnings ("s
			<< to_seconds(m_NextConnectingCheaterWarningTime - now) << " seconds left)");

		// Assume someone else with a lower userid is in charge, discard warnings about
		// connecting enemy cheaters while it looks like they are doing stuff
		for (auto cheater : connectingEnemyCheaters)
			cheater->GetOrCreateData<PlayerExtraData>().m_PreWarnedOtherTeam = true;

		return;
	}

	const bool isBotLeader = IsBotLeader();
	bool needsWarning = false;
	for (auto& cheater : connectingEnemyCheaters)
	{
		auto& cheaterData = cheater->GetOrCreateData<PlayerExtraData>();
		if (cheaterData.m_PreWarnedOtherTeam)
			continue; // Already included in a warning

		if (isBotLeader)
		{
			// We're supposedly in charge
			DebugLog("We're bot leader: Triggered connecting warning for {}", cheater);
			needsWarning = true;
			break;
		}
		else if (cheaterData.m_ConnectingWarningDelayEnd.has_value())
		{
			if (now >= cheaterData.m_ConnectingWarningDelayEnd)
			{
				DebugLog("We're not bot leader: Delay expired for connecting cheater {}", cheater);
				needsWarning = true;
				break;
			}
			else
			{
				DebugLog("We're not bot leader: {} seconds remaining for connecting cheater {}",
					to_seconds(cheaterData.m_ConnectingWarningDelayEnd.value() - now), cheater);
			}
		}
		else if (!cheaterData.m_ConnectingWarningDelayEnd.has_value())
		{
			DebugLog("We're not bot leader: Starting delay for connecting cheater {}", cheater);
			cheaterData.m_ConnectingWarningDelayEnd = now + CHEATER_WARNING_DELAY;
		}
	}

	if (!needsWarning)
		return;

	mh::fmtstr<128> chatMsg;
	if (connectingEnemyCheaters.size() == 1)
	{
		chatMsg.puts("Heads up! There is a known cheater joining the other team! Name unknown until they fully join.");
	}
	else
	{
		chatMsg.fmt("Heads up! There are {} known cheaters joining the other team! Names unknown until they fully join.",
			connectingEnemyCheaters.size());
	}

	Log("Telling other team about "s << connectingEnemyCheaters.size() << " cheaters currently connecting");
	if (m_ActionManager->QueueAction<ChatMessageAction>(chatMsg))
	{
		for (auto& cheater : connectingEnemyCheaters)
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
	[[maybe_unused]] const auto dbgDeltaTime = to_seconds(tfbd_clock_t::now() - now);
	if ((tfbd_clock_t::now() - now) > 15s)
		return;

	const auto myTeam = TryGetMyTeam();
	if (!myTeam)
		return; // We don't know what team we're on, so we can't really take any actions.

	uint8_t totalEnemyPlayers = 0;
	uint8_t connectedEnemyPlayers = 0;
	uint8_t totalFriendlyPlayers = 0;
	uint8_t connectedFriendlyPlayers = 0;
	std::vector<Cheater> enemyCheaters;
	std::vector<Cheater> friendlyCheaters;
	std::vector<Cheater> connectingEnemyCheaters;

	const bool isBotLeader = IsBotLeader();
	bool needsEnemyWarning = false;
	for (IPlayer& player : m_World->GetLobbyMembers())
	{
		const bool isPlayerConnected = player.GetConnectionState() == PlayerStatusState::Active;
		const auto isCheater = m_PlayerList.HasPlayerAttributes(player, PlayerAttribute::Cheater);
		const auto teamShareResult = m_World->GetTeamShareResult(*myTeam, player);
		if (teamShareResult == TeamShareResult::SameTeams)
		{
			if (isPlayerConnected)
			{
				if (player.GetActiveTime() > m_Settings->GetAutoVotekickDelay())
					connectedFriendlyPlayers++;

				if (!!isCheater)
					friendlyCheaters.push_back({ player, isCheater });
			}

			totalFriendlyPlayers++;
		}
		else if (teamShareResult == TeamShareResult::OppositeTeams)
		{
			if (isPlayerConnected)
			{
				connectedEnemyPlayers++;

				if (isCheater && !player.GetNameSafe().empty())
					enemyCheaters.push_back({ player, isCheater });
			}
			else
			{
				if (isCheater)
					connectingEnemyCheaters.push_back({ player, isCheater });
			}

			totalEnemyPlayers++;
		}
	}

	HandleEnemyCheaters(totalEnemyPlayers, enemyCheaters, connectingEnemyCheaters);
	HandleFriendlyCheaters(totalFriendlyPlayers, connectedFriendlyPlayers, friendlyCheaters);
}

bool ModeratorLogic::SetPlayerAttribute(const IPlayer& player, PlayerAttribute attribute, AttributePersistence persistence, bool set)
{
	bool attributeChanged = false;

	m_PlayerList.ModifyPlayer(player.GetSteamID(), [&](PlayerListData& data)
		{
			PlayerAttributesList& attribs = [&]() -> PlayerAttributesList&
			{
				switch (persistence)
				{
				case AttributePersistence::Any:
					LogError("Invalid AttributePersistence {}", mh::enum_fmt(persistence));
					[[fallthrough]];
				case AttributePersistence::Saved:      return data.m_SavedAttributes;
				case AttributePersistence::Transient:  return data.m_TransientAttributes;
				}

				throw std::invalid_argument(mh::format("{}", MH_SOURCE_LOCATION_CURRENT()));
			}();

			attributeChanged = attribs.SetAttribute(attribute, set);

			if (!data.m_LastSeen)
				data.m_LastSeen.emplace();

			data.m_LastSeen->m_Time = m_World->GetCurrentTime();

			if (const auto& name = player.GetNameUnsafe(); !name.empty())
				data.m_LastSeen->m_PlayerName = name;

			return ModifyPlayerAction::Modified;
		});

	return attributeChanged;
}

std::optional<LobbyMemberTeam> ModeratorLogic::TryGetMyTeam() const
{
	return m_World->FindLobbyMemberTeam(m_Settings->GetLocalSteamID());
}

TeamShareResult ModeratorLogic::GetTeamShareResult(const SteamID& id) const
{
	return m_World->GetTeamShareResult(id);
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

ModeratorLogic::ModeratorLogic(IWorldState& world, const Settings& settings, IRCONActionManager& actionManager) :
	AutoConsoleLineListener(world),
	AutoWorldEventListener(world),
	m_World(&world),
	m_Settings(&settings),
	m_ActionManager(&actionManager),
	m_PlayerList(settings),
	m_Rules(settings)
{
}

PlayerMarks ModeratorLogic::GetPlayerAttributes(const SteamID& id) const
{
	return m_PlayerList.GetPlayerAttributes(id);
}

PlayerMarks ModeratorLogic::HasPlayerAttributes(const SteamID& id, const PlayerAttributesList& attributes, AttributePersistence persistence) const
{
	return m_PlayerList.HasPlayerAttributes(id, attributes, persistence);
}

bool ModeratorLogic::InitiateVotekick(const IPlayer& player, KickReason reason, const PlayerMarks* marks)
{
	const auto userID = player.GetUserID();
	if (!userID)
	{
		Log("Wanted to kick {}, but could not find userid", player);
		return false;
	}

	if (m_ActionManager->QueueAction<KickAction>(userID.value(), reason))
	{
		std::string logMsg = mh::format("InitiateVotekick on {}: {:v}", player, mh::enum_fmt(reason));
		if (marks)
			mh::format_to_container(logMsg, ", in playerlist(s){}", *marks);

		Log(std::move(logMsg));

		m_LastVoteCallTime = tfbd_clock_t::now();
	}

	return true;
}

duration_t VoteCooldown::GetRemainingDuration() const
{
	return m_Total - m_Elapsed;
}

float VoteCooldown::GetProgress() const
{
	if (m_Elapsed >= m_Total)
		return 1;

	return static_cast<float>(to_seconds(m_Elapsed) / to_seconds(m_Total));
}
