#include "ISetupFlowPage.h"
#include "UI/ImGui_TF2BotDetector.h"
#include "Platform/Platform.h"

#undef DrawState

using namespace tf2_bot_detector;

namespace
{
	class CheckSteamOpenPage final : public ISetupFlowPage
	{
	public:
		ValidateSettingsResult ValidateSettings(const Settings& settings) const override;

		OnDrawResult OnDraw(const DrawState& ds) override;

		void Init(const InitState& is) override;
		bool CanCommit() const override;
		void Commit(const CommitState& cs) override;

		bool WantsSetupText() const override { return false; }
		bool WantsContinueButton() const override { return m_CanContinue; }

		SetupFlowPage GetPage() const override { return SetupFlowPage::CheckSteamOpen; }

	private:
		bool m_CanContinue = false;
	};

	auto CheckSteamOpenPage::ValidateSettings(const Settings& settings) const -> ValidateSettingsResult
	{
		if (!Platform::Processes::IsSteamRunning())
			return ValidateSettingsResult::TriggerOpen;

		return ValidateSettingsResult::Success;
	}

	auto CheckSteamOpenPage::OnDraw(const DrawState& ds) -> OnDrawResult
	{
		ImGui::Text("Steam must be open to use TF2 Bot Detector.");

		const bool isSteamRunning = Platform::Processes::IsSteamRunning();
		m_CanContinue = isSteamRunning;
		if (isSteamRunning)
		{
			m_CanContinue = true;

			if (Platform::GetCurrentActiveSteamID().IsValid())
			{
				return OnDrawResult::EndDrawing;
			}
			else
			{
				ImGui::NewLine();
				ImGui::Text("Steam is open, but it might not be logged into an account yet.");
			}
		}

		return OnDrawResult::ContinueDrawing;
	}
	void CheckSteamOpenPage::Init(const InitState& is)
	{
	}
	bool CheckSteamOpenPage::CanCommit() const
	{
		return m_CanContinue;
	}
	void CheckSteamOpenPage::Commit(const CommitState& cs)
	{
	}
}

namespace tf2_bot_detector
{
	std::unique_ptr<ISetupFlowPage> CreateCheckSteamOpenPage()
	{
		return std::make_unique<CheckSteamOpenPage>();
	}
}
