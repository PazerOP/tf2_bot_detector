#include "SetupFlow.h"
#include "Log.h"
#include "Config/Settings.h"
#include "PathUtils.h"
#include "ImGui_TF2BotDetector.h"
#include "PlatformSpecific/Steam.h"
#include "Version.h"
#include "PlatformSpecific/Processes.h"
#include "PlatformSpecific/Shell.h"

#include <imgui.h>
#include <imgui_desktop/ScopeGuards.h>
#include <misc/cpp/imgui_stdlib.h>
#include <mh/text/string_insertion.hpp>
#include <vdf_parser.hpp>

#include <optional>
#include <string_view>
#include <random>

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace tf2_bot_detector;
namespace ScopeGuards = ImGuiDesktop::ScopeGuards;

#ifdef _DEBUG
namespace tf2_bot_detector
{
	extern uint32_t g_StaticRandomSeed;
}
#endif

namespace
{
	class BasicSettingsPage final : public SetupFlow::IPage
	{
	public:
		template<typename T>
		static bool InternalValidateSettings(const T& settings)
		{
			if (auto steamDir = settings.GetSteamDir(); steamDir.empty() || !ValidateSteamDir(steamDir))
				return false;

			if (auto tfDir = settings.GetTFDir(); tfDir.empty() || !ValidateTFDir(tfDir))
				return false;

			if (!settings.GetLocalSteamID().IsValid())
				return false;

			return true;
		}

		bool ValidateSettings(const Settings& settings) const override
		{
			return InternalValidateSettings(settings);
		}

		OnDrawResult OnDraw(const DrawState& ds) override
		{
			ImGui::TextUnformatted("Due to your configuration, some settings could not be automatically detected.");
			ImGui::NewLine();

			// 1. Steam directory
			bool any = false;
			//if (auto autodetected = GetCurrentSteamDir(); autodetected.empty())
			{
				any = true;
				ImGui::TextUnformatted("Location of your Steam directory:");
				ImGui::NewLine();
				ScopeGuards::Indent indent;
				ImGui::TextUnformatted("Your Steam directory is needed to execute commands in TF2, read console logs, and determine your current Steam ID.");

				InputTextSteamDirOverride("##SetupFlow_SteamDir", m_Settings.m_SteamDirOverride);

				ImGui::NewLine();
			}

			//if (FindTFDir(m_Settings.GetSteamDir()).empty())
			{
				any = true;
				ImGui::TextUnformatted("Location of your tf directory:");
				ImGui::NewLine();
				ImGui::Indent();
				ImGui::TextUnformatted("Your tf directory is needed so this tool knows where to read console output from. It is also needed to automatically run commands in the game.");

				InputTextTFDirOverride("##SetupFlow_TFDir", m_Settings.m_TFDirOverride, FindTFDir(m_Settings.GetSteamDir()));

				ImGui::Unindent();
			}

			// 2. Steam ID
			//if (!GetCurrentActiveSteamID().IsValid())
			{
				any = true;
				ImGui::TextUnformatted("Your Steam ID:"sv);
				ImGui::NewLine();
				ImGui::Indent();
				ImGui::TextUnformatted("Your Steam ID is needed to identify who can be votekicked (same team) and who is on the other team. You can find your Steam ID using sites like steamidfinder.com.");

				InputTextSteamIDOverride("##SetupFlow_SteamID", m_Settings.m_LocalSteamIDOverride);

				ImGui::Unindent();
				ImGui::NewLine();
			}

			return any ? OnDrawResult::ContinueDrawing : OnDrawResult::EndDrawing;
		}

		void Init(const Settings& settings) override
		{
			m_Settings = settings;
		}

		bool CanCommit() const override { return InternalValidateSettings(m_Settings); }
		void Commit(Settings& settings)
		{
			static_cast<AutoDetectedSettings&>(settings) = m_Settings;
		}

	private:
		AutoDetectedSettings m_Settings;
	};

	class NetworkSettingsPage final : public SetupFlow::IPage
	{
	public:
		template<typename T>
		static bool InternalValidateSettings(const T& settings)
		{
			if (!settings.m_AllowInternetUsage)
				return false;
			if (settings.m_AllowInternetUsage.value() && settings.m_ProgramUpdateCheckMode == ProgramUpdateCheckMode::Unknown)
				return false;
			if (settings.m_ProgramUpdateCheckMode == ProgramUpdateCheckMode::Releases && VERSION.m_Preview != 0)
				return false;

			return true;
		}

