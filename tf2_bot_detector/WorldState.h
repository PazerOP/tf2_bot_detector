#pragma once

#include "CompensatedTS.h"
#include "FileMods.h"
#include "IConsoleLineListener.h"
#include "IPlayer.h"
#include "IWorldEventListener.h"
#include "LobbyMember.h"
#include "PlayerStatus.h"

#include <mh/coroutine/generator.hpp>

#include <any>
#include <experimental/coroutine>
#include <experimental/generator>
#include <filesystem>
#include <typeindex>
#include <unordered_set>

namespace tf2_bot_detector
{
	class ChatConsoleLine;
	enum class LobbyMemberTeam : uint8_t;
	class Settings;

	enum class TeamShareResult
	{
		SameTeams,
		OppositeTeams,
		Neither,
	};

	class WorldState : IConsoleLineListener
	{
	public:
		WorldState(const Settings& settings, const std::filesystem::path& conLogFile);

		void Update();
		time_point_t GetCurrentTime() const { return m_CurrentTimestamp.GetSnapshot(); }
		size_t GetParsedLineCount() const { return m_ParsedLineCount; }
		float GetParseProgress() const { return m_ParseProgress; }

		void AddWorldEventListener(IWorldEventListener* listener);
		void RemoveWorldEventListener(IWorldEventListener* listener);

		void AddConsoleLineListener(IConsoleLineListener* listener);
		void RemoveConsoleLineListener(IConsoleLineListener* listener);

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

		mh::generator<const IPlayer*> GetLobbyMembers() const;
		mh::generator<IPlayer*> GetLobbyMembers();
		mh::generator<const IPlayer*> GetPlayers() const;
		mh::generator<IPlayer*> GetPlayers();

	private:
		const Settings* m_Settings = nullptr;

		struct CustomDeleters
		{
			void operator()(FILE*) const;
		};
		std::filesystem::path m_FileName;
		std::unique_ptr<FILE, CustomDeleters> m_File;
		std::string m_FileLineBuf;
		size_t m_ParsedLineCount = 0;
		float m_ParseProgress = 0;
		std::vector<std::unique_ptr<IConsoleLine>> m_ConsoleLines;
		std::unordered_set<IConsoleLineListener*> m_ConsoleLineListeners;

		using striter = std::string::const_iterator;
		void Parse(bool& linesProcessed, bool& snapshotUpdated, bool& consoleLinesUpdated);
		void ParseChunk(striter& parseEnd, bool& linesProcessed, bool& snapshotUpdated, bool& consoleLinesUpdated);

		ChatWrappers m_ChatMsgWrappers;
		bool ParseChatMessage(const std::string_view& lineStr, striter& parseEnd, std::unique_ptr<IConsoleLine>& parsed);

		enum class ParseLineResult
		{
			Unparsed,
			Defer,
			Success,
			Modified,
		};

		void OnConsoleLineParsed(WorldState& world, IConsoleLine& parsed) override;

		void TrySnapshot(bool& snapshotUpdated);
		CompensatedTS m_CurrentTimestamp;

		struct PlayerExtraData final : IPlayer
		{
			PlayerExtraData(WorldState& world) : m_World(&world) {}

			using IPlayer::GetWorld;
			const WorldState& GetWorld() const override { return *m_World; }
			const LobbyMember* GetLobbyMember() const override;
			std::string_view GetName() const override { return m_Status.m_Name; }
			SteamID GetSteamID() const override { return m_Status.m_SteamID; }
			PlayerStatusState GetConnectionState() const override { return m_Status.m_State; }
			std::optional<UserID_t> GetUserID() const override;
			TFTeam GetTeam() const override { return m_Team; }
			time_point_t GetConnectionTime() const override { return m_Status.m_ConnectionTime; }
			duration_t GetConnectedTime() const override;
			const PlayerScores& GetScores() const override { return m_Scores; }
			uint16_t GetPing() const override { return m_Status.m_Ping; }
			time_point_t GetLastStatusUpdateTime() const override { return m_LastStatusUpdateTime; }

			WorldState* m_World{};
			PlayerStatus m_Status{};
			PlayerScores m_Scores{};
			TFTeam m_Team{};

			uint8_t m_ClientIndex{};
			time_point_t m_LastStatusUpdateTime{};
			time_point_t m_LastPingUpdateTime{};

		protected:
			std::map<std::type_index, std::any> m_UserData;
			const std::any* FindDataStorage(const std::type_index& type) const override;
			std::any& GetOrCreateDataStorage(const std::type_index& type) override;
		};

		PlayerExtraData& FindOrCreatePlayer(const SteamID& id);

		std::vector<LobbyMember> m_CurrentLobbyMembers;
		std::vector<LobbyMember> m_PendingLobbyMembers;
		std::unordered_map<SteamID, PlayerExtraData> m_CurrentPlayerData;

		time_point_t m_LastStatusUpdateTime{};

		struct EventBroadcaster final : IWorldEventListener
		{
			void OnUpdate(WorldState& world, bool consoleLinesUpdated) override;
			void OnTimestampUpdate(WorldState& world) override;
			void OnPlayerStatusUpdate(WorldState& world, const IPlayer& player) override;
			void OnChatMsg(WorldState& world, IPlayer& player, const std::string_view& msg) override;

			std::unordered_set<IWorldEventListener*> m_EventListeners;
		} m_EventBroadcaster;
	};
}
