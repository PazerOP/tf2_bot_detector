#pragma once

#include "Clock.h"
#include "IConsoleLineListener.h"

#include <map>
#include <memory>

namespace tf2_bot_detector
{
	class IAction;

	class ActionManager final : public IConsoleLineListener
	{
	public:
		~ActionManager();

		void QueueAction(std::unique_ptr<IAction>&& action);
		void Update();

	private:
		void OnConsoleLineParsed(IConsoleLine& line) override;

		static constexpr duration_t UPDATE_INTERVAL = std::chrono::milliseconds(100);

		struct QueuedAction
		{
			std::unique_ptr<IAction> m_Action;
			uint32_t m_UpdateIndex;
		};

		time_point_t m_LastUpdateTime = clock_t::now() - UPDATE_INTERVAL;
		std::map<uint32_t, std::unique_ptr<IAction>> m_Actions;
		uint32_t m_LastUpdateIndex = 0;
	};
}
