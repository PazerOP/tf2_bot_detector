#pragma once

#include "Actions/Actions.h"
#include "Actions/HijackActionManager.h"
#include "Actions/RCONActionManager.h"
#include "Clock.h"
#include "CompensatedTS.h"
#include "ConsoleLog/ConsoleLogParser.h"
#include "Config/PlayerListJSON.h"
#include "Config/Settings.h"
#include "Config/SponsorsList.h"
#include "Networking/GithubAPI.h"
#include "ModeratorLogic.h"
#include "SetupFlow/SetupFlow.h"
#include "WorldState.h"
#include "LobbyMember.h"
#include "PlayerStatus.h"
#include "TFConstants.h"
#include "ConsoleLog/IConsoleLineListener.h"

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
		void OnDrawScoreboardContextMenu(IPlayer& player);
		void OnDrawChat();
		void OnDrawAppLog();
		void OnDrawServerStats();

		void OnDrawSettingsPopup();
		bool m_SettingsPopupOpen = false;
		void OpenSettingsPopup() { m_SettingsPopupOpen = true; }

		void OnDrawUpdateCheckPopup();
		bool m_UpdateCheckPopupOpen = false;
		void OpenUpdateCheckPopup();

		void OnDrawUpdateAvailablePopup();
		bool m_UpdateAvailablePopupOpen = false;
		void OpenUpdateAvailablePopup();

		void OnDrawAboutPopup();
		bool m_AboutPopupOpen = false;
		void OpenAboutPopup() { m_AboutPopupOpen = true; }

		void GenerateDebugReport();

		GithubAPI::NewVersionResult* GetUpdateInfo();
		std::shared_future<GithubAPI::NewVersionResult> m_UpdateInfo;
		bool m_NotifyOnUpdateAvailable = true;
		void HandleUpdateCheck();

		void OnUpdate() override;

		bool IsSleepingEnabled() const override;

		bool IsTimeEven() const;
		float TimeSine(float interval = 1.0f, float min = 0, float max = 1) const;

		// IConsoleLineListener
		void OnConsoleLineParsed(WorldState& world, IConsoleLine& line) override;
		void OnConsoleLogChunkParsed(WorldState& world, bool consoleLinesParsed) override;

		// IWorldEventListener
		//void OnChatMsg(WorldState& world, const IPlayer& player, const std::string_view& msg) override;
		//void OnUpdate(WorldState& world, bool consoleLinesUpdated) override;

		bool m_Paused = false;

		WorldState& GetWorld() { return m_WorldState; }
		const WorldState& GetWorld() const { return m_WorldState; }
		ModeratorLogic& GetModLogic() { return m_ModeratorLogic; }
		const ModeratorLogic& GetModLogic() const { return m_ModeratorLogic; }

		// Gets the current timestamp, but time progresses in real time even without new messages
		time_point_t GetCurrentTimestampCompensated() const;

		struct PingSample
		{
			constexpr PingSample(time_point_t timestamp, uint16_t ping) :
				m_Timestamp(timestamp), m_Ping(ping)
			{
			}

			time_point_t m_Timestamp{};
			uint16_t m_Ping{};
		};

		struct PlayerExtraData final
		{
			PlayerExtraData(const IPlayer& player) : m_Parent(&player) {}

			const IPlayer* m_Parent = nullptr;

			time_point_t m_LastPingUpdateTime{};
			std::vector<PingSample> m_PingHistory{};
			float GetAveragePing() const;
		};

		struct EdictUsageSample
		{
			time_point_t m_Timestamp;
			uint16_t m_UsedEdicts;
			uint16_t m_MaxEdicts;
		};
		std::vector<EdictUsageSample> m_EdictUsageSamples;

		time_point_t m_OpenTime;

		void UpdateServerPing(time_point_t timestamp);
		std::vector<PingSample> m_ServerPingSamples;
		time_point_t m_LastServerPingSample{};

		Settings m_Settings;
		WorldState m_WorldState{ m_Settings };
#ifdef _WIN32
		HijackActionManager m_HijackActionManager{ m_Settings };
#endif
		RCONActionManager m_ActionManager{ m_Settings, m_WorldState };
		ModeratorLogic m_ModeratorLogic{ m_WorldState, m_Settings, m_ActionManager };
		SetupFlow m_SetupFlow;
		SponsorsList m_SponsorsList{ m_Settings };

		time_point_t GetLastStatusUpdateTime() const;

		struct ConsoleLogParserExtra
		{
			ConsoleLogParserExtra(MainWindow& parent);
			MainWindow* m_Parent = nullptr;
			ConsoleLogParser m_Parser;

			std::list<std::shared_ptr<const IConsoleLine>> m_PrintingLines;  // newest to oldest order
			static constexpr size_t MAX_PRINTING_LINES = 512;
			cppcoro::generator<IPlayer&> GeneratePlayerPrintData();
		};
		std::optional<ConsoleLogParserExtra> m_ConsoleLogParser;
	};
}
