#pragma once

#include "Config/Settings.h"
#include "ISetupFlowPage.h"

namespace tf2_bot_detector
{
	class BasicSettingsPage final : public ISetupFlowPage
	{
	public:
		ValidateSettingsResult ValidateSettings(const Settings& settings) const override;
		OnDrawResult OnDraw(const DrawState& ds) override;
		void Init(const InitState& is) override;

		bool CanCommit() const override;
		void Commit(Settings& settings);

	private:
		AutoDetectedSettings m_Settings;
	};
}
