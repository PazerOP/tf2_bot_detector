#include "RCONActionManager.h"
#include "Actions/ActionGenerators.h"
#include "Config/Settings.h"
#include "ConsoleLog/ConsoleLines.h"
#include "Actions.h"
#include "Log.h"
#include "WorldEventListener.h"
#include "WorldState.h"

#include <mh/text/insertion_conversion.hpp>
#include <mh/text/string_insertion.hpp>
#include <srcon/async_client.h>

#include <filesystem>
#include <iomanip>
#include <queue>
#include <regex>
#include <unordered_set>

#undef min
#undef max

static const std::regex s_SingleCommandRegex(R"regex(((?:\w+)(?:\ +\w+)?)(?:\n)?)regex", std::regex::optimize);

using namespace tf2_bot_detector;
using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;

namespace
{
	class RCONActionManager final : public IRCONActionManager, BaseWorldEventListener
	{
	public:
		RCONActionManager(const Settings& settings, IWorldState& world);
		~RCONActionManager();

		void Update();

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
		void OnLocalPlayerInitialized(IWorldState& world, bool initialized) override;

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

		IWorldState& m_WorldState;
		const Settings& m_Settings;
		time_point_t m_LastUpdateTime{};
		std::vector<std::unique_ptr<IAction>> m_Actions;
		std::vector<std::unique_ptr<IPeriodicActionGenerator>> m_PeriodicActionGenerators;
		std::map<ActionType, time_point_t> m_LastTriggerTime;

		bool ShouldDiscardCommand(const std::string_view& cmd) const;
		bool m_IsDiscardingServerCommands = true;
	};
}

struct SRCONInit
{
	SRCONInit()
	{
		srcon::SetLogFunc([](std::string&& msg)
			{
				DebugLog("[SRCON] "s << std::move(msg));
			});
	}

} s_SRCONInit;

std::unique_ptr<IRCONActionManager> IRCONActionManager::Create(const Settings& settings, IWorldState& world)
{
	return std::make_unique<RCONActionManager>(settings, world);
}

RCONActionManager::RCONActionManager(const Settings& settings, IWorldState& world) :
	m_Settings(settings), m_WorldState(world)
{
	world.AddWorldEventListener(this);

	m_IsDiscardingServerCommands = settings.m_ConfigCompatibilityMode;
}

RCONActionManager::~RCONActionManager()
{
}

bool RCONActionManager::QueueAction(std::unique_ptr<IAction>&& action)
{
	if (const auto maxQueuedCount = action->GetMaxQueuedCount();
		maxQueuedCount <= m_Actions.size())
	{
		const ActionType curActionType = action->GetType();
		size_t count = 0;
		for (const auto& queued : m_Actions)
		{
			if (queued->GetType() == curActionType)
			{
				if (++count >= maxQueuedCount)
					return false;
			}
		}
	}

	m_Actions.push_back(std::move(action));
	return true;
}

void RCONActionManager::AddPeriodicActionGenerator(std::unique_ptr<IPeriodicActionGenerator>&& action)
{
	m_PeriodicActionGenerators.push_back(std::move(action));
}

void RCONActionManager::OnLocalPlayerInitialized(IWorldState& world, bool initialized)
{
	m_IsDiscardingServerCommands = !initialized;
	DebugLog("m_IsDiscardingServerCommands = {}, m_Settings.m_ConfigCompatibilityMode = {}",
		m_IsDiscardingServerCommands, m_Settings.m_ConfigCompatibilityMode);
}

