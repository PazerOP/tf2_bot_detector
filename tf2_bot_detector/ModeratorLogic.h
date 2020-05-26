#pragma once

#include "IConsoleLineListener.h"
#include "IWorldEventListener.h"

#include <vector>

namespace tf2_bot_detector
{
	class IPlayer;
	enum class KickReason;
	enum class PlayerAttributes;
	class SteamID;

	class ModeratorLogic final : IConsoleLineListener, BaseWorldEventListener
	{
	public:
		ModeratorLogic(WorldState& world);

		bool InitiateVotekick(const SteamID& id, KickReason reason);

		bool SetPlayerAttribute(const SteamID& id, PlayerAttributes markType, bool set = true);
		bool HasPlayerAttribute(const SteamID& id, PlayerAttributes markType) const;

	private:
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
		void OnPlayerStatusUpdate(WorldState& world, const PlayerRef& player) override;
		void OnChatMsg(WorldState& world, const PlayerRef& player, const std::string_view& msg) override;

		struct DelayedChatBan
		{
			time_point_t m_Timestamp;
			std::string m_PlayerName;
		};
		std::vector<DelayedChatBan> m_DelayedBans;
		void ProcessDelayedBans(time_point_t timestamp, const IPlayer& updatedStatus);

		time_point_t m_LastPlayerActionsUpdate{};
		void ProcessPlayerActions();
		void HandleFriendlyCheaters(uint8_t friendlyPlayerCount, const std::vector<SteamID>& friendlyCheaters);
		void HandleEnemyCheaters(uint8_t enemyPlayerCount, const std::vector<SteamID>& enemyCheaters,
			const std::vector<IPlayer*>& connectingEnemyCheaters);
	};
}
