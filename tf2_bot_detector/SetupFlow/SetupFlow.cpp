#include "SetupFlow.h"
#include "BasicSettingsPage.h"
#include "ChatWrappersGeneratorPage.h"
#include "NetworkSettingsPage.h"
#include "TF2CommandLinePage.h"
#include "Log.h"
#include "Config/Settings.h"
#include "PathUtils.h"
#include "ImGui_TF2BotDetector.h"
#include "PlatformSpecific/Steam.h"
#include "Version.h"

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

namespace
{
#if 0
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
#endif
}

SetupFlow::SetupFlow()
{
	m_Pages.push_back(std::make_unique<BasicSettingsPage>());
	m_Pages.push_back(std::make_unique<ChatWrappersGeneratorPage>());
	m_Pages.push_back(std::make_unique<NetworkSettingsPage>());
	//m_Pages.push_back(std::make_unique<UseRCONCmdLinePage>());
	m_Pages.push_back(std::make_unique<TF2CommandLinePage>());
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

bool SetupFlow::OnDraw(Settings& settings, const ISetupFlowPage::DrawState& ds)
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
					drawResult == ISetupFlowPage::OnDrawResult::EndDrawing)
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