void RCONActionManager::ProcessRunningCommands()
{
	constexpr const char* funcName = __func__;
	const auto PrintErrorMsg = [funcName](const std::string_view& msg)
	{
		return DebugLogWarning(""s << funcName << "(): " << msg);
	};

	while (!m_RunningCommands.empty())
	{
		auto& cmd = m_RunningCommands.front();
		if (cmd.m_Future.wait_for(0s) == std::future_status::timeout)
			break;

		try
		{
			auto resultStr = cmd.m_Future.get();

			if (m_Settings.m_Unsaved.m_DebugShowCommands)
			{
				const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tf2_bot_detector::clock_t::now() - cmd.m_StartTime);
				std::string msg = "Game command processed in "s << elapsed.count() << "ms : " << std::quoted(cmd.m_Command);

				if (!resultStr.empty())
					msg << ", response " << resultStr.size() << " bytes";

				DebugLog({ 1, 1, 1, 0.5f }, std::move(msg));
			}

			if (!resultStr.empty())
				m_WorldState.AddConsoleOutputChunk(resultStr);
		}
		catch (const std::future_error& e)
		{
			if (e.code() == std::future_errc::broken_promise)
				DebugLogWarning(std::string(__FUNCTION__) << "(): " << e.code().message() << ": " << e.what() << ": " << std::quoted(cmd.m_Command));
			else
				PrintErrorMsg(e.code().message() << ": " << e.what() << ": " << std::quoted(cmd.m_Command));
		}
		catch (const std::exception& e)
		{
			PrintErrorMsg(""s << e.what() << ": " << std::quoted(cmd.m_Command));
		}

		m_RunningCommands.pop();
	}
}

bool RCONActionManager::ShouldDiscardCommand(const std::string_view& cmd) const
{
	if (!m_IsDiscardingServerCommands || !m_Settings.m_ConfigCompatibilityMode)
		return false;

	static const std::unordered_set<std::string_view> s_KnownClientCommands =
	{
		"tf_lobby_debug",
		"tf_party_debug",
		"net_status",
		"con_logfile",
		"con_timestamp",
		"tf_mm_debug_level",
		"net_showmsg",
	};

	if (s_KnownClientCommands.contains(cmd))
		return false;

	DebugLog("Discarding potential server command "s << std::quoted(cmd));
	return true;
}

void RCONActionManager::ProcessQueuedCommands()
{
	if (!m_Settings.m_Unsaved.m_RCONClient)
		return;

	const auto curTime = tfbd_clock_t::now();
	if (curTime < (m_LastUpdateTime + UPDATE_INTERVAL))
		return;

	// Update periodic actions
	for (const auto& generator : m_PeriodicActionGenerators)
		generator->Execute(*this);

	if (!m_Actions.empty())
	{
		bool actionTypes[(int)ActionType::COUNT]{};

		struct Writer final : ICommandWriter
		{
			void Write(std::string cmd, std::string args) override
			{
				if (m_Manager->ShouldDiscardCommand(cmd))
					return;

				if (!args.empty())
					cmd << ' ' << args;

				m_Manager->m_RunningCommands.push(
					{
						.m_StartTime = tfbd_clock_t::now(),
						.m_Command = cmd,
						.m_Future = m_Manager->m_Settings.m_Unsaved.m_RCONClient->send_command_async(cmd, false),
					});
			}

			RCONActionManager* m_Manager = nullptr;

		} writer;

		writer.m_Manager = this;

		const auto ProcessAction = [&](const IAction* action)
		{
			const ActionType type = action->GetType();
			{
				auto& previousMsg = actionTypes[(int)type];
				const auto minInterval = action->GetMinInterval();

				if (minInterval.count() > 0 && (previousMsg || (curTime - m_LastTriggerTime[type]) < minInterval))
					return false;

				previousMsg = true;
			}

			action->WriteCommands(writer);
			m_LastTriggerTime[type] = curTime;
			return true;
		};

		// Process actions
		for (auto it = m_Actions.begin(); it != m_Actions.end(); )
		{
			const IAction* action = it->get();
			if (ProcessAction(it->get()))
				it = m_Actions.erase(it);
			else
				++it;
		}
	}

	m_LastUpdateTime = curTime;
}

void RCONActionManager::Update()
{
	ProcessQueuedCommands();
	ProcessRunningCommands();
}
