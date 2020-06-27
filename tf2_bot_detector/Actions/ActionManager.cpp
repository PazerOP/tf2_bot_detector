#include "ActionManager.h"
#include "Actions.h"
#include "Config/Settings.h"
#include "ConsoleLog/ConsoleLines.h"
#include "Log.h"
#include "Actions/ActionGenerators.h"
#include "WorldState.h"

#include <mh/text/insertion_conversion.hpp>
#include <mh/text/string_insertion.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <regex>

#undef min
#undef max

static const std::regex s_SingleCommandRegex(R"regex(((?:\w+)(?:\ +\w+)?)(?:\n)?)regex", std::regex::optimize);

using namespace tf2_bot_detector;
using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;


ActionManager::ActionManager(const Settings& settings, WorldState& world) :
	m_Settings(&settings), m_WorldState(&world)
{
	try
	{
		//m_RCONClient.connect(settings.GetLocalIP(), "testpw");
		m_RCONClient.connect("127.0.0.1", "testpw");
	}
	catch (const std::exception& e)
	{
		DebugLogWarning("Failed initial RCON connection attempt: "s << e.what());
	}

	m_RCONThread = std::thread(&ActionManager::RCONThreadFunc, this, m_RCONCancellationSource.token());
}

ActionManager::~ActionManager()
{
	assert(m_RCONCancellationSource.can_be_cancelled());
	m_RCONCancellationSource.request_cancellation();
	m_RCONThread.join();
}

bool ActionManager::QueueAction(std::unique_ptr<IAction>&& action)
{
	if (const auto maxQueuedCount = action->GetMaxQueuedCount();
		maxQueuedCount <= m_Actions.size())
	{
		const ActionType curActionType = action->GetType();
		size_t count = 0;
		for (const auto& queuedAction : m_Actions)
		{
			if (queuedAction->GetType() == curActionType)
			{
				if (++count >= maxQueuedCount)
					return false;
			}
		}
	}

	m_Actions.push_back(std::move(action));
	return true;
}

void ActionManager::AddPeriodicActionGenerator(std::unique_ptr<IPeriodicActionGenerator>&& action)
{
	m_PeriodicActionGenerators.push_back(std::move(action));
}

void ActionManager::AddPiggybackActionGenerator(std::unique_ptr<IActionGenerator>&& action)
{
	m_PiggybackActionGenerators.push_back(std::move(action));
}

ActionManager::RCONCommand::RCONCommand(std::string cmd) :
	m_Command(std::move(cmd)),
	m_Promise(std::make_shared<std::promise<std::string>>()),
	m_Future(m_Promise->get_future())
{
}

void ActionManager::RCONThreadFunc(cppcoro::cancellation_token cancellationToken)
{
	while (!cancellationToken.is_cancellation_requested())
	{
		std::this_thread::sleep_for(250ms);

		while (!m_RCONCommands.empty() && !cancellationToken.is_cancellation_requested())
		{
			std::optional<RCONCommand> cmd;
			{
				std::lock_guard lock(m_RCONCommandsMutex);
				if (m_RCONCommands.empty())
					break;

				cmd = m_RCONCommands.front();
			}

			try
			{
				const auto startTime = clock_t::now();

				auto resultStr = RunCommand(cmd->m_Command);
				//DebugLog("Setting promise for "s << std::quoted(cmd->m_Command) << " to " << std::quoted(resultStr));
				cmd->m_Promise->set_value(resultStr);
#if 0
				if (auto waitResult = result.wait_for(7s); waitResult == std::future_status::timeout)
				{
					LogWarning("SRCON reconnecting after timing out waiting for game command "s << std::quoted(cmd));
					std::lock_guard lock(m_RCONClientMutex);
					m_RCONClient.reconnect();
					return false;
				}
				const auto resultStr = result.get();
#endif

				if (m_Settings->m_Unsaved.m_DebugShowCommands)
				{
					const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(clock_t::now() - startTime);
					std::string msg = "Game command processed in "s << elapsed.count() << "ms : " << std::quoted(cmd->m_Command);

					if (!resultStr.empty())
						msg << ", response " << resultStr.size() << " bytes";

					Log(std::move(msg), { 1, 1, 1, 0.6f });
				}

				if (!resultStr.empty())
				{
					if (m_WorldState)
					{
						m_WorldState->AddConsoleOutputChunk(resultStr);
					}
					else
					{
						LogError("WorldState was nullptr when we tried to give it the result: "s << resultStr);
					}
				}

				{
					std::lock_guard lock(m_RCONCommandsMutex);
					assert(!m_RCONCommands.empty());
					if (!m_RCONCommands.empty())
					{
						assert(m_RCONCommands.front() == cmd);
						m_RCONCommands.pop();
					}
				}
			}
			catch (const std::exception& e)
			{
				LogError(std::string(__FUNCTION__) << "(): Unhandled exception: " << e.what());
				std::this_thread::sleep_for(1s);

				try
				{
					std::lock_guard lock(m_RCONClientMutex);
					m_RCONClient.reconnect();
				}
				catch (const std::exception& e2)
				{
					LogError(std::string(__FUNCTION__) << "(): Failed to reconnect after exception: " << e2.what());
					std::this_thread::sleep_for(1s);
				}
			}
		}
	}
}

void ActionManager::Update()
{
	const auto curTime = clock_t::now();
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
				m_AnyCmdsRun = true;
				m_Manager->RunCommandAsync(cmd + " " + args);
			}

			ActionManager* m_Manager = nullptr;
			bool m_AnyCmdsRun = false;

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

		const auto ProcessActions = [&]()
		{
			for (auto it = m_Actions.begin(); it != m_Actions.end(); )
			{
				const IAction* action = it->get();
				if (ProcessAction(it->get()))
					it = m_Actions.erase(it);
				else
					++it;
			}
		};

		// Handle normal actions
		ProcessActions();

		if (writer.m_AnyCmdsRun)
		{
			// Handle piggyback commands
			for (const auto& generator : m_PiggybackActionGenerators)
				generator->Execute(*this);

			// Process any actions added by piggyback action generators
			ProcessActions();
		}
	}

	m_LastUpdateTime = curTime;
}

std::string ActionManager::RunCommand(std::string cmd)
{
	std::unique_lock lock(m_RCONClientMutex, 5s);
	if (!lock.owns_lock())
		throw std::runtime_error("Failed to acquire rcon client mutex");

	if (!m_RCONClient.is_connected())
	{
		DebugLog(std::string(__FUNCTION__) << "(): SRCON not connected, reconnecting for command " << std::quoted(cmd));
		m_RCONClient.reconnect();
	}

	return m_RCONClient.send(cmd);
}

std::shared_future<std::string> ActionManager::RunCommandAsync(std::string cmd)
{
	std::lock_guard lock(m_RCONCommandsMutex);
	return m_RCONCommands.emplace(std::move(cmd)).m_Future;
}

bool ActionManager::SendCommandToGame(std::string cmd)
{
	RunCommandAsync(std::move(cmd));
	return true;
}

ActionManager::InitSRCON::InitSRCON()
{
	srcon::SetLogFunc([](std::string&& msg)
		{
			DebugLog("[SRCON] "s << std::move(msg));
		});
}
