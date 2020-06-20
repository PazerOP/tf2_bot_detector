#include "SetupFlow.h"
#include "Config/Settings.h"
#include "PathUtils.h"
#include "ImGui_TF2BotDetector.h"
#include "PlatformSpecific/Steam.h"
#include "Version.h"

#include <imgui.h>
#include <imgui_desktop/ScopeGuards.h>
#include <misc/cpp/imgui_stdlib.h>
#include <mh/text/string_insertion.hpp>

#include <optional>
#include <string_view>

using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace tf2_bot_detector;
namespace ScopeGuards = ImGuiDesktop::ScopeGuards;

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

		OnDrawResult OnDraw() override
		{
			ImGui::TextUnformatted("Due to your configuration, some settings could not be automatically detected.");
			ImGui::NewLine();

			// 1. Steam directory
			bool any = false;
			if (GetCurrentSteamDir().empty())
			{
				any = true;
				ImGui::TextUnformatted("Location of your Steam directory");
				ImGui::NewLine();
				ScopeGuards::Indent indent;
				ImGui::TextUnformatted("Your Steam directory is needed to execute commands in TF2, read console logs, and determine your current Steam ID.");

				InputTextSteamDirOverride("##SetupFlow_SteamDir", m_Settings.m_SteamDirOverride, nullptr);

				ImGui::NewLine();
			}

			if (FindTFDir(m_Settings.GetSteamDir()).empty())
			{
				any = true;
				ImGui::TextUnformatted("Location of your tf directory");
				ImGui::NewLine();
				ImGui::Indent();
				ImGui::TextUnformatted("Your tf directory is needed so this tool knows where to read console output from. It is also needed to automatically run commands in the game.");

				InputTextTFDirOverride("##SetupFlow_TFDir", m_Settings.m_TFDirOverride, nullptr);

				ImGui::Unindent();
			}

			// 2. Steam ID
			if (!GetCurrentActiveSteamID().IsValid())
			{
				any = true;
				ImGui::TextUnformatted("Your Steam ID"sv);
				ImGui::NewLine();
				ImGui::Indent();
				ImGui::TextUnformatted("Your Steam ID is needed to identify who can be votekicked (same team) and who is on the other team. You can find your Steam ID using sites like steamidfinder.com.");

				InputTextSteamIDOverride("##SetupFlow_SteamID", m_Settings.m_LocalSteamIDOverride, nullptr);

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

		OnDrawResult OnDraw() override
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
		void Commit(Settings& settings)
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
}

SetupFlow::SetupFlow()
{
	m_Pages.push_back(std::make_unique<BasicSettingsPage>());
	m_Pages.push_back(std::make_unique<NetworkSettingsPage>());
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

bool SetupFlow::OnDraw(Settings& settings)
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

		ImGui::TextUnformatted("Welcome to TF2 Bot Detector! Some setup is required before first use."sv);

		ImGui::NewLine();

		auto page = m_Pages[m_ActivePage].get();

		if (m_ActivePage != lastPage)
		{
			assert(!page->ValidateSettings(settings));
			page->Init(settings);
		}

		auto drawResult = page->OnDraw();

		ImGui::NewLine();

		drewPage = true;

		ImGui::EnabledSwitch(page->CanCommit(), [&]
			{
				if (ImGui::Button(hasNextPage ? "Next >" : "Done") || drawResult == IPage::OnDrawResult::EndDrawing)
				{
					page->Commit(settings);
					settings.SaveFile();
					m_ActivePage = INVALID_PAGE;
					if (!hasNextPage)
						m_ShouldDraw = false;
				}
			});
	}

	return drewPage || (m_ActivePage != INVALID_PAGE);
}
