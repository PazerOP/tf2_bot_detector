#include "SetupFlow.h"
#include "Config/Settings.h"
#include "PathUtils.h"
#include "ImGui_TF2BotDetector.h"

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
			if (!settings.m_LocalSteamID.IsValid())
				return false;

			if (!TFDirectoryValidator(settings.m_TFDir))
				return false;

			return true;
		}

		bool ValidateSettings(const Settings& settings) const override
		{
			return InternalValidateSettings(settings);
		}

		void OnDraw() override
		{
			// 1. Steam ID
			{
				ImGui::TextUnformatted("1. Your Steam ID"sv);
				ImGui::NewLine();
				ImGui::Indent();
				ImGui::TextUnformatted("Your Steam ID is needed to identify who can be votekicked (same team) and who is on the other team. You can find your Steam ID using sites like steamidfinder.com.");

				InputTextSteamID("##SetupFlow_SteamID", m_Settings.m_LocalSteamID);

				ImGui::Unindent();
				ImGui::NewLine();
			}

			// 2. TF directory
			{
				ImGui::TextUnformatted("2. Location of your tf directory");
				ImGui::NewLine();
				ImGui::Indent();
				ImGui::TextUnformatted("Your tf directory is needed so this tool knows where to read console output from. It is also needed to automatically run commands in the game.");

				InputTextTFDir("##SetupFlow_TFDir", m_Settings.m_TFDir);

				ImGui::Unindent();
			}
		}

		void Init(const Settings& settings) override
		{
			m_Settings.m_TFDir = settings.m_TFDir;
			m_Settings.m_LocalSteamID = settings.m_LocalSteamID;
		}

		bool CanCommit() const override { return InternalValidateSettings(m_Settings); }
		void Commit(Settings& settings)
		{
			settings.m_TFDir = m_Settings.m_TFDir;
			settings.m_LocalSteamID = m_Settings.m_LocalSteamID;
		}

	private:
		struct
		{
			std::filesystem::path m_TFDir;
			SteamID m_LocalSteamID;

		} m_Settings;
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

			return true;
		}

		bool ValidateSettings(const Settings& settings) const override
		{
			return InternalValidateSettings(settings);
		}

		void OnDraw() override
		{
			ImGui::TextUnformatted("This tool can optionally connect to the internet to automatically update.");

			ImGui::Indent();
			{
				if (bool allow = m_Settings.m_AllowInternetUsage.value_or(false); ImGui::Checkbox("Allow Internet Connectivity", &allow))
					m_Settings.m_AllowInternetUsage = allow;

				if (m_Settings.m_AllowInternetUsage.value_or(false))
				{
					ScopeGuards::TextColor col({ 1, 1, 0, 1 });
					ImGui::TextUnformatted("If you use antivirus software, connecting to the internet may trigger warnings.");
				}
			}
			ImGui::Unindent();
			ImGui::NewLine();

			{
				const bool internetAllowed = m_Settings.m_AllowInternetUsage.value_or(false);
				std::optional<ScopeGuards::GlobalAlpha> globalAlpha;
				if (!internetAllowed)
				{
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					globalAlpha.emplace(0.65f);
				}

				ImGui::BeginGroup();
				ImGui::TextUnformatted("This tool can also check for updated functionality and bugfixes on startup.");
				ImGui::Indent();
				{
					const char* friendlyText = "<UNKNOWN>";
					static constexpr char FRIENDLY_TEXT_DISABLED[] = "Disable automatic update checks";
					static constexpr char FRIENDLY_TEXT_PREVIEW[] = "Notify about new preview releases";
					static constexpr char FRIENDLY_TEXT_STABLE[] = "Notify about new stable releases";

					switch (internetAllowed ? m_Settings.m_ProgramUpdateCheckMode : ProgramUpdateCheckMode::Disabled)
					{
					case ProgramUpdateCheckMode::Disabled: friendlyText = FRIENDLY_TEXT_DISABLED; break;
					case ProgramUpdateCheckMode::Previews: friendlyText = FRIENDLY_TEXT_PREVIEW; break;
					case ProgramUpdateCheckMode::Releases: friendlyText = FRIENDLY_TEXT_STABLE; break;
					case ProgramUpdateCheckMode::Unknown:  friendlyText = "Select an option"; break;
					}

					if (ImGui::BeginCombo("##SetupFlow_UpdateCheckingMode", friendlyText))
					{
						if (ImGui::Selectable(FRIENDLY_TEXT_DISABLED))
							m_Settings.m_ProgramUpdateCheckMode = ProgramUpdateCheckMode::Disabled;
						if (ImGui::Selectable(FRIENDLY_TEXT_STABLE))
							m_Settings.m_ProgramUpdateCheckMode = ProgramUpdateCheckMode::Releases;
						if (ImGui::Selectable(FRIENDLY_TEXT_PREVIEW))
							m_Settings.m_ProgramUpdateCheckMode = ProgramUpdateCheckMode::Previews;

						ImGui::EndCombo();
					}

					if (m_Settings.m_ProgramUpdateCheckMode == ProgramUpdateCheckMode::Disabled)
						ImGui::TextUnformatted("You can always check for updates manually via the Help menu.");
				}
				ImGui::Unindent();
				ImGui::EndGroup();

				if (!internetAllowed)
				{
					globalAlpha.reset();
					ImGui::PopItemFlag();
					ImGui::SetHoverTooltip("Requires \"Allow Internet Connectivity\"");
				}
			}
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

		page->OnDraw();

		ImGui::NewLine();

		drewPage = true;

		{
			const auto canCommit = page->CanCommit();
			std::optional<ScopeGuards::GlobalAlpha> disabled;
			if (!canCommit)
			{
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				disabled.emplace(0.5f);
			}

			if (ImGui::Button(hasNextPage ? "Next >" : "Done"))
			{
				page->Commit(settings);
				settings.SaveFile();
				m_ActivePage = INVALID_PAGE;
				if (!hasNextPage)
					m_ShouldDraw = false;
			}

			if (!canCommit)
				ImGui::PopItemFlag();
		}
	}

	return drewPage || (m_ActivePage != INVALID_PAGE);
}
