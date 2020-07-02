#pragma once

#include "Config/Settings.h"
#include "ISetupFlowPage.h"

#include <optional>

namespace tf2_bot_detector
{
	class NetworkSettingsPage final : public ISetupFlowPage
	{
	public:
		bool ValidateSettings(const Settings& settings) const override;
		OnDrawResult OnDraw(const DrawState& ds) override;
		void Init(const Settings& settings) override;
		bool CanCommit() const override;
		void Commit(Settings& settings) override;

	private:
		struct
		{
			std::optional<bool> m_AllowInternetUsage;
			ProgramUpdateCheckMode m_ProgramUpdateCheckMode = ProgramUpdateCheckMode::Unknown;

		} m_Settings;
	};
}
