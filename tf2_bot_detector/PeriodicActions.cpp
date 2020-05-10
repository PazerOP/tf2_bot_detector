#include "PeriodicActions.h"
#include "ActionManager.h"
#include "Actions.h"

using namespace tf2_bot_detector;
using namespace std::chrono_literals;

duration_t StatusUpdateAction::GetInterval() const
{
	return 6000ms;
}

bool StatusUpdateAction::Execute(ActionManager& manager)
{
	const bool success = manager.QueueAction(std::make_unique<GenericCommandAction>("status", m_NextShort ? "short" : ""));
	if (success)
		m_NextShort = !m_NextShort;

	return success;
}

