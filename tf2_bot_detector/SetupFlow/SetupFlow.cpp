#include "SetupFlow.h"
#include "Config/Settings.h"
#include "Platform/Platform.h"
#include "UI/ImGui_TF2BotDetector.h"
#include "BasicSettingsPage.h"
#include "ChatWrappersGeneratorPage.h"
#include "ChatWrappersVerifyPage.h"
#include "NetworkSettingsPage.h"
#include "TF2CommandLinePage.h"
#include "Log.h"
#include "Version.h"

#include <imgui.h>
#include <mh/algorithm/algorithm.hpp>
#include <mh/text/string_insertion.hpp>

#include <string_view>

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace tf2_bot_detector;

namespace tf2_bot_detector
{
	std::unique_ptr<ISetupFlowPage> CreateAddonManagerPage();
	std::unique_ptr<ISetupFlowPage> CreateUpdateCheckPage();
	std::unique_ptr<ISetupFlowPage> CreatePermissionsCheckPage();
	std::unique_ptr<ISetupFlowPage> CreateCheckSteamOpenPage();
}

SetupFlow::SetupFlow()
{
	// order unimportant, see SetupFlowPage enum
	m_Pages.push_back(CreatePermissionsCheckPage());
	m_Pages.push_back(CreateCheckSteamOpenPage());
	m_Pages.push_back(std::make_unique<BasicSettingsPage>());
	m_Pages.push_back(std::make_unique<NetworkSettingsPage>());
	m_Pages.push_back(CreateUpdateCheckPage());
	m_Pages.push_back(CreateAddonManagerPage());
	m_Pages.push_back(std::make_unique<ChatWrappersGeneratorPage>());
	m_Pages.push_back(std::make_unique<TF2CommandLinePage>());
	m_Pages.push_back(std::make_unique<ChatWrappersVerifyPage>());
	// order unimportant, see SetupFlowPage enum

	mh::sort(m_Pages, [](const std::unique_ptr<ISetupFlowPage>& lhs, const std::unique_ptr<ISetupFlowPage>& rhs)
		{
			assert(lhs->GetPage() != rhs->GetPage());
			return lhs->GetPage() < rhs->GetPage();
		});
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

	for (size_t i = 0; i < m_Pages.size(); i++)
	{
		if (m_Pages[i]->ValidateSettings(settings) != ISetupFlowPage::ValidateSettingsResult::TriggerOpen)
			continue;

		if (currentPage == INVALID_PAGE || i < currentPage)
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

		ImGui::NewLine();

		if (page->WantsSetupText())
			ImGui::TextFmt("Welcome to TF2 Bot Detector! Some setup is required before first use."sv);

		if (m_ActivePage != lastPage)
		{
			assert(page->ValidateSettings(settings) == ISetupFlowPage::ValidateSettingsResult::TriggerOpen);

			ISetupFlowPage::InitState is(settings);
			is.m_UpdateManager = ds.m_UpdateManager;
			page->Init(is);
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
						ISetupFlowPage::CommitState cs(settings);
						cs.m_UpdateManager = ds.m_UpdateManager;
						page->Commit(cs);
						settings.SaveFile();
					}

					// If this assertion is hit, the page is reporting that it can commit, but we are failing validation.
					// This means that we'll be closing and reopening the page every frame.
					assert(page->ValidateSettings(settings) != ISetupFlowPage::ValidateSettingsResult::TriggerOpen);

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

SetupFlowPage SetupFlow::GetCurrentPage() const
{
	if (m_ActivePage == INVALID_PAGE)
		return SetupFlowPage::Invalid;

	return m_Pages.at(m_ActivePage)->GetPage();
}
