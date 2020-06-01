#pragma once

#include "IConsoleLineListener.h"
#include "IWorldEventListener.h"
#include "Config/PlayerListJSON.h"
#include "Config/Rules.h"

#include <optional>
#include <vector>

namespace tf2_bot_detector
{
	class ActionManager;
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
		ModeratorLogic(WorldState& world, const Settings& settings, ActionManager& actionManager);

		bool InitiateVotekick(const IPlayer& id, KickReason reason);

		bool SetPlayerAttribute(const IPlayer& id, PlayerAttributes markType, bool set = true);
		bool HasPlayerAttribute(const SteamID& id, PlayerAttributes markType) const;

		std::optional<LobbyMemberTeam> TryGetMyTeam() const;
		TeamShareResult GetTeamShareResult(const SteamID& id) const;
		const IPlayer* GetLocalPlayer() const;

	private:
		WorldState* m_World = nullptr;
		const Settings* m_Settings = nullptr;
		ActionManager* m_ActionManager = nullptr;

		struct PlayerExtraData
		{
			// If this is a known cheater, warn them ahead of time that the player is connecting, but only once
			// (we don't know the cheater's name yet, so don't spam if they can't do anything about it yet)
			bool m_PreWarnedOtherTeam = false;

			struct
			{
				time_point_t m_LastTransmission{};
				duration_t m_TotalTransmissions{};
			} m_Voice;
		};

		void OnUpdate(WorldState& world, bool consoleLinesUpdated) override;
		void OnPlayerStatusUpdate(WorldState& world, const IPlayer& player) override;
		void OnChatMsg(WorldState& world, const IPlayer& player, const std::string_view& msg) override;

		void OnRuleMatch(const ModerationRule& rule, const IPlayer& player);

		struct DelayedChatBan
		{
			time_point_t m_Timestamp;
			std::string m_PlayerName;
		};
		std::vector<DelayedChatBan> m_DelayedBans;
		void ProcessDelayedBans(time_point_t timestamp, const IPlayer& updatedStatus);

		static constexpr duration_t CHEATER_WARNING_INTERVAL = std::chrono::seconds(10);
		static constexpr duration_t CHEATER_WARNING_INTERVAL_NONLOCAL = std::chrono::seconds(20);

		time_point_t m_NextConnectingCheaterWarningTime{};
		time_point_t m_NextCheaterWarningTime{};
		time_point_t m_LastPlayerActionsUpdate{};
		void ProcessPlayerActions();
		void HandleFriendlyCheaters(uint8_t friendlyPlayerCount, const std::vector<const IPlayer*>& friendlyCheaters);
		void HandleEnemyCheaters(uint8_t enemyPlayerCount, const std::vector<const IPlayer*>& enemyCheaters,
			const std::vector<IPlayer*>& connectingEnemyCheaters);

		ModerationRules m_Rules;
		PlayerListJSON m_PlayerList;
	};
}
