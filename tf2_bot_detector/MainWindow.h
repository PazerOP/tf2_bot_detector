#pragma once

#include "Actions.h"
#include "ActionManager.h"
#include "Clock.h"
#include "PlayerList.h"
#include "PlayerListJSON.h"
#include "LobbyMember.h"
#include "PeriodicActionManager.h"
#include "PlayerStatus.h"
#include "TFConstants.h"
#include "IConsoleLineListener.h"

#include <imgui_desktop/Window.h>

#include <chrono>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

struct ImVec4;

namespace tf2_bot_detector
{
	class IConsoleLine;
	class IConsoleLineListener;

	struct PlayerScores
	{
		uint16_t m_Kills = 0;
		uint16_t m_Deaths = 0;
	};

	class MainWindow final : public ImGuiDesktop::Window, IConsoleLineListener
	{
	public:
		MainWindow();
		~MainWindow();

		void AddConsoleLineListener(IConsoleLineListener* listener);
		bool RemoveConsoleLineListener(IConsoleLineListener* listener);

	private:
		void OnDraw() override;
		void OnDrawMenuBar() override;
		bool HasMenuBar() const override { return true; }
		void OnDrawScoreboard();
		void OnDrawScoreboardContextMenu(const SteamID& steamID);
		void OnDrawChat();
		void OnDrawAppLog();
		void OnDrawServerStats();
		void OnDrawNetGraph();

		void OnUpdate() override;
		size_t m_ParsedLineCount = 0;

		bool m_IsSleepingEnabled = true;
		bool IsSleepingEnabled() const override;

		bool IsTimeEven() const;
		float TimeSine(float interval = 1.0f, float min = 0, float max = 1) const;

		void OnConsoleLineParsed(IConsoleLine& line);

		struct CustomDeleters
		{
			void operator()(FILE*) const;
		};
		std::unique_ptr<FILE, CustomDeleters> m_File;
		std::string m_FileLineBuf;

		// A compensated timestamp. Allows time to appear to progress normally
		// (with all the sub-second precision offered by clock_t) despite the uneven
		// pacing of the output from the log file.
		struct CompensatedTS
		{
		public:
			bool IsRecordedValid() const { return m_Recorded.has_value(); }
			void SetRecorded(time_point_t recorded);

			void Snapshot();
			bool IsSnapshotValid() const { return m_Snapshot.has_value(); }
			time_point_t GetSnapshot() const;

		private:
			std::optional<time_point_t> m_Recorded;  // real time at instant of recording
			time_point_t m_Parsed{};                 // real time when the recorded value was read
			std::optional<time_point_t> m_Snapshot;  // compensated time when Snapshot() was called
			std::optional<time_point_t> m_PreviousSnapshot;

			mutable bool m_SnapshotUsed = false;

		} m_CurrentTimestamp;

		std::vector<std::unique_ptr<IConsoleLine>> m_ConsoleLines;
		bool m_Paused = false;

		// Gets the current timestamp, but time progresses in real time even without new messages
		time_point_t GetCurrentTimestampCompensated() const { return m_CurrentTimestamp.GetSnapshot(); }

		size_t m_PrintingLineCount = 0;
		IConsoleLine* m_PrintingLines[512]{};
		void UpdatePrintingLines();

		struct PlayerPrintData final
		{
			std::string m_Name;
			SteamID m_SteamID;
			PlayerScores m_Scores;
			duration_t m_ConnectedTime{};
			uint16_t m_UserID;
			uint16_t m_Ping;
			TFTeam m_Team;
			PlayerStatusState m_State;
		};
		size_t GeneratePlayerPrintData(PlayerPrintData* begin, PlayerPrintData* end) const;

		enum class TeamShareResult
		{
			SameTeams,
			OppositeTeams,
			Neither,
		};

		std::optional<SteamID> FindSteamIDForName(const std::string_view& playerName) const;
		std::optional<LobbyMemberTeam> FindLobbyMemberTeam(const SteamID& id) const;
		std::optional<uint16_t> FindUserID(const SteamID& id) const;
		TeamShareResult GetTeamShareResult(const SteamID& id) const;
		TeamShareResult GetTeamShareResult(const SteamID& id0, const SteamID& id1) const;
		TeamShareResult GetTeamShareResult(const std::optional<LobbyMemberTeam>& team0, const SteamID& id1) const;
		static TeamShareResult GetTeamShareResult(
			const std::optional<LobbyMemberTeam>& team0, const std::optional<LobbyMemberTeam>& team1);
		std::optional<LobbyMemberTeam> TryGetMyTeam() const;

