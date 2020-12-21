#include "Config/Settings.h"
#include "Platform/Platform.h"
#include "SetupFlow/ISetupFlowPage.h"
#include "UI/ImGui_TF2BotDetector.h"
#include "Log.h"
#include "ReleaseChannel.h"
#include "UpdateManager.h"

#include <mh/algorithm/multi_compare.hpp>
#include <mh/raii/scope_exit.hpp>

using namespace tf2_bot_detector;

namespace
{
	class UpdateCheckPage final : public ISetupFlowPage
	{
	public:
		ValidateSettingsResult ValidateSettings(const Settings& settings) const override;
		OnDrawResult OnDraw(const DrawState& ds) override;
		void Init(const InitState& is) override;

		bool CanCommit() const override;
		void Commit(const CommitState& cs) override {}

		bool WantsSetupText() const override { return false; }
		bool WantsContinueButton() const override { return false; }
		SetupFlowPage GetPage() const override { return SetupFlowPage::UpdateCheck; }

	private:
		mh::status_reader<UpdateStatus> m_StatusReader;
		bool m_HasChangedReleaseChannel = false;
		bool m_UpdateButtonPressed = false;
		bool m_DrawnOnce = false;
	};

	auto UpdateCheckPage::ValidateSettings(const Settings& settings) const -> ValidateSettingsResult
	{
		if (!settings.GetHTTPClient())
			return ValidateSettingsResult::Success;

		const auto status = m_StatusReader.get();

		switch (status.m_Status)
		{
		case UpdateStatus::Unknown:
		case UpdateStatus::CheckQueued:
		case UpdateStatus::Checking:
		case UpdateStatus::UpdateAvailable:
		case UpdateStatus::UpdateToolRequired:
		case UpdateStatus::UpdateToolDownloading:
		case UpdateStatus::Downloading:
		case UpdateStatus::Updating:
		{
			if (m_DrawnOnce)
				return ValidateSettingsResult::Success;
			else
				return ValidateSettingsResult::TriggerOpen;
		}

		case UpdateStatus::DownloadFailed:
		case UpdateStatus::DownloadSuccess:
		case UpdateStatus::UpdateFailed:
		case UpdateStatus::UpdateSuccess:
		case UpdateStatus::UpdateCheckDisabled:
		case UpdateStatus::StateSwitchFailure:
		case UpdateStatus::InternetAccessDisabled:
		case UpdateStatus::UpdateToolDownloadSuccess:
		case UpdateStatus::UpdateToolDownloadFailed:
		case UpdateStatus::CheckFailed:
		case UpdateStatus::UpToDate:
			return ValidateSettingsResult::Success;
		}

		assert(!"Should never get here...");
		return ValidateSettingsResult::Success;
	}

