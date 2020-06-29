#pragma once

#include "ISetupFlowPage.h"

namespace tf2_bot_detector
{
	class RCONConnectionPage final : public ISetupFlowPage
	{
	public:
		[[nodiscard]] bool ValidateSettings(const Settings& settings) const override;
		[[nodiscard]] OnDrawResult OnDraw(const DrawState& ds) override;

		void Init(const Settings& settings) override;
		bool CanCommit() const override;
		void Commit(Settings& settings) override;
	};
}