		bool ValidateSettings(const Settings& settings) const override
		{
			return InternalValidateSettings(settings);
		}

		OnDrawResult OnDraw(const DrawState& ds) override
		{
			ImGui::TextUnformatted("This tool can optionally connect to the internet to automatically update.");

			ImGui::Indent();
			{
				if (bool allow = m_Settings.m_AllowInternetUsage.value_or(false); ImGui::Checkbox("Allow Internet Connectivity", &allow))
					m_Settings.m_AllowInternetUsage = allow;

				if (m_Settings.m_AllowInternetUsage.value_or(false))
					ImGui::TextColoredUnformatted({ 1, 1, 0, 1 }, "If you use antivirus software, connecting to the internet may trigger warnings.");
			}
			ImGui::Unindent();
			ImGui::NewLine();

			const bool enabled = m_Settings.m_AllowInternetUsage.value_or(false);
			ImGui::EnabledSwitch(enabled, [&](bool enabled)
				{
					ImGui::BeginGroup();
					ImGui::TextUnformatted("This tool can also check for updated functionality and bugfixes on startup.");
					ImGui::Indent();
					{
						auto mode = enabled ? m_Settings.m_ProgramUpdateCheckMode : ProgramUpdateCheckMode::Disabled;
						if (Combo("##SetupFlow_UpdateCheckingMode", mode))
							m_Settings.m_ProgramUpdateCheckMode = mode;

						if (m_Settings.m_ProgramUpdateCheckMode == ProgramUpdateCheckMode::Disabled)
							ImGui::TextUnformatted("You can always check for updates manually via the Help menu.");
					}
					ImGui::Unindent();
					ImGui::EndGroup();
				}, "Requires \"Allow Internet Connectivity\"");

			return OnDrawResult::ContinueDrawing;
		}

		void Init(const Settings& settings) override
		{
			m_Settings.m_AllowInternetUsage = settings.m_AllowInternetUsage.value_or(true);
			m_Settings.m_ProgramUpdateCheckMode = settings.m_ProgramUpdateCheckMode;
			if (m_Settings.m_ProgramUpdateCheckMode == ProgramUpdateCheckMode::Unknown)
				m_Settings.m_ProgramUpdateCheckMode = ProgramUpdateCheckMode::Releases;
		}
		bool CanCommit() const override { return InternalValidateSettings(m_Settings); }
		void Commit(Settings& settings) override
		{
			settings.m_AllowInternetUsage = m_Settings.m_AllowInternetUsage;
			settings.m_ProgramUpdateCheckMode = m_Settings.m_ProgramUpdateCheckMode;
		}

	private:
		struct
		{
			std::optional<bool> m_AllowInternetUsage;
			ProgramUpdateCheckMode m_ProgramUpdateCheckMode = ProgramUpdateCheckMode::Unknown;

		} m_Settings;
	};

	class UseRCONCmdLinePage final : public SetupFlow::IPage
	{
		struct ConfigFile
		{
			std::filesystem::path m_Filename;
			tyti::vdf::object m_File;
		};

		static cppcoro::generator<ConfigFile> GetConfigFiles(std::filesystem::path steamDir)
		{
			for (auto localAccount : std::filesystem::directory_iterator(steamDir / "userdata"))
			{
				if (!localAccount.is_directory())
					continue;

				ConfigFile file;

				file.m_Filename = localAccount.path() / "config/localconfig.vdf";
				if (!std::filesystem::exists(file.m_Filename))
				{
					LogWarning(std::string(__FUNCTION__) << "(): " << file.m_Filename << " not found");
					continue;
				}

				{
					std::ifstream localCfg(file.m_Filename, std::ios::binary);
					if (!localCfg.good())
					{
						LogError(std::string(__FUNCTION__) << "(): Failed to open " << file.m_Filename);
						continue;
					}

					file.m_File = tyti::vdf::read(localCfg);
				}

				co_yield file;
			}
		}

		template<typename TFunc>
		static bool ConfigFilesHaveOpt(const std::filesystem::path& steamDir, TFunc&& onLaunchOptionMissing)
		{
			for (ConfigFile cfgFile : GetConfigFiles(steamDir))
			{
				auto& tf2cfg = cfgFile.m_File
					.get_or_add_child("Software")
					.get_or_add_child("Valve")
					.get_or_add_child("Steam")
					.get_or_add_child("Apps")
					.get_or_add_child("440");

				auto& launchOpts = tf2cfg.get_or_add_attrib("LaunchOptions");
				if (launchOpts.find("-usercon") == launchOpts.npos)
					onLaunchOptionMissing(launchOpts);
			}

			return true;
		}

