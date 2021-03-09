#include "MainWindow.h"
#include "DiscordRichPresence.h"
#include "ConsoleLog/ConsoleLines.h"
#include "Networking/GithubAPI.h"
#include "Networking/SteamAPI.h"
#include "ConsoleLog/NetworkStatus.h"
#include "Platform/Platform.h"
#include "ImGui_TF2BotDetector.h"
#include "Actions/ActionGenerators.h"
#include "BaseTextures.h"
#include "Filesystem.h"
#include "GenericErrors.h"
#include "Log.h"
#include "IPlayer.h"
#include "ReleaseChannel.h"
#include "TextureManager.h"
#include "UpdateManager.h"
#include "Util/PathUtils.h"
#include "Version.h"
#include "GlobalDispatcher.h"
#include "Networking/HTTPClient.h"
#include "SettingsWindow.h"

#include <imgui_desktop/Application.h>
#include <imgui_desktop/ScopeGuards.h>
#include <imgui_desktop/ImGuiHelpers.h>
#include <imgui.h>
#include <libzippp/libzippp.h>
#include <misc/cpp/imgui_stdlib.h>
#include <mh/math/interpolation.hpp>
#include <mh/text/case_insensitive_string.hpp>
#include <mh/text/formatters/error_code.hpp>
#include <mh/text/fmtstr.hpp>
#include <mh/text/string_insertion.hpp>
#include <mh/text/stringops.hpp>
#include <srcon/async_client.h>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>

using namespace tf2_bot_detector;
using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;

namespace tf2_bot_detector
{
	mh::dispatcher& GetDispatcher()
	{
		static mh::dispatcher s_Dispatcher;
		return s_Dispatcher;
	}
}

MainWindow::MainWindow(ImGuiDesktop::Application& app) :
	ImGuiDesktop::Window(app, 800, 600, mh::fmtstr<128>("TF2 Bot Detector v{}", VERSION).c_str()),
	m_WorldState(IWorldState::Create(m_Settings)),
	m_ActionManager(IRCONActionManager::Create(m_Settings, GetWorld())),
	m_TextureManager(ITextureManager::Create()),
	m_UpdateManager(IUpdateManager::Create(m_Settings))
{
	SetIsPrimaryAppWindow(true);
	ShowWindow();

	ILogManager::GetInstance().CleanupLogFiles();

	GetWorld().AddConsoleLineListener(this);
	GetWorld().AddWorldEventListener(this);

	PrintDebugInfo();

	m_OpenTime = clock_t::now();

	GetActionManager().AddPeriodicActionGenerator<StatusUpdateActionGenerator>();
	GetActionManager().AddPeriodicActionGenerator<ConfigActionGenerator>();
	GetActionManager().AddPeriodicActionGenerator<LobbyDebugActionGenerator>();

	//app.AddManagedWindow(std::make_unique<SettingsWindow>(app, m_Settings));
}

MainWindow::~MainWindow()
{
}

void MainWindow::SetupFonts()
{
	// Add ProggyClean.ttf 24px (200%) and ProggyTiny 10px

	ImFontConfig config{};
	config.OversampleV = config.OversampleH = 1; // Bitmap fonts look bad with oversampling

	if (!m_ProggyTiny10Font)
	{
		m_ProggyTiny10Font = ImGui::GetIO().Fonts->AddFontFromFileTTF(
			IFilesystem::Get().ResolvePath("fonts/ProggyTiny.ttf", PathUsage::Read).string().c_str(),
			10, &config);
	}

	if (!m_ProggyTiny20Font)
	{
		m_ProggyTiny20Font = ImGui::GetIO().Fonts->AddFontFromFileTTF(
			IFilesystem::Get().ResolvePath("fonts/ProggyTiny.ttf", PathUsage::Read).string().c_str(),
			20, &config);
	}

	if (!m_ProggyClean26Font)
	{
		config.GlyphOffset.y = 1;
		m_ProggyClean26Font = ImGui::GetIO().Fonts->AddFontFromFileTTF(
			IFilesystem::Get().ResolvePath("fonts/ProggyClean.ttf", PathUsage::Read).string().c_str(),
			26, &config);
	}
}

void MainWindow::OnImGuiInit()
{
	Super::OnImGuiInit();

	ImGui::GetIO().FontGlobalScale = m_Settings.m_Theme.m_GlobalScale;
	ImGui::GetIO().FontDefault = GetFontPointer(m_Settings.m_Theme.m_Font);

	SetupFonts();
}

void MainWindow::OnOpenGLInit()
{
	Super::OnOpenGLInit();

	m_BaseTextures = IBaseTextures::Create(*m_TextureManager);
}

