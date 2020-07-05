#include "RCONActionManager.h"
#include "Actions.h"
#include "Config/Settings.h"
#include "ConsoleLog/ConsoleLines.h"
#include "Log.h"
#include "Actions/ActionGenerators.h"
#include "WorldState.h"

#include <mh/text/insertion_conversion.hpp>
#include <mh/text/string_insertion.hpp>
#include <srcon/async_client.h>

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

RCONActionManager::RCONActionManager(const Settings& settings, WorldState& world) :
	m_Settings(&settings), m_WorldState(&world)
{
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

void RCONActionManager::ProcessRunningCommands()
{
	constexpr const char* funcName = __func__;
	const auto PrintErrorMsg = [funcName](const std::string_view& msg)
	{
		return LogError(""s << funcName << "(): " << msg);
	};

	while (!m_RunningCommands.empty())
	{
		auto& cmd = m_RunningCommands.front();
		if (cmd.m_Future.wait_for(0s) == std::future_status::timeout)
			break;

		try
		{
			auto resultStr = cmd.m_Future.get();

			if (m_Settings->m_Unsaved.m_DebugShowCommands)
			{
				const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(clock_t::now() - cmd.m_StartTime);
				std::string msg = "Game command processed in "s << elapsed.count() << "ms : " << std::quoted(cmd.m_Command);

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
					LogError("WorldState was nullptr when we tried to give it the result for "s
						<< std::quoted(cmd.m_Command) << ": " << resultStr);
				}
			}
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

void RCONActionManager::ProcessQueuedCommands()
{
	if (!m_Settings->m_Unsaved.m_RCONClient)
		return;

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
				if (!args.empty())
					cmd << ' ' << args;

				m_Manager->m_RunningCommands.push(
					{
						.m_StartTime = clock_t::now(),
						.m_Command = cmd,
						.m_Future = m_Manager->m_Settings->m_Unsaved.m_RCONClient->send_command_async(cmd),
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
