#pragma once

#include "LobbyMember.h"

#include <imgui_desktop/Window.h>

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace tf2_bot_detector
{
	class IConsoleLine;

	class MainWindow final : public ImGuiDesktop::Window
	{
	public:
		MainWindow();
		~MainWindow();

	private:
		void OnDraw() override;
		void OnUpdate() override;

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

		std::vector<LobbyMember> m_CurrentLobbyMembers;
		std::chrono::steady_clock::time_point m_OpenTime;
	};
}