	auto UpdateCheckPage::OnDraw(const DrawState& ds) -> OnDrawResult
	{
		if (!m_StatusReader.has_value())
		{
			assert(false);
			LogWarning(MH_SOURCE_LOCATION_CURRENT(), "status reader empty!");
			return OnDrawResult::EndDrawing;
		}

		m_DrawnOnce = true;

		const auto updateStatus = m_StatusReader.get();

		const IAvailableUpdate* update = ds.m_UpdateManager->GetAvailableUpdate();

		ImGui::TextFmt("Update Check");
		ImGui::NewLine();
		ImGui::Separator();

		ImGui::NewLine();

		if (auto rc = ds.m_Settings->m_ReleaseChannel; Combo("##SetupFlow_UpdateCheckingMode", rc) && rc.has_value())
		{
			DebugLog(MH_SOURCE_LOCATION_CURRENT(),
				"Update check mode changed to {}, queueing update check and saving settings...",
				mh::enum_fmt(rc.value()));

			ds.m_Settings->m_ReleaseChannel = rc;
			ds.m_Settings->SaveFile();
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

		// Early out
		switch (updateStatus.m_Status)
		{
		case UpdateStatus::UpdateCheckDisabled:
		{
			if (!m_HasChangedReleaseChannel)
			{
				DebugLog(MH_SOURCE_LOCATION_CURRENT(),
					"Update check disabled, and release channel unchanged. Continuing...");
				return OnDrawResult::EndDrawing;
			}

			break;
		}

		case UpdateStatus::InternetAccessDisabled:
			DebugLog(MH_SOURCE_LOCATION_CURRENT(), "Internet access disabled. Continuing...");
			return OnDrawResult::EndDrawing;

		default:
			break;
		}

		// Message color
		ImVec4 msgColor = { 1, 0, 1, 1 };
		switch (updateStatus.m_Status)
		{
			// Red
		case UpdateStatus::StateSwitchFailure:
		case UpdateStatus::CheckFailed:
		case UpdateStatus::UpdateToolDownloadFailed:
		case UpdateStatus::DownloadFailed:
		case UpdateStatus::UpdateFailed:
			msgColor = { 1, 0, 0, 1 };
			break;

			// Green
		case UpdateStatus::UpToDate:
		case UpdateStatus::UpdateSuccess:
			msgColor = { 0, 1, 0, 1 };
			break;

			// Cyan
		case UpdateStatus::UpdateAvailable:
			msgColor = { 0, 1, 1, 1 };
			break;

			// Yellow
		case UpdateStatus::Unknown:
		case UpdateStatus::UpdateCheckDisabled:
		case UpdateStatus::InternetAccessDisabled:
			msgColor = { 1, 1, 0, 1 };
			break;

			// White
		case UpdateStatus::CheckQueued:
		case UpdateStatus::Checking:
		case UpdateStatus::UpdateToolRequired:
		case UpdateStatus::UpdateToolDownloading:
		case UpdateStatus::UpdateToolDownloadSuccess:
		case UpdateStatus::Downloading:
		case UpdateStatus::DownloadSuccess:
		case UpdateStatus::Updating:
			msgColor = { 1, 1, 1, 1 };
			break;
		}

		if (updateStatus.m_Status == UpdateStatus::UpdateAvailable)
		{
			ImGui::TextFmt({ 0, 1, 1, 1 }, "Update available: v{} {:v} (current version v{})",
				update->m_BuildInfo.m_Version, mh::enum_fmt(update->m_BuildInfo.m_ReleaseChannel), VERSION);

			if (update->m_BuildInfo.m_ReleaseChannel == ReleaseChannel::Nightly)
			{
				ImGui::NewLine();
				ImGui::TextFmt({ 1, 1, 0, 1 }, "Reminder: Nightly builds can be unstable and/or unusable.");
			}

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
		}
		else if (updateStatus.m_Status == UpdateStatus::Unknown)
		{
			ImGui::TextFmt(msgColor, "UNKNOWN UPDATE STATUS: {}", std::quoted(updateStatus.m_Message));
		}
		else
		{
			assert(!updateStatus.m_Message.empty());
			ImGui::TextFmt(msgColor, updateStatus.m_Message);
		}

#if 0
		switch (updateStatus.m_Status)
		{
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
		}
#endif

		// Continue button mode
		switch (updateStatus.m_Status)
		{
		case UpdateStatus::Unknown:
		case UpdateStatus::CheckFailed:
		case UpdateStatus::StateSwitchFailure:
		case UpdateStatus::UpdateAvailable:
		{
			if (ImGui::Button("Continue without updating >"))
				return OnDrawResult::EndDrawing;

			break;
		}

		case UpdateStatus::UpToDate:
		case UpdateStatus::UpdateCheckDisabled:
		case UpdateStatus::InternetAccessDisabled:
		case UpdateStatus::UpdateSuccess:
		case UpdateStatus::UpdateToolDownloadFailed:
		case UpdateStatus::DownloadFailed:
		case UpdateStatus::UpdateFailed:
		{
			if (ImGui::Button("Continue >"))
				return OnDrawResult::EndDrawing;

			break;
		}

		case UpdateStatus::CheckQueued:
		case UpdateStatus::Checking:
		case UpdateStatus::UpdateToolRequired:
		case UpdateStatus::UpdateToolDownloading:
		case UpdateStatus::UpdateToolDownloadSuccess:
		case UpdateStatus::Downloading:
		case UpdateStatus::DownloadSuccess:
		case UpdateStatus::Updating:
			break;
		}

		// Continue automatically if we haven't changed any settings and we've determined we're up to date
		if (updateStatus.m_Status == UpdateStatus::UpToDate && !m_HasChangedReleaseChannel)
			return OnDrawResult::EndDrawing;

		return OnDrawResult::ContinueDrawing;
	}

	void UpdateCheckPage::Init(const InitState& is)
	{
		assert(is.m_UpdateManager);
		if (is.m_UpdateManager && !m_StatusReader.has_value())
			m_StatusReader = is.m_UpdateManager->GetUpdateStatus();

		m_HasChangedReleaseChannel = false;
		m_UpdateButtonPressed = false;
	}

	bool UpdateCheckPage::CanCommit() const
	{
		return mh::none_eq(m_StatusReader.get().m_Status
			, UpdateStatus::CheckQueued
			, UpdateStatus::Checking
			, UpdateStatus::UpdateToolRequired
			, UpdateStatus::UpdateToolDownloading
			, UpdateStatus::Downloading
			, UpdateStatus::DownloadSuccess
			, UpdateStatus::Updating
			);
	}
}

namespace tf2_bot_detector
{
	std::unique_ptr<ISetupFlowPage> CreateUpdateCheckPage()
{
	return std::make_unique<UpdateCheckPage>();
}
}