		struct PingSample
		{
			constexpr PingSample(time_point_t timestamp, uint16_t ping) :
				m_Timestamp(timestamp), m_Ping(ping)
			{
			}

			time_point_t m_Timestamp{};
			uint16_t m_Ping{};
		};

		struct PlayerExtraData
		{
			PlayerStatus m_Status{};
			PlayerScores m_Scores{};
			TFTeam m_Team{};
			uint8_t m_ClientIndex{};
			time_point_t m_LastStatusUpdateTime{};
			time_point_t m_LastPingUpdateTime{};
			std::vector<PingSample> m_PingHistory{};
			float GetAveragePing() const;

			// If this is a known cheater, warn them ahead of time that the player is connecting, but only once
			// (we don't know the cheater's name yet, so don't spam if they can't do anything about it yet)
			bool m_PreWarnedOtherTeam = false;

			struct
			{
				time_point_t m_LastTransmission{};
				duration_t m_TotalTransmissions{};
			} m_Voice;
		};

		struct DelayedChatBan
		{
			time_point_t m_Timestamp;
			std::string m_PlayerName;
		};
		std::vector<DelayedChatBan> m_DelayedBans;
		void ProcessDelayedBans(time_point_t timestamp, const PlayerStatus& updatedStatus);

		time_point_t m_LastPlayerActionsUpdate{};
		void ProcessPlayerActions();
		void HandleFriendlyCheaters(uint8_t friendlyPlayerCount, const std::vector<SteamID>& friendlyCheaters);
		void HandleEnemyCheaters(uint8_t enemyPlayerCount, const std::vector<SteamID>& enemyCheaters,
			const std::vector<PlayerExtraData*>& connectingEnemyCheaters);

		struct EdictUsageSample
		{
			time_point_t m_Timestamp;
			uint16_t m_UsedEdicts;
			uint16_t m_MaxEdicts;
		};
		std::vector<EdictUsageSample> m_EdictUsageSamples;

		std::vector<LobbyMember> m_CurrentLobbyMembers;
		std::vector<LobbyMember> m_PendingLobbyMembers;
		std::unordered_map<SteamID, PlayerExtraData> m_CurrentPlayerData;
		time_point_t m_OpenTime;
		PlayerListJSON m_PlayerList;
		time_point_t m_LastStatusUpdateTime{};
		time_point_t m_LastCheaterWarningTime{};

		void UpdateServerPing(time_point_t timestamp);
		std::vector<PingSample> m_ServerPingSamples;
		time_point_t m_LastServerPingSample{};

		struct AvgSample
		{
			float m_AvgValue{};
			uint32_t m_SampleCount{};

			void AddSample(float value);
		};

		struct NetSamples
		{
			std::map<time_point_t, AvgSample> m_Latency;
			std::map<time_point_t, AvgSample> m_Loss;
			std::map<time_point_t, AvgSample> m_Packets;
			std::map<time_point_t, AvgSample> m_Data;
		};
		NetSamples m_NetSamplesOut;
		NetSamples m_NetSamplesIn;
		std::pair<time_point_t, time_point_t> GetNetSamplesRange() const;
		void PruneNetSamples(time_point_t& startTime, time_point_t& endTime);
		static constexpr duration_t NET_GRAPH_DURATION = std::chrono::seconds(30);

		void PlotNetSamples(const char* label_id, const std::map<time_point_t, AvgSample>& data,
			time_point_t startTime, time_point_t endTime, int yAxis = 0) const;
		static float GetMaxValue(const std::map<time_point_t, AvgSample>& data);

		bool SetPlayerAttribute(const SteamID& id, PlayerAttributes markType, bool set = true);
		bool HasPlayerAttribute(const SteamID& id, PlayerAttributes markType) const;

		void InitiateVotekick(const SteamID& id, KickReason reason);

		ActionManager m_ActionManager;
		PeriodicActionManager m_PeriodicActionManager;
		std::set<IConsoleLineListener*> m_ConsoleLineListeners;
	};
}
