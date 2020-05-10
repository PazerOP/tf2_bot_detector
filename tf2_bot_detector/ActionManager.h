#pragma once

#include "Clock.h"
#include "IConsoleLineListener.h"

#include <map>
#include <memory>
#include <typeindex>
#include <vector>

namespace tf2_bot_detector
{
	class IAction;
	enum class ActionType;

	class ActionManager final
	{
	public:
		ActionManager();
		~ActionManager();

		// Returns false if the action was not queued
		bool QueueAction(std::unique_ptr<IAction>&& action);
		void Update(time_point_t curTime);

	private:
		static void SendCommandToGame(const std::string_view& cmd);

		static constexpr duration_t UPDATE_INTERVAL = std::chrono::milliseconds(100);

		struct QueuedAction
		{
			std::unique_ptr<IAction> m_Action;
			uint32_t m_UpdateIndex;
		};

		time_point_t m_LastUpdateTime{};
		std::vector<std::unique_ptr<IAction>> m_Actions;
		std::map<ActionType, time_point_t> m_LastTriggerTime;
		uint32_t m_LastUpdateIndex = 0;
	};
}
