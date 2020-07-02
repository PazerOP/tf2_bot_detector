#pragma once

#include "Config/Settings.h"
#include "ISetupFlowPage.h"

namespace tf2_bot_detector
{
	class BasicSettingsPage final : public ISetupFlowPage
	{
	public:
		bool ValidateSettings(const Settings& settings) const override;
		OnDrawResult OnDraw(const DrawState& ds) override;
		void Init(const Settings& settings) override;

		bool CanCommit() const override;
		void Commit(Settings& settings);

	private:
		AutoDetectedSettings m_Settings;
	};
}