	public:
		bool ValidateSettings(const Settings& settings) const override
		{
			bool isLaunchOptionMissing = false;
			ConfigFilesHaveOpt(settings.GetSteamDir(), [&](std::string& opts) { isLaunchOptionMissing = true; });
			return isLaunchOptionMissing;
		}

		OnDrawResult OnDraw(const DrawState& d) override
		{
			ImGui::TextColoredUnformatted({ 1, 0, 0, 1 }, "Not implemented");
			return OnDrawResult::ContinueDrawing;
		}

		void Init(const Settings& settings) override {}
		bool CanCommit() const override { return false; }
		void Commit(Settings& settings) override {}
	};

	static std::string GenerateRandomRCONPassword(size_t length = 16)
	{
		std::mt19937 generator;
#ifdef _DEBUG
		if (g_StaticRandomSeed != 0)
		{
			generator.seed(g_StaticRandomSeed);
		}
		else
#endif
		{
			std::random_device randomSeed;
			generator.seed(randomSeed());
		}

		constexpr char PALETTE[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
		std::uniform_int_distribution<size_t> dist(0, std::size(PALETTE) - 1);

		std::string retVal(length, '\0');
		for (size_t i = 0; i < length; i++)
			retVal[i] = PALETTE[dist(generator)];

		return retVal;
	}

	static uint16_t GenerateRandomRCONPort()
	{
		std::mt19937 generator;
#ifdef _DEBUG
		if (g_StaticRandomSeed != 0)
		{
			generator.seed(g_StaticRandomSeed + 314);
		}
		else
#endif
		{
			std::random_device randomSeed;
			generator.seed(randomSeed());
		}

		// Some routers have issues handling high port numbers. By restricting
		// ourselves to these high port numbers, we add another layer of security.
		std::uniform_int_distribution<uint16_t> dist(40000, 65535);
		return dist(generator);
	}

	class TF2OpenPage final : public SetupFlow::IPage
	{
		std::vector<std::string> m_CommandLineArgs;
		std::shared_future<std::vector<std::string>> m_CommandLineArgsFuture;
		bool m_Ready = false;

		void TryUpdateCmdlineArgs()
		{
			if (!m_CommandLineArgsFuture.valid())
			{
				m_CommandLineArgsFuture = Processes::GetTF2CommandLineArgsAsync();
				return;
			}
			else if (m_CommandLineArgsFuture.wait_for(0s) == std::future_status::ready)
			{
				m_CommandLineArgs = m_CommandLineArgsFuture.get();
				m_CommandLineArgsFuture = Processes::GetTF2CommandLineArgsAsync();
				m_Ready = true;
			}
		}

		bool HasUseRconCmdLineFlag() const
		{
			if (m_CommandLineArgs.size() != 1)
				return false;

			return m_CommandLineArgs.at(0).find("-usercon") != std::string::npos;
		}

	public:
		bool ValidateSettings(const Settings& settings) const override
		{
			if (!Processes::IsTF2Running())
				return false;
			if (!HasUseRconCmdLineFlag())
				return false;

			return true;
		}

		OnDrawResult OnDraw(const DrawState& ds) override
		{
			TryUpdateCmdlineArgs();

			const auto LaunchTF2Button = [&]
			{
				ImGui::NewLine();
				ImGui::EnabledSwitch(m_Ready, [&]
					{
						if (ImGui::Button("Launch TF2"))
						{
							std::string url;
							url << "steam://run/440//"
								" -usercon"
								" +ip 0.0.0.0 +alias ip"
								" +sv_rcon_whitelist_address 127.0.0.1 +alias sv_rcon_whitelist_address"
								" +rcon_password " << m_RCONPassword <<
								" +hostport " << m_RCONPort << " +alias hostport"
								" +con_timestamp 1 +alias con_timestamp"
								" +net_start"
								" -condebug"
								" -conclearlog";

							Shell::OpenURL(std::move(url));
						}
					}, "Finding command line arguments...");
			};

			if (m_CommandLineArgs.empty())
			{
				ImGui::TextUnformatted("Waiting for TF2 to be opened...");
				LaunchTF2Button();
			}
			else if (m_CommandLineArgs.size() > 1)
			{
				ImGui::TextUnformatted("More than one instance of hl2.exe found. Please close the other instances.");
			}
			else if (!HasUseRconCmdLineFlag())
			{
				ImGui::TextUnformatted("TF2 must be run with the -usercon command line flag. You can either add that flag under Launch Options in Steam, or close TF2 and open it with the button below.");

				ImGui::EnabledSwitch(false, LaunchTF2Button, "TF2 is currently running. Please close it first.");
			}
			else
			{
				return OnDrawResult::EndDrawing;
			}

			return OnDrawResult::ContinueDrawing;
		}

		void Init(const Settings& settings) override
		{
			m_RCONPassword = GenerateRandomRCONPassword();
			m_RCONPort = GenerateRandomRCONPort();
			m_Ready = false;
		}
		bool CanCommit() const override { return true; }
		void Commit(Settings& settings) override
		{
			settings.m_Unsaved.m_RCONPassword = m_RCONPassword;
			settings.m_Unsaved.m_RCONPort = m_RCONPort;
		}

		bool WantsSetupText() const override { return false; }
		bool WantsContinueButton() const override { return false; }

	private:
		std::string m_RCONPassword;
		uint16_t m_RCONPort;
	};

#if 0
	class RCONConnectionPage final : public SetupFlow::IPage
	{
	public:
		bool ValidateSettings(const Settings& settings) const override
		{

		}

		OnDrawResult OnDraw(const DrawState& ds) override
		{

		}
	};
#endif
}

SetupFlow::SetupFlow()
{
	m_Pages.push_back(std::make_unique<BasicSettingsPage>());
	m_Pages.push_back(std::make_unique<NetworkSettingsPage>());
	//m_Pages.push_back(std::make_unique<UseRCONCmdLinePage>());
	m_Pages.push_back(std::make_unique<TF2OpenPage>());
	//m_Pages.push_back(std::make_unique<RCONConnectionPage>());
}

bool SetupFlow::OnUpdate(const Settings& settings)
{
	if (ShouldDraw())
		return true;

	size_t pageIndex = INVALID_PAGE;
	bool dummy;
	GetPageState(settings, pageIndex, dummy);
	return m_ShouldDraw = pageIndex != INVALID_PAGE;
}

void SetupFlow::GetPageState(const Settings& settings, size_t& currentPage, bool& hasNextPage) const
{
	hasNextPage = false;

	for (size_t i = (currentPage == INVALID_PAGE ? 0 : currentPage + 1); i < m_Pages.size(); i++)
	{
		if (m_Pages[i]->ValidateSettings(settings))
			continue;

		if (currentPage == INVALID_PAGE)
			currentPage = i;

		if (i != currentPage)
		{
			hasNextPage = true;
			return;
		}
	}
}

bool SetupFlow::OnDraw(Settings& settings, const IPage::DrawState& ds)
{
	if (!m_ShouldDraw)
		return false;

	bool hasNextPage = false;
	const auto lastPage = m_ActivePage;
	GetPageState(settings, m_ActivePage, hasNextPage);

	bool drewPage = false;
	if (m_ActivePage != INVALID_PAGE)
	{
		ImGui::PushTextWrapPos();

		auto page = m_Pages[m_ActivePage].get();
		if (page->WantsSetupText())
			ImGui::TextUnformatted("Welcome to TF2 Bot Detector! Some setup is required before first use."sv);
		else
			ImGui::NewLine();

		ImGui::NewLine();

		if (m_ActivePage != lastPage)
		{
			assert(!page->ValidateSettings(settings));
			page->Init(settings);
		}

		auto drawResult = page->OnDraw(ds);

		const bool wantsContinueButton = page->WantsContinueButton();
		if (wantsContinueButton)
			ImGui::NewLine();

		drewPage = true;

		const bool canCommit = page->CanCommit();
		ImGui::EnabledSwitch(canCommit, [&]
			{
				if ((wantsContinueButton && ImGui::Button(hasNextPage ? "Next >" : "Done")) ||
					drawResult == IPage::OnDrawResult::EndDrawing)
				{
					if (canCommit)
					{
						page->Commit(settings);
						settings.SaveFile();
					}

					m_ActivePage = INVALID_PAGE;
					if (!hasNextPage)
						m_ShouldDraw = false;
				}
			});
	}

	if (!drewPage && m_ActivePage == INVALID_PAGE)
		m_ShouldDraw = false;

	return drewPage || (m_ActivePage != INVALID_PAGE);
}