ImFont* MainWindow::GetFontPointer(Font f) const
{
	const auto defaultFont = ImGui::GetIO().Fonts->Fonts[0];
	switch (f)
	{
	default:
		LogError("Unknown font {}", mh::enum_fmt(f));
	case Font::ProggyClean_13px:
		return defaultFont;
	case Font::ProggyClean_26px:
		return m_ProggyClean26Font ? m_ProggyClean26Font : defaultFont;
	case Font::ProggyTiny_10px:
		return m_ProggyTiny10Font ? m_ProggyTiny10Font : defaultFont;
	case Font::ProggyTiny_20px:
		return m_ProggyTiny20Font ? m_ProggyTiny20Font : defaultFont;
	}
}

void MainWindow::OnDrawColorPicker(const char* name, std::array<float, 4>& color)
{
	if (ImGui::ColorEdit4(name, color.data(), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreview))
		m_Settings.SaveFile();
}

void MainWindow::OnDrawColorPickers(const char* id, const std::initializer_list<ColorPicker>& pickers)
{
	ImGui::HorizontalScrollBox(id, [&]
		{
			for (const auto& picker : pickers)
			{
				OnDrawColorPicker(picker.m_Name, picker.m_Color);
				ImGui::SameLine();
			}
		});
}

void MainWindow::OnDrawChat()
{
	OnDrawColorPickers("ChatColorPickers",
		{
			{ "You", m_Settings.m_Theme.m_Colors.m_ChatLogYouFG },
			{ "Enemies", m_Settings.m_Theme.m_Colors.m_ChatLogEnemyTeamFG },
			{ "Friendlies", m_Settings.m_Theme.m_Colors.m_ChatLogFriendlyTeamFG },
		});

	ImGui::AutoScrollBox("##fileContents", { 0, 0 }, [&]()
		{
			if (!m_MainState)
				return;

			ImGui::PushTextWrapPos();

			const IConsoleLine::PrintArgs args{ m_Settings, *m_WorldState, *this };
			for (auto it = m_MainState->m_PrintingLines.rbegin(); it != m_MainState->m_PrintingLines.rend(); ++it)
			{
				assert(*it);
				(*it)->Print(args);
			}

			ImGui::PopTextWrapPos();
		});
}

void MainWindow::OnDrawAppLog()
{
	ImGui::AutoScrollBox("AppLog", { 0, 0 }, [&]()
		{
			ImGui::PushTextWrapPos();

			const void* lastLogMsg = nullptr;
			for (const LogMessage& msg : ILogManager::GetInstance().GetVisibleMsgs())
			{
				const std::tm timestamp = ToTM(msg.m_Timestamp);

				ImGuiDesktop::ScopeGuards::ID id(&msg);

				ImGui::BeginGroup();
				ImGui::TextColored({ 0.25f, 1.0f, 0.25f, 0.25f }, "[%02i:%02i:%02i]",
					timestamp.tm_hour, timestamp.tm_min, timestamp.tm_sec);

				ImGui::SameLine();
				ImGui::TextFmt({ msg.m_Color.r, msg.m_Color.g, msg.m_Color.b, msg.m_Color.a }, msg.m_Text);
				ImGui::EndGroup();

				if (auto scope = ImGui::BeginPopupContextItemScope("AppLogContextMenu"))
				{
					if (ImGui::MenuItem("Copy"))
						ImGui::SetClipboardText(msg.m_Text.c_str());
				}

				lastLogMsg = &msg;
			}

			if (m_LastLogMessage != lastLogMsg)
			{
				m_LastLogMessage = lastLogMsg;
				QueueUpdate();
			}

			ImGui::PopTextWrapPos();
		});
}

void MainWindow::OpenSettingsPopup()
{
	if (!m_SettingsWindow)
		m_SettingsWindow = std::make_unique<SettingsWindow>(GetApplication(), m_Settings, *this);
	else
		m_SettingsWindow->RaiseWindow();
}

void MainWindow::OnDrawUpdateCheckPopup()
{
	static constexpr char POPUP_NAME[] = "Check for Updates##Popup";

	static bool s_Open = false;
	if (m_UpdateCheckPopupOpen)
	{
		m_UpdateCheckPopupOpen = false;
		ImGui::OpenPopup(POPUP_NAME);
		s_Open = true;
	}
}

void MainWindow::OpenUpdateCheckPopup()
{
	m_UpdateCheckPopupOpen = true;
}

