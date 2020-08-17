#pragma once

#include <memory>

namespace tf2_bot_detector
{
	class IAction;
	class IPeriodicActionGenerator;

	class IActionManager
	{
	public:
		virtual ~IActionManager() = default;

		virtual void Update() = 0;

		// Returns false if the action was not queued
		virtual bool QueueAction(std::unique_ptr<IAction>&& action) = 0;

		template<typename TAction, typename... TArgs>
		bool QueueAction(TArgs&&... args)
		{
			return QueueAction(std::make_unique<TAction>(std::forward<TArgs>(args)...));
		}

		virtual void AddPeriodicActionGenerator(std::unique_ptr<IPeriodicActionGenerator>&& action) = 0;

		template<typename TAction, typename... TArgs>
		void AddPeriodicActionGenerator(TArgs&&... args)
		{
			return AddPeriodicActionGenerator(std::make_unique<TAction>(std::forward<TArgs>(args)...));
		}
	};
}
