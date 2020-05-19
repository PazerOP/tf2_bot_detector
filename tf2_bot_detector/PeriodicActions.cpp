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
		if (!manager.QueueAction<GenericCommandAction>("status", m_NextShort ? "short" : ""))
			return false;

		m_NextShort = !m_NextShort;
	}

	m_NextPing = !m_NextPing;

	return true;
}

duration_t NetStatusAction::GetInterval() const
{
	return 100ms;
}

bool NetStatusAction::Execute(ActionManager& manager)
{
	if (!manager.QueueAction<GenericCommandAction>("net_status"))
		return false;

	return true;
}

duration_t IPeriodicAction::GetInitialDelay() const
{
	return 0s;
}
