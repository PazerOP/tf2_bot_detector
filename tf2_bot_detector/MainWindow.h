#pragma once

#include "Actions.h"
#include "ActionManager.h"
#include "Clock.h"
#include "CompensatedTS.h"
#include "ModeratorLogic.h"
#include "PlayerListJSON.h"
#include "Settings.h"
#include "WorldState.h"
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

	class MainWindow final : public ImGuiDesktop::Window, IConsoleLineListener, BaseWorldEventListener
	{
	public:
		MainWindow();
		~MainWindow();

	private:
		void OnDraw() override;
		void OnDrawMenuBar() override;
		bool HasMenuBar() const override { return true; }
		void OnDrawScoreboard();
		void OnDrawScoreboardColorPicker(const char* name_id, float color[4]);
		void OnDrawScoreboardContextMenu(const IPlayer& player);
		void OnDrawChat();
		void OnDrawAppLog();
		void OnDrawServerStats();
		void OnDrawNetGraph();

		void OnDrawSettingsPopup();
		bool m_SettingsPopupOpen = false;
		void OpenSettingsPopup() { m_SettingsPopupOpen = true; }

		void OnUpdate() override;
		size_t m_ParsedLineCount = 0;

		bool IsSleepingEnabled() const override;

		bool IsTimeEven() const;
		float TimeSine(float interval = 1.0f, float min = 0, float max = 1) const;

		// IConsoleLineListener
		void OnConsoleLineParsed(IConsoleLine& line);

		// IWorldEventListener
		//void OnChatMsg(WorldState& world, const IPlayer& player, const std::string_view& msg) override;
		void OnUpdate(WorldState& world, bool consoleLinesUpdated) override;

		bool m_Paused = false;

		struct WorldStateExtra
		{
			WorldStateExtra(MainWindow* window, const std::filesystem::path& conLogFile);

			WorldState m_WorldState;
			ModeratorLogic m_ModeratorLogic;
		};
		std::optional<WorldStateExtra> m_WorldState;
		WorldState& GetWorld() { return m_WorldState.value().m_WorldState; }
		const WorldState& GetWorld() const { return m_WorldState.value().m_WorldState; }
		ModeratorLogic& GetModLogic() { return m_WorldState.value().m_ModeratorLogic; }
		const ModeratorLogic& GetModLogic() const { return m_WorldState.value().m_ModeratorLogic; }

		// Gets the current timestamp, but time progresses in real time even without new messages
		time_point_t GetCurrentTimestampCompensated() const { return GetWorld().GetCurrentTime(); }

		size_t m_PrintingLineCount = 0;
		const IConsoleLine* m_PrintingLines[512]{}; // newest to oldest order

#if 0
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
#else
		size_t GeneratePlayerPrintData(const IPlayer** begin, const IPlayer** end) const;
#endif

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

		struct EdictUsageSample
		{
			time_point_t m_Timestamp;
			uint16_t m_UsedEdicts;
			uint16_t m_MaxEdicts;
		};
		std::vector<EdictUsageSample> m_EdictUsageSamples;

		time_point_t m_OpenTime;
		time_point_t m_LastStatusUpdateTime{};

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

		Settings m_Settings;
		ActionManager m_ActionManager;
		PeriodicActionManager m_PeriodicActionManager;
	};
}
