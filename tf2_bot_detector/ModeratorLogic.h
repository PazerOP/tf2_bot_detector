#pragma once

#include "ConsoleLog/IConsoleLineListener.h"
#include "IWorldEventListener.h"
#include "Config/PlayerListJSON.h"
#include "Config/Rules.h"

#include <optional>
#include <unordered_set>
#include <vector>

namespace tf2_bot_detector
{
	class RCONActionManager;
	enum class LobbyMemberTeam : uint8_t;
	class IPlayer;
	enum class KickReason;
	struct ModerationRule;
	enum class PlayerAttributes;
	class Settings;
	class SteamID;
	enum class TeamShareResult;

	class ModeratorLogic final : IConsoleLineListener, BaseWorldEventListener
	{
	public:
		ModeratorLogic(WorldState& world, const Settings& settings, RCONActionManager& actionManager);

		void Update();

		bool InitiateVotekick(const IPlayer& id, KickReason reason);

		bool SetPlayerAttribute(const IPlayer& id, PlayerAttributes markType, bool set = true);
		bool HasPlayerAttribute(const SteamID& id, PlayerAttributes markType) const;

		std::optional<LobbyMemberTeam> TryGetMyTeam() const;
		TeamShareResult GetTeamShareResult(const SteamID& id) const;
		const IPlayer* GetLocalPlayer() const;

		// Are we the leader of the bots? AKA do we have the lowest userID of
		// the pool of players we think are running the bot?
		bool IsBotLeader() const;
		const IPlayer* GetBotLeader() const;

		duration_t TimeToConnectingCheaterWarning() const;
		duration_t TimeToCheaterWarning() const;

		bool IsUserRunningTool(const SteamID& id) const;
		void SetUserRunningTool(const SteamID& id, bool isRunningTool = true);

		size_t GetBlacklistedPlayerCount() const { return m_PlayerList.GetPlayerCount(); }
		size_t GetRuleCount() const { return m_Rules.GetRuleCount(); }

		void ReloadConfigFiles();

	private:
		WorldState* m_World = nullptr;
		const Settings* m_Settings = nullptr;
		RCONActionManager* m_ActionManager = nullptr;

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

		void OnPlayerStatusUpdate(WorldState& world, const IPlayer& player) override;
		void OnChatMsg(WorldState& world, IPlayer& player, const std::string_view& msg) override;

		void OnRuleMatch(const ModerationRule& rule, const IPlayer& player);

		struct DelayedChatBan
		{
			time_point_t m_Timestamp;
			std::string m_PlayerName;
		};
		std::vector<DelayedChatBan> m_DelayedBans;
		void ProcessDelayedBans(time_point_t timestamp, const IPlayer& updatedStatus);

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
			const std::vector<const IPlayer*>& friendlyCheaters);
		void HandleEnemyCheaters(uint8_t enemyPlayerCount, const std::vector<IPlayer*>& enemyCheaters,
			const std::vector<IPlayer*>& connectingEnemyCheaters);
		void HandleConnectedEnemyCheaters(const std::vector<IPlayer*>& enemyCheaters);
		void HandleConnectingEnemyCheaters(const std::vector<IPlayer*>& connectingEnemyCheaters);

		PlayerListJSON m_PlayerList;
		ModerationRules m_Rules;
	};
}
