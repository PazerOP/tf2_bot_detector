#pragma once

#include "Clock.h"
#include "IWorldEventListener.h"

#include <future>
#include <map>
#include <memory>
#include <queue>
#include <vector>

namespace tf2_bot_detector
{
	enum class ActionType;
	class IAction;
	class IActionGenerator;
	class IPeriodicActionGenerator;
	class Settings;
	class WorldState;

	class RCONActionManager final : BaseWorldEventListener
	{
	public:
		RCONActionManager(const Settings& settings, WorldState& world);
		~RCONActionManager();

		void Update();

		// Returns false if the action was not queued
		bool QueueAction(std::unique_ptr<IAction>&& action);

		template<typename TAction, typename... TArgs>
		bool QueueAction(TArgs&&... args)
		{
			return QueueAction(std::make_unique<TAction>(std::forward<TArgs>(args)...));
		}

		void AddPeriodicActionGenerator(std::unique_ptr<IPeriodicActionGenerator>&& action);

		template<typename TAction, typename... TArgs>
		void AddPeriodicActionGenerator(TArgs&&... args)
		{
			return AddPeriodicActionGenerator(std::make_unique<TAction>(std::forward<TArgs>(args)...));
		}

	private:
		void OnLocalPlayerInitialized(WorldState& world, bool initialized) override;

		struct RunningCommand
		{
			time_point_t m_StartTime{};
			std::string m_Command;
			std::shared_future<std::string> m_Future;
		};
		std::queue<RunningCommand> m_RunningCommands;
		void ProcessRunningCommands();
		void ProcessQueuedCommands();

		struct Writer;

		static constexpr duration_t UPDATE_INTERVAL = std::chrono::milliseconds(250);

		WorldState* m_WorldState = nullptr;
		const Settings* m_Settings = nullptr;
		time_point_t m_LastUpdateTime{};
		std::vector<std::unique_ptr<IAction>> m_Actions;
		std::vector<std::unique_ptr<IPeriodicActionGenerator>> m_PeriodicActionGenerators;
		std::map<ActionType, time_point_t> m_LastTriggerTime;

		bool ShouldDiscardCommand(const std::string_view& cmd) const;
		bool m_IsDiscardingServerCommands = true;
	};
}
