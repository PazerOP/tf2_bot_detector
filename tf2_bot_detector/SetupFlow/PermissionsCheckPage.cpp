#include "UI/ImGui_TF2BotDetector.h"
#include "Filesystem.h"
#include "ISetupFlowPage.h"
#include "Platform/Platform.h"

#include <mh/algorithm/multi_compare.hpp>

#include <filesystem>
#include <fstream>

using namespace tf2_bot_detector;

namespace
{
	class PermissionsCheckPage final : public ISetupFlowPage
	{
	public:
		ValidateSettingsResult ValidateSettings(const Settings& settings) const override;

		OnDrawResult OnDraw(const DrawState& ds) override
		{
			if (m_ValidationState == ValidationState::Unvalidated)
				Validate(IFilesystem::Get());

			if (m_ValidationState == ValidationState::ValidationSuccess)
				return OnDrawResult::EndDrawing;

			ImGui::TextFmt({ 1, 1, 0, 1 }, m_ValidationMessage);
			ImGui::NewLine();

			if (!Platform::IsInstalled())
			{
				ImGui::TextFmt("Sensors detect that this version of TF2 Bot Detector is a \"portable\" version (you"
					" downloaded a zip file and extracted it). Make sure you haven't extracted it in a place where"
					" it doesn't have write permissions (like Program Files). If you need help, try asking on Discord"
					" or creating an issue on GitHub (links are in the About menu, above).");
			}
			else
			{
				ImGui::TextFmt("This might be due to a problem with the TF2 Bot Detector installation. You should"
					" try reinstalling TF2 Bot Detector first. If the problem persists, try asking for help on Discord"
					" or creating an issue on GitHub (links are in the About menu, above).");
			}

			return OnDrawResult::ContinueDrawing;
		}

		void Init(const InitState&) override {}
		bool CanCommit() const override { return m_ValidationState == ValidationState::ValidationSuccess; }
		void Commit(const CommitState& cs) override {}
		bool WantsSetupText() const { return false; }
		bool WantsContinueButton() const { return false; }
		SetupFlowPage GetPage() const override { return SetupFlowPage::PermissionsCheck; }

		enum class ValidationState
		{
			Unvalidated,

			ValidationSuccess,
			ValidationFailure,
		};

	private:

		ValidationState m_ValidationState = ValidationState::Unvalidated;
		std::error_code m_ErrorCode;
		std::string m_ValidationMessage;

		void Validate(const IFilesystem& fs) try
		{
			assert(m_ValidationState == ValidationState::Unvalidated);
			m_ValidationState = ValidationState::ValidationFailure;
			m_ValidationMessage = "Validation of filesystem permissions failed";

			const auto cfgDir = fs.GetConfigDir();

			// Check that /cfg exists
			{
				bool exists = std::filesystem::exists(cfgDir, m_ErrorCode);
				if (m_ErrorCode)
				{
					m_ValidationMessage = mh::format("Filesystem error when checking status of {}: {}", cfgDir, m_ErrorCode);
					return;
				}

				if (!exists)
				{
					std::filesystem::create_directories(cfgDir, m_ErrorCode);
					if (m_ErrorCode)
					{
						m_ValidationMessage = mh::format("Filesystem error when creating {}: {}", cfgDir, m_ErrorCode);
						return;
					}
				}
			}

			// Try to create a file in cfg
			const auto testFile = cfgDir / ".perms_test";

			const auto DeleteFile = [&]()
			{
				std::filesystem::remove(testFile, m_ErrorCode);
				if (m_ErrorCode)
				{
					m_ValidationMessage = mh::format(
						"Filesystem error when trying to delete {}: {}", testFile, m_ErrorCode);
					return false;
				}

				return true;
			};

			if (!DeleteFile())
				return;

			// Create the file
			{
				std::ofstream file(testFile, std::ios::trunc | std::ios::binary);
				if (!file.good())
				{
					m_ValidationMessage = mh::format("Filesystem error when trying to create {}", testFile);
					return;
				}

				file << "hello world permissions test" << std::flush;
				if (!file.good())
				{
					m_ValidationMessage = mh::format("Filesystem error when writing to {}", testFile);
					return;
				}
			}

			if (!DeleteFile())
				return;

			m_ValidationMessage = "Validation success";
			m_ValidationState = ValidationState::ValidationSuccess;
		}
		catch (const std::exception& e)
		{
			LogException(MH_SOURCE_LOCATION_CURRENT(), e);
			m_ValidationMessage = mh::format("Exception: {}: {}\n\nPlease report this issue if possible.",
				typeid(e).name(), e.what());
		}
	};
}

MH_ENUM_REFLECT_BEGIN(PermissionsCheckPage::ValidationState)
	MH_ENUM_REFLECT_VALUE(Unvalidated)
	MH_ENUM_REFLECT_VALUE(ValidationSuccess)
	MH_ENUM_REFLECT_VALUE(ValidationFailure)
MH_ENUM_REFLECT_END()

auto PermissionsCheckPage::ValidateSettings(const Settings& settings) const -> ValidateSettingsResult
{
	switch (m_ValidationState)
	{
	default:
		LogError(MH_SOURCE_LOCATION_CURRENT(), "Unknown {}", mh::enum_fmt(m_ValidationState));

	case ValidationState::Unvalidated:
	case ValidationState::ValidationFailure:
		return ValidateSettingsResult::TriggerOpen;

	case ValidationState::ValidationSuccess:
		return ValidateSettingsResult::Success;
	}
}

namespace tf2_bot_detector
{
	std::unique_ptr<ISetupFlowPage> CreatePermissionsCheckPage()
	{
		return std::make_unique<PermissionsCheckPage>();
	}
}
