#pragma once

#include "Clock.h"
#include "IConsoleLineListener.h"

#include <map>
#include <memory>
#include <typeindex>

namespace tf2_bot_detector
{
	class IAction;
	enum class ActionType;

	class ActionManager final : public IConsoleLineListener
	{
	public:
		~ActionManager();

		void QueueAction(std::unique_ptr<IAction>&& action);
		void Update();

	private:
		void OnConsoleLineParsed(IConsoleLine& line) override;
		void OnConsoleLineUnparsed(time_point_t timestamp, const std::string_view& text) override;

		static constexpr duration_t UPDATE_INTERVAL = std::chrono::milliseconds(100);

		struct QueuedAction
		{
			std::unique_ptr<IAction> m_Action;
			uint32_t m_UpdateIndex;
		};

		time_point_t m_CurrentTime;
		time_point_t m_LastUpdateTime = clock_t::now() - UPDATE_INTERVAL;
		std::map<uint32_t, std::unique_ptr<IAction>> m_Actions;
		std::map<ActionType, time_point_t> m_LastTriggerTime;
		uint32_t m_LastUpdateIndex = 0;
	};
}
