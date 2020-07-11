#pragma once

#include "CompensatedTS.h"
#include "ConsoleLog/IConsoleLineListener.h"
#include "IPlayer.h"
#include "IWorldEventListener.h"
#include "LobbyMember.h"
#include "PlayerStatus.h"
#include "Networking/SteamAPI.h"

#include <cppcoro/generator.hpp>

#include <any>
#include <filesystem>
#include <typeindex>
#include <unordered_set>

namespace tf2_bot_detector
{
	class ChatConsoleLine;
	class ConsoleLogParser;
	enum class LobbyMemberTeam : uint8_t;
	class Settings;

	enum class TeamShareResult
	{
		SameTeams,
		OppositeTeams,
		Neither,
	};

	class WorldState;

	class WorldStateConLog
	{
	public:
		void AddConsoleLineListener(IConsoleLineListener* listener);
		void RemoveConsoleLineListener(IConsoleLineListener* listener);

		void AddConsoleOutputChunk(const std::string_view& chunk);
		void AddConsoleOutputLine(const std::string_view& line);

	private:
		WorldState& GetWorldState();

		std::unordered_set<IConsoleLineListener*> m_ConsoleLineListeners;
		friend class ConsoleLogParser;
	};

	class WorldState : IConsoleLineListener, public WorldStateConLog
	{
	public:
		WorldState(const Settings& settings);
		~WorldState();

		time_point_t GetCurrentTime() const { return m_CurrentTimestamp.GetSnapshot(); }
		void Update();
		void UpdateTimestamp(const ConsoleLogParser& parser);

		void AddWorldEventListener(IWorldEventListener* listener);
		void RemoveWorldEventListener(IWorldEventListener* listener);

		std::optional<SteamID> FindSteamIDForName(const std::string_view& playerName) const;
		std::optional<LobbyMemberTeam> FindLobbyMemberTeam(const SteamID& id) const;
		std::optional<UserID_t> FindUserID(const SteamID& id) const;
		TeamShareResult GetTeamShareResult(const SteamID& id0, const SteamID& id1) const;
		TeamShareResult GetTeamShareResult(const std::optional<LobbyMemberTeam>& team0, const SteamID& id1) const;
		static TeamShareResult GetTeamShareResult(
			const std::optional<LobbyMemberTeam>& team0, const std::optional<LobbyMemberTeam>& team1);

		IPlayer* FindPlayer(const SteamID& id);
		const IPlayer* FindPlayer(const SteamID& id) const;

		size_t GetApproxLobbyMemberCount() const;

		cppcoro::generator<const IPlayer&> GetLobbyMembers() const;
		cppcoro::generator<IPlayer&> GetLobbyMembers();
		cppcoro::generator<const IPlayer&> GetPlayers() const;
		cppcoro::generator<IPlayer&> GetPlayers();

		time_point_t GetLastStatusUpdateTime() const { return m_LastStatusUpdateTime; }

		// Have we joined a team and picked a class?
		bool IsLocalPlayerInitialized() const { return m_IsLocalPlayerInitialized; }
		bool IsVoteInProgress() const { return m_IsVoteInProgress; }

	private:
		const Settings* m_Settings = nullptr;

		CompensatedTS m_CurrentTimestamp;

		void OnConsoleLineParsed(WorldState& world, IConsoleLine& parsed) override;

		struct PlayerExtraData final : IPlayer
		{
			PlayerExtraData(WorldState& world, SteamID id);

			using IPlayer::GetWorld;
			const WorldState& GetWorld() const override { return *m_World; }
			const LobbyMember* GetLobbyMember() const override;
			std::string_view GetNameUnsafe() const override { return m_Status.m_Name; }
			std::string_view GetNameSafe() const override { return m_PlayerNameSafe; }
			SteamID GetSteamID() const override { return m_Status.m_SteamID; }
			PlayerStatusState GetConnectionState() const override { return m_Status.m_State; }
			std::optional<UserID_t> GetUserID() const override;
			TFTeam GetTeam() const override { return m_Team; }
			time_point_t GetConnectionTime() const override { return m_Status.m_ConnectionTime; }
			duration_t GetConnectedTime() const override;
			const PlayerScores& GetScores() const override { return m_Scores; }
			uint16_t GetPing() const override { return m_Status.m_Ping; }
			time_point_t GetLastStatusUpdateTime() const override { return m_LastStatusUpdateTime; }
			const SteamAPI::PlayerSummary* GetPlayerSummary() const override { return m_PlayerSummary ? &*m_PlayerSummary : nullptr; }

			WorldState* m_World{};
			PlayerScores m_Scores{};
			TFTeam m_Team{};

			uint8_t m_ClientIndex{};
			std::optional<SteamAPI::PlayerSummary> m_PlayerSummary;

			void SetStatus(PlayerStatus status, time_point_t timestamp);
			const PlayerStatus& GetStatus() const { return m_Status; }

			void SetPing(uint16_t ping, time_point_t timestamp);

		protected:
			std::map<std::type_index, std::any> m_UserData;
			const std::any* FindDataStorage(const std::type_index& type) const override;
			std::any& GetOrCreateDataStorage(const std::type_index& type) override;

		private:
			PlayerStatus m_Status{};
			std::string m_PlayerNameSafe;

			time_point_t m_LastStatusUpdateTime{};
			time_point_t m_LastPingUpdateTime{};
		};

		PlayerExtraData& FindOrCreatePlayer(const SteamID& id);

		std::list<std::future<std::vector<SteamAPI::PlayerSummary>>> m_PlayerSummaryRequests;
		void QueuePlayerSummaryUpdate();
		void ApplyPlayerSummaries();

		std::vector<LobbyMember> m_CurrentLobbyMembers;
		std::vector<LobbyMember> m_PendingLobbyMembers;
		std::unordered_map<SteamID, PlayerExtraData> m_CurrentPlayerData;
		bool m_IsLocalPlayerInitialized = false;
		bool m_IsVoteInProgress = false;

		time_point_t m_LastStatusUpdateTime{};

		struct EventBroadcaster final : IWorldEventListener
		{
			void OnTimestampUpdate(WorldState& world) override;
			void OnPlayerStatusUpdate(WorldState& world, const IPlayer& player) override;
			void OnChatMsg(WorldState& world, IPlayer& player, const std::string_view& msg) override;

			std::unordered_set<IWorldEventListener*> m_EventListeners;
		} m_EventBroadcaster;
	};
}
