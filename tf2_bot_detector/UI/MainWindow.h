#pragma once

#include "Actions/Actions.h"
#include "Actions/RCONActionManager.h"
#include "Clock.h"
#include "CompensatedTS.h"
#include "ConsoleLog/ConsoleLineListener.h"
#include "ConsoleLog/ConsoleLogParser.h"
#include "Config/PlayerListJSON.h"
#include "Config/Settings.h"
#include "Config/SponsorsList.h"
#include "DiscordRichPresence.h"
#include "Networking/GithubAPI.h"
#include "ModeratorLogic.h"
#include "SetupFlow/SetupFlow.h"
#include "WorldEventListener.h"
#include "WorldState.h"
#include "LobbyMember.h"
#include "PlayerStatus.h"
#include "TFConstants.h"

#include <imgui_desktop/Window.h>
#include <mh/error/expected.hpp>

#include <optional>
#include <vector>

struct ImFont;
struct ImVec4;

namespace tf2_bot_detector
{
	class IBaseTextures;
	class IConsoleLine;
	class IConsoleLineListener;
	class ITexture;
	class ITextureManager;
	class IUpdateManager;
	class SettingsWindow;

	class MainWindow final : public ImGuiDesktop::Window, IConsoleLineListener, BaseWorldEventListener
	{
		using Super = ImGuiDesktop::Window;

	public:
		explicit MainWindow(ImGuiDesktop::Application& app);
		~MainWindow();

		ImFont* GetFontPointer(Font f) const;

		void DrawPlayerTooltip(IPlayer& player);
		void DrawPlayerTooltip(IPlayer& player, TeamShareResult teamShareResult, const PlayerMarks& playerAttribs);

	private:
		void OnImGuiInit() override;
		void OnOpenGLInit() override;
		void OnDraw() override;
		void OnEndFrame() override;
		void OnDrawMenuBar() override;
		bool HasMenuBar() const override { return true; }
		void OnDrawScoreboard();
		void OnDrawAllPanesDisabled();
		void OnDrawScoreboardContextMenu(IPlayer& player);
		void OnDrawScoreboardRow(IPlayer& player);
		void OnDrawColorPicker(const char* name_id, std::array<float, 4>& color);
		void OnDrawChat();
		void OnDrawServerStats();
		void DrawPlayerTooltipBody(IPlayer& player, TeamShareResult teamShareResult, const PlayerMarks& playerAttribs);

		struct ColorPicker
		{
			const char* m_Name;
			std::array<float, 4>& m_Color;
		};
		void OnDrawColorPickers(const char* id, const std::initializer_list<ColorPicker>& pickers);

		void OnDrawAppLog();
		const void* m_LastLogMessage = nullptr;

		void OpenSettingsPopup();

		void OnDrawUpdateCheckPopup();
		bool m_UpdateCheckPopupOpen = false;
		void OpenUpdateCheckPopup();

		void OnDrawAboutPopup();
		bool m_AboutPopupOpen = false;
		void OpenAboutPopup() { m_AboutPopupOpen = true; }

		void PrintDebugInfo();
		void GenerateDebugReport();

		void OnUpdate() override;

		bool IsSleepingEnabled() const override;

		bool IsTimeEven() const;
		float TimeSine(float interval = 1.0f, float min = 0, float max = 1) const;

		void SetupFonts();
		ImFont* m_ProggyTiny10Font{};
		ImFont* m_ProggyTiny20Font{};
		ImFont* m_ProggyClean26Font{};

		// IConsoleLineListener
		void OnConsoleLineParsed(IWorldState& world, IConsoleLine& line) override;
		void OnConsoleLineUnparsed(IWorldState& world, const std::string_view& text) override;
		void OnConsoleLogChunkParsed(IWorldState& world, bool consoleLinesParsed) override;
		size_t m_ParsedLineCount = 0;

		// IWorldEventListener
		//void OnChatMsg(WorldState& world, const IPlayer& player, const std::string_view& msg) override;
		//void OnUpdate(WorldState& world, bool consoleLinesUpdated) override;

		bool m_Paused = false;

		// Gets the current timestamp, but time progresses in real time even without new messages
		time_point_t GetCurrentTimestampCompensated() const;

		mh::expected<std::shared_ptr<ITexture>, std::error_condition> TryGetAvatarTexture(IPlayer& player);
		std::shared_ptr<ITextureManager> m_TextureManager;
		std::unique_ptr<IBaseTextures> m_BaseTextures;

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
		std::unique_ptr<SettingsWindow> m_SettingsWindow;

		std::unique_ptr<IUpdateManager> m_UpdateManager;

		SetupFlow m_SetupFlow;

		std::shared_ptr<IWorldState> m_WorldState;
		std::unique_ptr<IRCONActionManager> m_ActionManager;

		IWorldState& GetWorld() { return *m_WorldState; }
		const IWorldState& GetWorld() const { return *m_WorldState; }
		IRCONActionManager& GetActionManager() { return *m_ActionManager; }
		const IRCONActionManager& GetActionManager() const { return *m_ActionManager; }

		struct PostSetupFlowState
		{
			PostSetupFlowState(MainWindow& window);

			MainWindow* m_Parent = nullptr;
			std::unique_ptr<IModeratorLogic> m_ModeratorLogic;
			SponsorsList m_SponsorsList;

			ConsoleLogParser m_Parser;
			std::list<std::shared_ptr<const IConsoleLine>> m_PrintingLines;  // newest to oldest order
			static constexpr size_t MAX_PRINTING_LINES = 512;
			mh::generator<IPlayer&> GeneratePlayerPrintData();

			void OnUpdateDiscord();
#ifdef TF2BD_ENABLE_DISCORD_INTEGRATION
			std::unique_ptr<IDRPManager> m_DRPManager;
#endif
		};
		std::optional<PostSetupFlowState> m_MainState;

		IModeratorLogic& GetModLogic() { return *m_MainState.value().m_ModeratorLogic; }
		const IModeratorLogic& GetModLogic() const { return *m_MainState.value().m_ModeratorLogic; }
		SponsorsList& GetSponsorsList() { return m_MainState.value().m_SponsorsList; }
		const SponsorsList& GetSponsorsList() const { return m_MainState.value().m_SponsorsList; }

		time_point_t GetLastStatusUpdateTime() const;
	};
}
