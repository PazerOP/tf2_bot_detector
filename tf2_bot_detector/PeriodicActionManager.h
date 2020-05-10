#pragma once

#include "Clock.h"

#include <memory>
#include <vector>

namespace tf2_bot_detector
{
	class ActionManager;
	class IPeriodicAction;

	class PeriodicActionManager final
	{
	public:
		PeriodicActionManager(ActionManager& actionManager);
		~PeriodicActionManager();

		void Add(std::unique_ptr<IPeriodicAction>&& action);

		void Update(time_point_t curTime);

	private:
		ActionManager& m_ActionManager;

		struct Action
		{
			std::unique_ptr<IPeriodicAction> m_Action;
			time_point_t m_LastRunTime{};
		};
		std::vector<Action> m_Actions;
	};
}
