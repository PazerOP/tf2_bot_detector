#include "PeriodicActions.h"
#include "ActionManager.h"
#include "Actions.h"

using namespace tf2_bot_detector;
using namespace std::chrono_literals;

duration_t StatusUpdateAction::GetInterval() const
{
	return 5500ms;
}

bool StatusUpdateAction::Execute(ActionManager& manager)
{
	const char* cmd = m_NextShort ? "status short" : "status";

	const bool success = manager.QueueAction(std::make_unique<GenericCommandAction>(cmd));
	if (success)
		m_NextShort = !m_NextShort;

	return success;
}

