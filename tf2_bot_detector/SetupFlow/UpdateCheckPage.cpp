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
		void Init(const Settings& settings) override;

		bool CanCommit() const override;
		void Commit(Settings& settings);

		bool WantsSetupText() const override { return false; }
		bool WantsContinueButton() const override { return false; }

	private:
		mh::status_reader<UpdateStatus> m_StatusReader;
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

		if (!m_StatusReader.has_value())
			m_StatusReader = ds.m_UpdateManager->GetUpdateStatus();

		const auto updateStatus = m_StatusReader.get();

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

		// Early out
		switch (updateStatus.m_Status)
		{
		case UpdateStatus::UpdateCheckDisabled:
		{
			if (!m_HasChangedReleaseChannel)
				return OnDrawResult::EndDrawing;

			break;
		}

		case UpdateStatus::InternetAccessDisabled:
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
