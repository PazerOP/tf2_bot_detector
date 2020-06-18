#pragma once

#include "Clock.h"
#include "IConsoleLineListener.h"

#include <map>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace tf2_bot_detector
{
	class IAction;
	enum class ActionType;
	class Settings;
	class IPeriodicAction;

	class ActionManager final
	{
	public:
		ActionManager(const Settings& settings);
		~ActionManager();

		void Update();

		// Returns false if the action was not queued
		bool QueueAction(std::unique_ptr<IAction>&& action);

		template<typename TAction, typename... TArgs>
		bool QueueAction(TArgs&&... args)
		{
			return QueueAction(std::make_unique<TAction>(std::forward<TArgs>(args)...));
		}

		// Whenever another action triggers a send of command(s) to the game, these actions will be
		// given the chance to add themselves to the
		void AddPiggybackAction(std::unique_ptr<IAction> action);

		template<typename TAction, typename... TArgs>
		void AddPiggybackAction(TArgs&&... args)
		{
			return AddPiggybackAction(std::make_unique<TAction>(std::forward<TArgs>(args)...));
		}

		void AddPeriodicAction(std::unique_ptr<IPeriodicAction>&& action);

		template<typename TAction, typename... TArgs>
		void AddPeriodicAction(TArgs&&... args)
		{
			return AddPeriodicAction(std::make_unique<TAction>(std::forward<TArgs>(args)...));
		}

	private:
		struct Writer;
		bool ProcessSimpleCommands(const Writer& writer);
		bool ProcessComplexCommands(const Writer& writer);
		bool SendCommandToGame(std::string cmd);

		static constexpr duration_t UPDATE_INTERVAL = std::chrono::seconds(1);

		struct QueuedAction
		{
			std::unique_ptr<IAction> m_Action;
			uint32_t m_UpdateIndex;
		};

		auto absolute_root() const;
		auto absolute_cfg() const;
		auto absolute_cfg_temp() const;

		// Map of temp cfg file content hashes to cfg file name so we don't
		// create a ton of duplicate files.
		std::unordered_multimap<size_t, uint32_t> m_TempCfgFiles;

		const Settings* m_Settings = nullptr;
		time_point_t m_LastUpdateTime{};
		std::vector<std::unique_ptr<IAction>> m_Actions;
		std::vector<std::unique_ptr<IAction>> m_PiggybackActions;
		std::map<ActionType, time_point_t> m_LastTriggerTime;
		uint32_t m_LastUpdateIndex = 0;

		struct RunningCommand;
		std::list<RunningCommand> m_RunningCommands;
		void ProcessRunningCommands();

		struct PeriodicAction
		{
			std::unique_ptr<IPeriodicAction> m_Action;
			time_point_t m_LastRunTime{};
		};
		std::vector<PeriodicAction> m_PeriodicActions;
	};
}
