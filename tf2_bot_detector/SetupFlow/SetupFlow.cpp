#include "SetupFlow.h"
#include "BasicSettingsPage.h"
#include "ChatWrappersGeneratorPage.h"
#include "ChatWrappersVerifyPage.h"
#include "NetworkSettingsPage.h"
#include "TF2CommandLinePage.h"
#include "Log.h"
#include "Config/Settings.h"
#include "ImGui_TF2BotDetector.h"
#include "Platform/Platform.h"
#include "Version.h"

#include <imgui.h>
#include <mh/text/string_insertion.hpp>

#include <string_view>

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace tf2_bot_detector;

SetupFlow::SetupFlow()
{
	m_Pages.push_back(std::make_unique<BasicSettingsPage>());
	m_Pages.push_back(std::make_unique<ChatWrappersGeneratorPage>());
	m_Pages.push_back(std::make_unique<NetworkSettingsPage>());
	//m_Pages.push_back(std::make_unique<UseRCONCmdLinePage>());
	m_Pages.push_back(std::make_unique<TF2CommandLinePage>());
	m_Pages.push_back(std::make_unique<ChatWrappersVerifyPage>());
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

	for (size_t i = 0; i < m_Pages.size(); i++)
	{
		if (m_Pages[i]->ValidateSettings(settings))
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
			ImGui::TextUnformatted("Welcome to TF2 Bot Detector! Some setup is required before first use."sv);

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
