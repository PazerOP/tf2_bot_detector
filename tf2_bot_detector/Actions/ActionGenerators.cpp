#include "ActionGenerators.h"
#include "Actions.h"
#include "Log.h"
#include "RCONActionManager.h"

using namespace tf2_bot_detector;
using namespace std::chrono_literals;

duration_t StatusUpdateActionGenerator::GetInterval() const
{
	return 3s;
}

bool StatusUpdateActionGenerator::ExecuteImpl(RCONActionManager& manager)
{
	// 3 second interval, we want:
	//   1. status
	//   2. ping
	//   3. status short
	//   4. ping
	// repeat

	if (m_NextPing)
	{
		if (!manager.QueueAction<GenericCommandAction>("ping"))
			return false;
	}
	else
	{
		//if (!manager.QueueAction<GenericCommandAction>("status", m_NextShort ? "short" : ""))
		//	return false;

		//m_NextShort = !m_NextShort;
		if (!manager.QueueAction<GenericCommandAction>("status"))
			return false;
	}

	m_NextPing = !m_NextPing;

	return true;
}

duration_t ConfigActionGenerator::GetInterval() const
{
	return 10s;
}

bool ConfigActionGenerator::ExecuteImpl(RCONActionManager& manager)
{
	if (!manager.QueueAction<GenericCommandAction>("con_logfile", "console.log"))
		return false;
	if (!manager.QueueAction<GenericCommandAction>("con_timestamp", "1"))
		return false;
	if (!manager.QueueAction<GenericCommandAction>("tf_mm_debug_level", "4"))
		return false; // This is defaulted to 4, but mastercom's stupid config turns this off

	if (!manager.QueueAction<GenericCommandAction>("net_showmsg", "svc_UserMessage"))
		return false;

	return true;
}

bool IPeriodicActionGenerator::Execute(RCONActionManager& manager)
{
	const auto curTime = clock_t::now();
	const auto interval = GetInterval();
	if (m_LastRunTime == time_point_t{})
	{
		const auto delay = GetInitialDelay();
		m_LastRunTime = curTime - interval + delay;
	}

	if ((curTime - m_LastRunTime) >= interval)
	{
		if (ExecuteImpl(manager))
		{
			m_LastRunTime = curTime;
			return true;
		}
		else
		{
			LogWarning("Couldn't execute IPeriodicActionGenerator!");
			return false;
		}
	}

	return true;
}

bool LobbyDebugActionGenerator::ExecuteImpl(RCONActionManager& manager)
{
	return manager.QueueAction<GenericCommandAction>("tf_lobby_debug");
}
