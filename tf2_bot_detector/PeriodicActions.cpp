#include "PeriodicActions.h"
#include "ActionManager.h"
#include "Actions.h"

using namespace tf2_bot_detector;
using namespace std::chrono_literals;

duration_t StatusUpdateAction::GetInterval() const
{
	return 3s;
}

bool StatusUpdateAction::Execute(ActionManager& manager)
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

duration_t ConfigAction::GetInterval() const
{
	return 10s;
}

bool ConfigAction::Execute(ActionManager& manager)
{
	if (!manager.QueueAction<GenericCommandAction>("con_logfile", "console.log"))
		return false;
	if (!manager.QueueAction<GenericCommandAction>("con_timestamp", "1"))
		return false;
	if (!manager.QueueAction<GenericCommandAction>("tf_mm_debug_level", "4"))
		return false; // This is defaulted to 4, but mastercom's stupid config turns this off

	return true;
}
