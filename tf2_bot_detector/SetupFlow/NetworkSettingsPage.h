#pragma once

#include "Config/Settings.h"
#include "ISetupFlowPage.h"

#include <optional>

namespace tf2_bot_detector
{
	enum class ReleaseChannel;

	class NetworkSettingsPage final : public ISetupFlowPage
	{
	public:
		ValidateSettingsResult ValidateSettings(const Settings& settings) const override;
		OnDrawResult OnDraw(const DrawState& ds) override;
		void Init(const Settings& settings) override;
		bool CanCommit() const override;
		void Commit(Settings& settings) override;

	private:
		struct
		{
			std::optional<bool> m_AllowInternetUsage;
			std::optional<ReleaseChannel> m_ReleaseChannel;

		} m_Settings;
	};
}
