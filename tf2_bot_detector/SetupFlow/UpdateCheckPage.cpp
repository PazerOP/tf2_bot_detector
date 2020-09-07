#include "Config/Settings.h"
#include "Platform/Platform.h"
#include "SetupFlow/ISetupFlowPage.h"
#include "UI/ImGui_TF2BotDetector.h"
#include "Log.h"
#include "ReleaseChannel.h"
#include "UpdateManager.h"

#include <mh/algorithm/multi_compare.hpp>

using namespace tf2_bot_detector;

namespace
{
	class UpdateCheckPage final : public ISetupFlowPage
	{
	public:
		ValidateSettingsResult ValidateSettings(const Settings& settings) const override;
		OnDrawResult OnDraw(const DrawState& ds) override;
		void Init(const Settings& settings) override;

		bool CanCommit() const override;
		void Commit(Settings& settings);

		bool WantsSetupText() const override { return false; }
		bool WantsContinueButton() const override { return false; }

	private:
		UpdateStatus m_LastUpdateStatus = UpdateStatus::Unknown;
		bool m_HasChangedReleaseChannel = false;
		bool m_HasCheckedForUpdate = false;
		bool m_UpdateButtonPressed = false;
	};

	auto UpdateCheckPage::ValidateSettings(const Settings& settings) const -> ValidateSettingsResult
	{
		if (m_HasCheckedForUpdate)
			return ValidateSettingsResult::Success;
		if (!settings.GetHTTPClient())
			return ValidateSettingsResult::Success;
		if (settings.m_ReleaseChannel.value_or(ReleaseChannel::None) == ReleaseChannel::None)
			return ValidateSettingsResult::Success;

		return ValidateSettingsResult::TriggerOpen;
	}

	auto UpdateCheckPage::OnDraw(const DrawState& ds) -> OnDrawResult
	{
		m_HasCheckedForUpdate = true;

		const UpdateStatus updateStatus = m_LastUpdateStatus = ds.m_UpdateManager->GetUpdateStatus();
		const IAvailableUpdate* update = ds.m_UpdateManager->GetAvailableUpdate();

		ImGui::TextFmt("Update Check");
		ImGui::NewLine();
		ImGui::Separator();

		ImGui::NewLine();

		if (auto rc = ds.m_Settings->m_ReleaseChannel; Combo("##SetupFlow_UpdateCheckingMode", rc))
		{
			ds.m_Settings->m_ReleaseChannel = rc;
			ds.m_UpdateManager->QueueUpdateCheck();
			m_HasChangedReleaseChannel = true;
		}

		ImGui::NewLine();

		enum class ContinueButtonMode
		{
			None,
			Continue,
			ContinueWithoutUpdating,

		} continueButtonMode = ContinueButtonMode::None;

		switch (updateStatus)
		{
		case UpdateStatus::Unknown:
			ImGui::TextFmt({ 1, 1, 0, 1 }, "Unknown");
			continueButtonMode = ContinueButtonMode::ContinueWithoutUpdating;
			break;

		case UpdateStatus::UpdateCheckDisabled:
		{
			ImGui::TextFmt("Automatic update checks disabled by user");
			if (!m_HasChangedReleaseChannel)
				return OnDrawResult::EndDrawing;

			ImGui::NewLine();
			continueButtonMode = ContinueButtonMode::Continue;
			break;
		}
		case UpdateStatus::InternetAccessDisabled:
			ImGui::TextFmt("Internet connectivity disabled by user");
			return OnDrawResult::EndDrawing;

		case UpdateStatus::CheckQueued:
			ImGui::TextFmt("Update check queued...");
			break;
		case UpdateStatus::Checking:
			ImGui::TextFmt("Checking for updates...");
			break;

		case UpdateStatus::CheckFailed:
			ImGui::TextFmt({ 1, 1, 0, 1 }, "Update check failed");
			continueButtonMode = ContinueButtonMode::ContinueWithoutUpdating;
			ImGui::NewLine();
			break;
		case UpdateStatus::UpToDate:
		{
			ImGui::TextFmt({ 0, 1, 0, 1 }, "Up to date (v{} {:v})", VERSION,
				mh::enum_fmt(ds.m_Settings->m_ReleaseChannel.value_or(ReleaseChannel::Public)));

			if (!m_HasChangedReleaseChannel)
				return OnDrawResult::EndDrawing;

			ImGui::NewLine();
			continueButtonMode = ContinueButtonMode::Continue;

			break;
		}
		case UpdateStatus::UpdateAvailable:
		{
			ImGui::TextFmt({ 0, 1, 1, 1 }, "Update available: v{} {:v} (current version v{})",
				update->m_BuildInfo.m_Version, mh::enum_fmt(update->m_BuildInfo.m_ReleaseChannel), VERSION);

			ImGui::NewLine();

			ImGui::EnabledSwitch(update->CanSelfUpdate(), [&]
				{
					ImGui::EnabledSwitch(!m_UpdateButtonPressed, [&]
						{
							if (ImGui::Button("Update Now"))
							{
								update->BeginSelfUpdate();
							}

						}, "Update in progress...");

				}, "Self-update not currently available. Visit GitHub to download and install the new version.");

			ImGui::SameLine();

			ImGui::EnabledSwitch(!update->m_BuildInfo.m_GitHubURL.empty(), [&]
				{
					if (ImGui::Button("View on GitHub"))
						Shell::OpenURL(update->m_BuildInfo.m_GitHubURL);

				}, "Unable to determine GitHub URL");

			ImGui::SameLine();
			continueButtonMode = ContinueButtonMode::ContinueWithoutUpdating;
			break;
		}

		case UpdateStatus::Updating:
			ImGui::TextFmt("Updating...");
			break;

		case UpdateStatus::UpdateFailed:
			ImGui::TextFmt({ 1, 0, 0, 1 }, "Update failed");
			break;
		case UpdateStatus::UpdateSuccess:
			ImGui::TextFmt({ 0, 1, 0, 1 }, "Update succeeded. Restart TF2 Bot Detector to apply.");
			break;

		default:
			LogError(MH_SOURCE_LOCATION_CURRENT(), "Unknown UpdateStatus {}", mh::enum_fmt(updateStatus));
			ImGui::TextFmt({ 1, 0, 0, 1 }, "Unexpected({})", mh::enum_fmt(updateStatus));
			break;
		}

		if (continueButtonMode == ContinueButtonMode::ContinueWithoutUpdating &&
			ImGui::Button("Continue without updating >"))
		{
			return OnDrawResult::EndDrawing;
		}
		if (continueButtonMode == ContinueButtonMode::Continue &&
			ImGui::Button("Continue >"))
		{
			return OnDrawResult::EndDrawing;
		}

		return OnDrawResult::ContinueDrawing;
	}

	void UpdateCheckPage::Init(const Settings& settings)
	{
		m_HasChangedReleaseChannel = false;
		m_UpdateButtonPressed = false;
	}

	bool UpdateCheckPage::CanCommit() const
	{
		if (!m_HasCheckedForUpdate)
			return false;

		return mh::none_eq(m_LastUpdateStatus
			, UpdateStatus::CheckQueued
			, UpdateStatus::Checking
			, UpdateStatus::Updating
			);
	}

	void UpdateCheckPage::Commit(Settings& settings)
	{
	}
}

namespace tf2_bot_detector
{
	std::unique_ptr<ISetupFlowPage> CreateUpdateCheckPage()
	{
		return std::make_unique<UpdateCheckPage>();
	}
}
