#pragma once

#include "PlayerList.h"
#include "LobbyMember.h"
#include "PlayerStatus.h"
#include "TFConstants.h"

#include <imgui_desktop/Window.h>

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace tf2_bot_detector
{
	class IConsoleLine;

	struct PlayerScores
	{
		uint16_t m_Kills = 0;
		uint16_t m_Deaths = 0;
	};

	class MainWindow final : public ImGuiDesktop::Window
	{
	public:
		MainWindow();
		~MainWindow();

	private:
		void OnDraw() override;
		void OnDrawScoreboard();
		void OnDrawChat();

		void OnUpdate() override;
		size_t m_ParsedLineCount = 0;

		bool IsTimeEven() const;
		float TimeSine(float interval = 1.0f, float min = 0, float max = 1) const;

		void OnConsoleLineParsed(IConsoleLine* line);

		struct CustomDeleters
		{
			void operator()(FILE*) const;
		};
		std::unique_ptr<FILE, CustomDeleters> m_File;
		std::string m_FileLineBuf;
		std::optional<std::tm> m_CurrentTimestamp;
		std::vector<std::unique_ptr<IConsoleLine>> m_ConsoleLines;

		size_t m_PrintingLineCount = 0;
		IConsoleLine* m_PrintingLines[512]{};
		void UpdatePrintingLines();

		struct PlayerPrintData final
		{
			SteamID m_SteamID;
			std::string m_Name;
			PlayerScores m_Scores;
			uint16_t m_UserID;
			uint16_t m_Ping;
			TFTeam m_Team;
		};
		size_t GeneratePlayerPrintData(PlayerPrintData* begin, PlayerPrintData* end) const;

		std::optional<SteamID> FindSteamIDForName(const std::string_view& playerName) const;

		struct PlayerExtraData
		{
			PlayerStatus m_Status{};
			PlayerScores m_Scores{};
			TFTeam m_Team{};
		};

		std::vector<LobbyMember> m_CurrentLobbyMembers;
		std::vector<LobbyMember> m_PendingLobbyMembers;
		std::unordered_map<SteamID, PlayerExtraData> m_CurrentPlayerData;
		using clock_t = std::chrono::steady_clock;
		clock_t::time_point m_OpenTime;
		PlayerList m_CheaterList;
		PlayerList m_SuspiciousList;
		PlayerList m_ExploiterList;
	};
}
