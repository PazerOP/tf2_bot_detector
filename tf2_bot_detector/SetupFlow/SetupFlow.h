#pragma once

#include "ISetupFlowPage.h"

#include <memory>
#include <vector>

namespace tf2_bot_detector
{
	class RCONActionManager;
	class HijackActionManager;
	class Settings;

	class SetupFlow final
	{
	public:
		SetupFlow();

		// Returns true if the setup flow needs to draw.
		[[nodiscard]] bool OnUpdate(const Settings& settings);
		[[nodiscard]] bool OnDraw(Settings& settings, const ISetupFlowPage::DrawState& ds);

		bool ShouldDraw() const { return m_ShouldDraw; }

	private:
		bool m_ShouldDraw = false;
		std::vector<std::unique_ptr<ISetupFlowPage>> m_Pages;

		void GetPageState(const Settings& settings, size_t& currentPage, bool& hasNextPage) const;

		static constexpr size_t INVALID_PAGE = size_t(-1);
		size_t m_ActivePage = INVALID_PAGE;
	};
}
