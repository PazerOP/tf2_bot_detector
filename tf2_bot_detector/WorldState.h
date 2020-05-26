#pragma once

#include "CompensatedTS.h"
#include "IConsoleLineListener.h"
#include "IWorldEventListener.h"
#include "LobbyMember.h"
#include "PlayerStatus.h"

#include <any>
#include <filesystem>
#include <typeindex>
#include <unordered_set>

namespace tf2_bot_detector
{
	enum class LobbyMemberTeam : uint8_t;

	enum class TeamShareResult
	{
		SameTeams,
		OppositeTeams,
		Neither,
	};

	struct PlayerScores
	{
		uint16_t m_Kills = 0;
		uint16_t m_Deaths = 0;
	};

	class WorldState : IConsoleLineListener
	{
	public:
		WorldState(const std::filesystem::path& conLogFile);

		void Update();
		time_point_t GetCurrentTime() const { return m_CurrentTimestamp.GetSnapshot(); }
		size_t GetParsedLineCount() const { return m_ParsedLineCount; }

		void AddWorldEventListener(IWorldEventListener* listener);
		void RemoveWorldEventListener(IWorldEventListener* listener);

		void AddConsoleLineListener(IConsoleLineListener* listener);
		void RemoveConsoleLineListener(IConsoleLineListener* listener);

		std::optional<SteamID> FindSteamIDForName(const std::string_view& playerName) const;
		std::optional<LobbyMemberTeam> FindLobbyMemberTeam(const SteamID& id) const;
		std::optional<uint16_t> FindUserID(const SteamID& id) const;
		TeamShareResult GetTeamShareResult(const SteamID& id0, const SteamID& id1) const;
		TeamShareResult GetTeamShareResult(const std::optional<LobbyMemberTeam>& team0, const SteamID& id1) const;
		static TeamShareResult GetTeamShareResult(
			const std::optional<LobbyMemberTeam>& team0, const std::optional<LobbyMemberTeam>& team1);

		template<typename T> T* GetPlayerData(const SteamID& id)
		{
			return const_cast<T*>(std::as_const(*this).GetPlayerData<T>(id));
		}
		template<typename T> const T* GetPlayerData(const SteamID& id) const
		{
			if (auto foundPlayer = m_CurrentPlayerData.find(id); foundPlayer != m_CurrentPlayerData.end())
			{
				if (auto foundData = foundPlayer->second.m_UserData.find(typeid(T)); foundData != foundPlayer->second.m_UserData.end())
					return &std::any_cast<T>(foundData->second);
			}

			return nullptr;
		}
		template<typename T> bool SetPlayerData(const SteamID& id, T&& value)
		{
			if (auto foundPlayer = m_CurrentPlayerData.find(id); foundPlayer != m_CurrentPlayerData.end())
				foundPlayer->second.m_UserData[typeid(T)] = std::move(value);
		}

	private:
		struct CustomDeleters
		{
			void operator()(FILE*) const;
		};
		std::filesystem::path m_FileName;
		std::unique_ptr<FILE, CustomDeleters> m_File;
		std::string m_FileLineBuf;
		size_t m_ParsedLineCount = 0;
		std::vector<std::unique_ptr<IConsoleLine>> m_ConsoleLines;
		std::unordered_set<IConsoleLineListener*> m_ConsoleLineListeners;

		void OnConsoleLineParsed(IConsoleLine& parsed);

		CompensatedTS m_CurrentTimestamp;

		struct PlayerExtraData final
		{
			PlayerStatus m_Status{};
			PlayerScores m_Scores{};
			TFTeam m_Team{};

			// uint8_t m_ClientIndex{};
			time_point_t m_LastStatusUpdateTime{};
			time_point_t m_LastPingUpdateTime{};
			std::map<std::type_index, std::any> m_UserData;
		};

		std::vector<LobbyMember> m_CurrentLobbyMembers;
		std::vector<LobbyMember> m_PendingLobbyMembers;
		std::unordered_map<SteamID, PlayerExtraData> m_CurrentPlayerData;

		struct EventBroadcaster final : IWorldEventListener
		{
			void OnUpdate(WorldState& world, bool consoleLinesUpdated) override;
			void OnTimestampUpdate(WorldState& world) override;
			void OnPlayerStatusUpdate(WorldState& world, const PlayerRef& player) override;
			void OnChatMsg(WorldState& world, const PlayerRef& player, const std::string_view& msg) override;

			std::unordered_set<IWorldEventListener*> m_EventListeners;
		} m_EventBroadcaster;
	};
}
