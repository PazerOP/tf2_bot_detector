#include "UI/ImGui_TF2BotDetector.h"
#include "Filesystem.h"
#include "ISetupFlowPage.h"
#include "Platform/Platform.h"

#include <mh/algorithm/multi_compare.hpp>

#include <filesystem>
#include <fstream>

#undef DrawState

using namespace tf2_bot_detector;

namespace
{
	class CheckFaceitClosedPage final : public ISetupFlowPage
	{
	public:
		ValidateSettingsResult ValidateSettings(const Settings& settings) const override
		{
			return Processes::IsProcessRunning("faceitservice.exe") ? ValidateSettingsResult::TriggerOpen : ValidateSettingsResult::Success;
		}

		OnDrawResult OnDraw(const DrawState& ds) override
		{
			if (ValidateSettings(*ds.m_Settings) == ValidateSettingsResult::TriggerOpen)
			{
				ImGui::TextFmt("TF2 Bot Detector is not compatible with FACEIT anti-cheat. You must close it before continuing.\n\n(Check your tray icons for an orange shield, right click it, and click Exit.)");
				return OnDrawResult::ContinueDrawing;
			}
			else
			{
				return OnDrawResult::EndDrawing;
			}
		}

		void Init(const InitState&) override {}
		bool CanCommit() const override { return true; }
		void Commit(const CommitState& cs) override {}
		bool WantsSetupText() const { return false; }
		bool WantsContinueButton() const { return false; }
		SetupFlowPage GetPage() const override { return SetupFlowPage::CheckFaceitClosed; }
	};
}

namespace tf2_bot_detector
{
	std::unique_ptr<ISetupFlowPage> CreateCheckFaceitClosedPage()
	{
		return std::make_unique<CheckFaceitClosedPage>();
	}
}