void MainWindow::OnDrawAboutPopup()
{
	static constexpr char POPUP_NAME[] = "About##Popup";

	static bool s_Open = false;
	if (m_AboutPopupOpen)
	{
		m_AboutPopupOpen = false;
		ImGui::OpenPopup(POPUP_NAME);
		s_Open = true;
	}

	ImGui::SetNextWindowSize({ 600, 450 }, ImGuiCond_Appearing);
	if (ImGui::BeginPopupModal(POPUP_NAME, &s_Open))
	{
		ImGui::PushTextWrapPos();

		ImGui::TextFmt("TF2 Bot Detector v\"{}\n"
			"\n"
			"Automatically detects and votekicks cheaters in Team Fortress 2 Casual.\n"
			"\n"
			"This program is free, open source software licensed under the MIT license. Full license text"
			" for this program and its dependencies can be found in the licenses subfolder next to this"
			" executable.", VERSION);

		ImGui::NewLine();
		ImGui::Separator();
		ImGui::NewLine();

		ImGui::TextFmt("Credits");
		ImGui::Spacing();
		if (ImGui::TreeNode("Code/concept by Matt \"pazer\" Haynie"))
		{
			if (ImGui::Selectable("GitHub - PazerOP", false, ImGuiSelectableFlags_DontClosePopups))
				Shell::OpenURL("https://github.com/PazerOP");
			if (ImGui::Selectable("Twitter - @PazerFromSilver", false, ImGuiSelectableFlags_DontClosePopups))
				Shell::OpenURL("https://twitter.com/PazerFromSilver");

			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Artwork/icon by S-Purple"))
		{
			if (ImGui::Selectable("Twitter (NSFW)", false, ImGuiSelectableFlags_DontClosePopups))
				Shell::OpenURL("https://twitter.com/spurpleheart");

			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Documentation/moderation by Nicholas \"ClusterConsultant\" Flamel"))
		{
			if (ImGui::Selectable("GitHub - ClusterConsultant", false, ImGuiSelectableFlags_DontClosePopups))
				Shell::OpenURL("https://github.com/ClusterConsultant");

			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Other Attributions"))
		{
			ImGui::TextFmt("\"Game Ban\" icon made by Freepik from www.flaticon.com");
			ImGui::TreePop();
		}

		ImGui::NewLine();
		ImGui::Separator();
		ImGui::NewLine();

		if (m_MainState)
		{
			if (const auto sponsors = GetSponsorsList().GetSponsors(); !sponsors.empty())
			{
				ImGui::TextFmt("Sponsors\n"
					"Huge thanks to the people sponsoring this project via GitHub Sponsors:");

				ImGui::NewLine();

				for (const auto& sponsor : sponsors)
				{
					ImGui::Bullet();
					ImGui::TextFmt(sponsor.m_Name);

					if (!sponsor.m_Message.empty())
					{
						ImGui::SameLineNoPad();
						ImGui::TextFmt(" - {}", sponsor.m_Message);
					}
				}

				ImGui::NewLine();
			}
		}

		ImGui::TextFmt("If you're feeling generous, you can make a small donation to help support my work.");
		if (ImGui::Button("GitHub Sponsors"))
			Shell::OpenURL("https://github.com/sponsors/PazerOP");

		ImGui::EndPopup();
	}
}

void MainWindow::PrintDebugInfo()
{
	DebugLog("Debug Info:"s
		<< "\n\tSteam dir:         " << m_Settings.GetSteamDir()
		<< "\n\tTF dir:            " << m_Settings.GetTFDir()
		<< "\n\tSteamID:           " << m_Settings.GetLocalSteamID()
		<< "\n\tVersion:           " << VERSION
		<< "\n\tIs CI Build:       " << std::boolalpha << (TF2BD_IS_CI_COMPILE ? true : false)
		<< "\n\tCompile Timestamp: " << __TIMESTAMP__
		<< "\n\tOpenGL Version:    " << GetGLContextVersion()

		<< "\n\tIs Debug Build:    "
#ifdef _DEBUG
		<< true
#else
		<< false
#endif

#ifdef _MSC_FULL_VER
		<< "\n\t-D _MSC_FULL_VER:  " << _MSC_FULL_VER
#endif
#if _M_X64
		<< "\n\t-D _M_X64:         " << _M_X64
#endif
#if _MT
		<< "\n\t-D _MT:            " << _MT
#endif
	);
}

void MainWindow::GenerateDebugReport() try
{
	Log("Generating debug_report.zip...");
	const auto dbgReportLocation = IFilesystem::Get().ResolvePath("debug_report.zip", PathUsage::WriteLocal);

	{
		using namespace libzippp;
		ZipArchive archive(dbgReportLocation.string());
		archive.open(ZipArchive::New);

		const auto logsDir = IFilesystem::Get().GetLogsDir();
		for (const auto& entry : std::filesystem::recursive_directory_iterator(logsDir))
		{
			if (!entry.is_regular_file())
				continue;

			const auto& path = entry.path();

			if (archive.addFile(std::filesystem::relative(path, logsDir / "..").string(), path.string()))
				Log("Added file to debug report: {}", path);
			else
				LogWarning("Failed to add file to debug report: {}", path);
		}

		if (auto err = archive.close(); err != LIBZIPPP_OK)
		{
			LogError("Failed to close debug report zip archive: close() returned {}", err);
			return;
		}
	}

	Log("Finished generating debug_report.zip ({})", dbgReportLocation);
	Shell::ExploreToAndSelect(dbgReportLocation);
}
catch (...)
{
	LogException(MH_SOURCE_LOCATION_CURRENT(), "Failed to generate debug_report.zip");
}

void MainWindow::OnDrawServerStats()
{
	ImGui::PlotLines("Edicts", [&](int idx)
		{
			return m_EdictUsageSamples[idx].m_UsedEdicts;
		}, (int)m_EdictUsageSamples.size(), 0, nullptr, 0, 2048);

	if (!m_EdictUsageSamples.empty())
	{
		ImGui::SameLine(0, 4);

		auto& lastSample = m_EdictUsageSamples.back();
		const float percent = float(lastSample.m_UsedEdicts) / lastSample.m_MaxEdicts;
		ImGui::ProgressBar(percent, { -1, 0 },
			mh::pfstr<64>("%i (%1.0f%%)", lastSample.m_UsedEdicts, percent * 100).c_str());

		ImGui::SetHoverTooltip("{} of {} ({:1.1f}%)", lastSample.m_UsedEdicts, lastSample.m_MaxEdicts, percent * 100);
	}

	if (!m_ServerPingSamples.empty())
	{
		ImGui::PlotLines(mh::fmtstr<64>("Average ping: {}", m_ServerPingSamples.back().m_Ping).c_str(),
			[&](int idx)
			{
				return m_ServerPingSamples[idx].m_Ping;
			}, (int)m_ServerPingSamples.size(), 0, nullptr, 0);
	}

	//OnDrawNetGraph();
}

void MainWindow::OnDraw()
{
	ImGui::GetIO().FontDefault = GetFontPointer(m_Settings.m_Theme.m_Font);
	ImGui::GetIO().FontGlobalScale = m_Settings.m_Theme.m_GlobalScale;

	if (m_SettingsWindow && m_SettingsWindow->ShouldClose())
		m_SettingsWindow.reset();

	OnDrawUpdateCheckPopup();
	OnDrawAboutPopup();

	{
		ISetupFlowPage::DrawState ds;
		ds.m_ActionManager = &GetActionManager();
		ds.m_UpdateManager = m_UpdateManager.get();
		ds.m_Settings = &m_Settings;

		if (m_SetupFlow.OnDraw(m_Settings, ds))
			return;
	}

	if (!m_MainState)
		return;

	const auto& mainWindowState = m_Settings.m_UIState.m_MainWindow;
	const bool columnsEnabled = (mainWindowState.m_ChatEnabled && (mainWindowState.m_AppLogEnabled || mainWindowState.m_ScoreboardEnabled));

	if (columnsEnabled)
		ImGui::Columns(2, "MainWindowSplit");

	ImGui::HorizontalScrollBox("SettingsScroller", [&]
		{
			ImGui::Checkbox("Pause", &m_Paused); ImGui::SameLine();

			auto& settings = m_Settings;
			const auto ModerationCheckbox = [&settings](const char* name, bool& value, const char* tooltip)
			{
				{
					ImGuiDesktop::ScopeGuards::TextColor text({ 1, 0.5f, 0, 1 }, !value);
					if (ImGui::Checkbox(name, &value))
						settings.SaveFile();
				}

				const char* orangeReason = "";
				if (!value)
					orangeReason = "\n\nThis label is orange to highlight the fact that it is currently disabled.";

				ImGui::SameLine();
				ImGui::SetHoverTooltip("{}{}", tooltip, orangeReason);
			};

			ModerationCheckbox("Enable Chat Warnings", m_Settings.m_AutoChatWarnings, "Enables chat message warnings about cheaters.");
			ModerationCheckbox("Enable Auto Votekick", m_Settings.m_AutoVotekick, "Automatically votekicks cheaters on your team.");
			ModerationCheckbox("Enable Auto-mark", m_Settings.m_AutoMark, "Automatically marks players matching the detection rules.");

			ImGui::Checkbox("Show Commands", &m_Settings.m_Unsaved.m_DebugShowCommands); ImGui::SameLine();
			ImGui::SetHoverTooltip("Prints out all game commands to the log.");
		});

#ifdef _DEBUG
	{
		ImGui::Value("Time (Compensated)", to_seconds<float>(GetCurrentTimestampCompensated() - m_OpenTime));

		auto leader = GetModLogic().GetBotLeader();
		ImGui::Value("Bot Leader", leader ? mh::fmtstr<128>("{}", *leader).view() : ""sv);

		ImGui::TextFmt("Is vote in progress:");
		ImGui::SameLine();
		if (GetWorld().IsVoteInProgress())
			ImGui::TextFmt({ 1, 1, 0, 1 }, "YES");
		else
			ImGui::TextFmt({ 0, 1, 0, 1 }, "NO");

		ImGui::TextFmt("FPS: {:1.1f}", GetFPS());

		ImGui::Value("Texture Count", m_TextureManager->GetActiveTextureCount());

		ImGui::TextFmt("RAM Usage: {:1.1f} MB", Platform::Processes::GetCurrentRAMUsage() / 1024.0f / 1024);

		if (auto client = m_Settings.GetHTTPClient())
		{
			const IHTTPClient::RequestCounts reqs = client->GetRequestCounts();
			ImGui::TextFmt("HTTP Requests: {} total | ", reqs.m_Total);

			ImGui::SameLineNoPad();
			ImGui::TextFmt({ 1, 0.5f, 0.5f, 1 }, "{} failed ({:1.1f}%)",
				reqs.m_Failed, reqs.m_Failed / float(reqs.m_Total) * 100);

			const auto QueuedText = [](uint32_t count, const std::string_view& name)
			{
				ImGui::SameLineNoPad();
				ImGui::TextFmt(" | ");

				ImGui::SameLineNoPad();
				ImGui::TextFmt(
					count > 0 ? ImVec4{ 2.0f / 3, 1, 2.0f / 3, 1 } : ImVec4{ 0.5f, 0.5f, 0.5f, 1 },
					"{} {}", count, name);
			};

			QueuedText(reqs.m_InProgress, "running");
			QueuedText(reqs.m_Throttled, "throttled");
		}
		else
		{
			ImGui::TextFmt("HTTP Requests: HTTPClient Unavailable");
		}
	}
#endif

	ImGui::Value("Blacklisted user count", GetModLogic().GetBlacklistedPlayerCount());
	ImGui::Value("Rule count", GetModLogic().GetRuleCount());

	if (m_MainState)
	{
		auto& world = GetWorld();
		const auto parsedLineCount = m_ParsedLineCount;
		const auto parseProgress = m_MainState->m_Parser.GetParseProgress();

		if (parseProgress < 0.95f)
		{
			ImGui::ProgressBar(parseProgress, { 0, 0 }, mh::pfstr<64>("%1.2f %%", parseProgress * 100).c_str());
			ImGui::SameLine(0, 4);
		}

		ImGui::Value("Parsed line count", parsedLineCount);
	}

	//OnDrawServerStats();
	if (mainWindowState.m_ChatEnabled)
		OnDrawChat();

	if (columnsEnabled)
		ImGui::NextColumn();

	if (mainWindowState.m_ScoreboardEnabled)
		OnDrawScoreboard();
	if (mainWindowState.m_AppLogEnabled)
		OnDrawAppLog();

	if (columnsEnabled)
		ImGui::Columns();

	if (!mainWindowState.m_ChatEnabled && !mainWindowState.m_ScoreboardEnabled && !mainWindowState.m_AppLogEnabled)
		OnDrawAllPanesDisabled();
}

void MainWindow::OnDrawAllPanesDisabled()
{
	ImGui::NewLine();
	ImGui::NewLine();
	ImGui::Text("All panes are currently disabled.");
	ImGui::NewLine();

	auto& mainWindowState = m_Settings.m_UIState.m_MainWindow;
	if (ImGui::Button("Show Chat"))
	{
		mainWindowState.m_ChatEnabled = true;
		m_Settings.SaveFile();
	}
	if (ImGui::Button("Show Scoreboard"))
	{
		mainWindowState.m_ScoreboardEnabled = true;
		m_Settings.SaveFile();
	}
	if (ImGui::Button("Show App Log"))
	{
		mainWindowState.m_AppLogEnabled = true;
		m_Settings.SaveFile();
	}
}

void MainWindow::OnEndFrame()
{
	m_TextureManager->EndFrame();
}

void MainWindow::OnDrawMenuBar()
{
	const bool isInSetupFlow = m_SetupFlow.ShouldDraw();

	if (ImGui::BeginMenu("File"))
	{
		if (ImGui::MenuItem("Open Config Folder"))
			Shell::ExploreTo(IFilesystem::Get().GetConfigDir());
		if (ImGui::MenuItem("Open Logs Folder"))
			Shell::ExploreTo(IFilesystem::Get().GetLogsDir());

		ImGui::Separator();

		if (!isInSetupFlow)
		{
			if (ImGui::MenuItem("Reload Playerlists/Rules"))
				GetModLogic().ReloadConfigFiles();
			if (ImGui::MenuItem("Reload Settings"))
				m_Settings.LoadFile();

			ImGui::Separator();
		}

		if (ImGui::MenuItem("Generate Debug Report"))
			GenerateDebugReport();

		ImGui::Separator();

		if (ImGui::MenuItem("Exit", "Alt+F4"))
			SetShouldClose(true);

		ImGui::EndMenu();
	}

	if (!isInSetupFlow && ImGui::BeginMenu("View"))
	{
		if (ImGui::MenuItem("Show Chat Log", nullptr, &m_Settings.m_UIState.m_MainWindow.m_ChatEnabled))
			m_Settings.SaveFile();
		if (ImGui::MenuItem("Show App Log", nullptr, &m_Settings.m_UIState.m_MainWindow.m_AppLogEnabled))
			m_Settings.SaveFile();
		if (ImGui::MenuItem("Show Team Stats", nullptr, &m_Settings.m_UIState.m_MainWindow.m_TeamStatsEnabled))
			m_Settings.SaveFile();
		if (ImGui::MenuItem("Show Scoreboard", nullptr, &m_Settings.m_UIState.m_MainWindow.m_ScoreboardEnabled))
			m_Settings.SaveFile();

		ImGui::EndMenu();
	}

#ifdef _DEBUG
	if (ImGui::BeginMenu("Debug"))
	{
		ImGui::Separator();

		if (ImGui::MenuItem("Crash"))
		{
			struct Test
			{
				int i;
			};

			Test* testPtr = nullptr;
			testPtr->i = 42;
		}
		ImGui::EndMenu();
	}

#ifdef _DEBUG
	static bool s_ImGuiDemoWindow = false;
#endif
	if (ImGui::BeginMenu("Window"))
	{
#ifdef _DEBUG
		ImGui::MenuItem("ImGui Demo Window", nullptr, &s_ImGuiDemoWindow);
#endif
		ImGui::EndMenu();
	}

#ifdef _DEBUG
	if (s_ImGuiDemoWindow)
		ImGui::ShowDemoWindow(&s_ImGuiDemoWindow);
#endif
#endif

	if (!isInSetupFlow || m_SetupFlow.GetCurrentPage() == SetupFlowPage::TF2CommandLine)
	{
		if (ImGui::MenuItem("Settings"))
			OpenSettingsPopup();
	}

	if (ImGui::BeginMenu("Help"))
	{
		if (ImGui::MenuItem("Open GitHub"))
			Shell::OpenURL("https://github.com/PazerOP/tf2_bot_detector");
		if (ImGui::MenuItem("Open Discord"))
			Shell::OpenURL("https://discord.gg/W8ZSh3Z");

		ImGui::Separator();

		static const mh::fmtstr<128> VERSION_STRING_LABEL("Version: {}", VERSION);
		ImGui::MenuItem(VERSION_STRING_LABEL.c_str(), nullptr, false, false);

		ImGui::Separator();

		if (ImGui::MenuItem("About TF2 Bot Detector"))
			OpenAboutPopup();

		ImGui::EndMenu();
	}
}

void MainWindow::PostSetupFlowState::OnUpdateDiscord()
{
#ifdef TF2BD_ENABLE_DISCORD_INTEGRATION
	const auto curTime = clock_t::now();
	if (!m_DRPManager && m_Parent->m_Settings.m_Discord.m_EnableRichPresence)
	{
		m_DRPManager = IDRPManager::Create(m_Parent->m_Settings, m_Parent->GetWorld());
	}
	else if (m_DRPManager && !m_Parent->m_Settings.m_Discord.m_EnableRichPresence)
	{
		m_DRPManager.reset();
	}

	if (m_DRPManager)
		m_DRPManager->Update();
#endif
}

void MainWindow::OnUpdate()
{
	if (m_Paused)
		return;

	GetDispatcher().run_for(10ms);

	GetWorld().Update();
	m_UpdateManager->Update();

	if (m_Settings.m_Unsaved.m_RCONClient)
		m_Settings.m_Unsaved.m_RCONClient->set_logging(m_Settings.m_Logging.m_RCONPackets);

	if (m_SetupFlow.OnUpdate(m_Settings))
	{
		m_MainState.reset();
	}
	else
	{
		if (!m_MainState)
			m_MainState.emplace(*this);

		m_MainState->m_Parser.Update();
		GetModLogic().Update();

		m_MainState->OnUpdateDiscord();
	}

	GetActionManager().Update();
}

void MainWindow::OnConsoleLogChunkParsed(IWorldState& world, bool consoleLinesUpdated)
{
	assert(&world == &GetWorld());

	if (consoleLinesUpdated)
		UpdateServerPing(GetCurrentTimestampCompensated());
}

bool MainWindow::IsSleepingEnabled() const
{
	return m_Settings.m_SleepWhenUnfocused && !HasFocus();
}

bool MainWindow::IsTimeEven() const
{
	const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(clock_t::now() - m_OpenTime);
	return !(seconds.count() % 2);
}

float MainWindow::TimeSine(float interval, float min, float max) const
{
	const auto elapsed = (clock_t::now() - m_OpenTime) % std::chrono::duration_cast<clock_t::duration>(std::chrono::duration<float>(interval));
	const auto progress = std::chrono::duration<float>(elapsed).count() / interval;
	return mh::remap(std::sin(progress * 6.28318530717958647693f), -1.0f, 1.0f, min, max);
}

void MainWindow::OnConsoleLineParsed(IWorldState& world, IConsoleLine& parsed)
{
	m_ParsedLineCount++;

	if (parsed.ShouldPrint() && m_MainState)
	{
		while (m_MainState->m_PrintingLines.size() > m_MainState->MAX_PRINTING_LINES)
			m_MainState->m_PrintingLines.pop_back();

		m_MainState->m_PrintingLines.push_front(parsed.shared_from_this());
	}

	switch (parsed.GetType())
	{
	case ConsoleLineType::LobbyChanged:
	{
		auto& lobbyChangedLine = static_cast<const LobbyChangedLine&>(parsed);
		const LobbyChangeType changeType = lobbyChangedLine.GetChangeType();

		if (changeType == LobbyChangeType::Created || changeType == LobbyChangeType::Updated)
			GetActionManager().QueueAction<LobbyUpdateAction>();

		break;
	}
	case ConsoleLineType::EdictUsage:
	{
		auto& usageLine = static_cast<const EdictUsageLine&>(parsed);
		m_EdictUsageSamples.push_back({ usageLine.GetTimestamp(), usageLine.GetUsedEdicts(), usageLine.GetTotalEdicts() });

		while (m_EdictUsageSamples.front().m_Timestamp < (usageLine.GetTimestamp() - 5min))
			m_EdictUsageSamples.erase(m_EdictUsageSamples.begin());

		break;
	}

	default: break;
	}
}

void MainWindow::OnConsoleLineUnparsed(IWorldState& world, const std::string_view& text)
{
	m_ParsedLineCount++;
}

mh::generator<IPlayer&> MainWindow::PostSetupFlowState::GeneratePlayerPrintData()
{
	IPlayer* printData[33]{};
	auto begin = std::begin(printData);
	auto end = std::end(printData);
	assert(begin <= end);
	auto& world = m_Parent->m_WorldState;
	assert(static_cast<size_t>(end - begin) >= world->GetApproxLobbyMemberCount());

	std::fill(begin, end, nullptr);

	{
		auto* current = begin;
		for (IPlayer& member : world->GetLobbyMembers())
		{
			*current = &member;
			current++;
		}

		if (current == begin)
		{
			// We seem to have either an empty lobby or we're playing on a community server.
			// Just find the most recent status updates.
			for (IPlayer& playerData : world->GetPlayers())
			{
				if (playerData.GetLastStatusUpdateTime() >= (world->GetLastStatusUpdateTime() - 15s))
				{
					*current = &playerData;
					current++;

					if (current >= end)
						break; // This might happen, but we're not in a lobby so everything has to be approximate
				}
			}
		}

		end = current;
	}

	std::sort(begin, end, [](const IPlayer* lhs, const IPlayer* rhs) -> bool
		{
			assert(lhs);
			assert(rhs);
			//if (!lhs && !rhs)
			//	return false;
			//if (auto result = !!rhs <=> !!lhs; !std::is_eq(result))
			//	return result < 0;

			// Intentionally reversed, we want descending kill order
			if (auto killsResult = rhs->GetScores().m_Kills <=> lhs->GetScores().m_Kills; !std::is_eq(killsResult))
				return std::is_lt(killsResult);

			if (auto deathsResult = lhs->GetScores().m_Deaths <=> rhs->GetScores().m_Deaths; !std::is_eq(deathsResult))
				return std::is_lt(deathsResult);

			// Sort by ascending userid
			{
				auto luid = lhs->GetUserID();
				auto ruid = rhs->GetUserID();
				if (luid && ruid)
				{
					if (auto result = *luid <=> *ruid; !std::is_eq(result))
						return std::is_lt(result);
				}
			}

			return false;
		});

	for (auto it = begin; it != end; ++it)
		co_yield **it;
}

void MainWindow::UpdateServerPing(time_point_t timestamp)
{
	if ((timestamp - m_LastServerPingSample) <= 7s)
		return;

	float totalPing = 0;
	uint16_t samples = 0;

	for (IPlayer& player : GetWorld().GetPlayers())
	{
		if (player.GetLastStatusUpdateTime() < (timestamp - 20s))
			continue;

		auto& data = player.GetOrCreateData<PlayerExtraData>(player);
		totalPing += data.GetAveragePing();
		samples++;
	}

	m_ServerPingSamples.push_back({ timestamp, uint16_t(totalPing / samples) });
	m_LastServerPingSample = timestamp;

	while ((timestamp - m_ServerPingSamples.front().m_Timestamp) > 5min)
		m_ServerPingSamples.erase(m_ServerPingSamples.begin());
}

time_point_t MainWindow::GetLastStatusUpdateTime() const
{
	return GetWorld().GetLastStatusUpdateTime();
}

float MainWindow::PlayerExtraData::GetAveragePing() const
{
	//throw std::runtime_error("TODO");
#if 1
	unsigned totalPing = m_Parent->GetPing();
	unsigned samples = 1;

	for (const auto& entry : m_PingHistory)
	{
		totalPing += entry.m_Ping;
		samples++;
	}

	return totalPing / float(samples);
#endif
}

time_point_t MainWindow::GetCurrentTimestampCompensated() const
{
	return GetWorld().GetCurrentTime();
}

mh::expected<std::shared_ptr<ITexture>, std::error_condition> MainWindow::TryGetAvatarTexture(IPlayer& player)
{
	using StateTask_t = mh::task<mh::expected<std::shared_ptr<ITexture>, std::error_condition>>;

	struct PlayerAvatarData
	{
		StateTask_t m_State;

		static StateTask_t LoadAvatarAsync(mh::task<Bitmap> avatarBitmapTask,
			mh::dispatcher updateDispatcher, std::shared_ptr<ITextureManager> textureManager)
		{
			const Bitmap* avatarBitmap = nullptr;

			try
			{
				avatarBitmap = &(co_await avatarBitmapTask);
			}
			catch (...)
			{
				LogException(MH_SOURCE_LOCATION_CURRENT(), "Failed to load avatar bitmap");
				co_return ErrorCode::UnknownError;
			}

			// Switch to main thread
			co_await updateDispatcher.co_dispatch();

			try
			{
				co_return textureManager->CreateTexture(*avatarBitmap);
			}
			catch (...)
			{
				LogException(MH_SOURCE_LOCATION_CURRENT(), "Failed to create avatar bitmap");
				co_return ErrorCode::UnknownError;
			}
		}
	};

	auto& avatarData = player.GetOrCreateData<PlayerAvatarData>().m_State;

	if (avatarData.empty())
	{
		auto playerPtr = player.shared_from_this();
		const auto& summary = playerPtr->GetPlayerSummary();
		if (summary)
		{
			avatarData = PlayerAvatarData::LoadAvatarAsync(
				summary->GetAvatarBitmap(m_Settings.GetHTTPClient()),
				GetDispatcher(), m_TextureManager);
		}
		else
		{
			return summary.error();
		}
	}

	if (auto data = avatarData.try_get())
		return *data;
	else
		return std::errc::operation_in_progress;
}

MainWindow::PostSetupFlowState::PostSetupFlowState(MainWindow& window) :
	m_Parent(&window),
	m_ModeratorLogic(IModeratorLogic::Create(window.GetWorld(), window.m_Settings, window.GetActionManager())),
	m_SponsorsList(window.m_Settings),
	m_Parser(window.GetWorld(), window.m_Settings, window.m_Settings.GetTFDir() / "console.log")
{
#ifdef TF2BD_ENABLE_DISCORD_INTEGRATION
	m_DRPManager = IDRPManager::Create(window.m_Settings, window.GetWorld());
#endif
}
