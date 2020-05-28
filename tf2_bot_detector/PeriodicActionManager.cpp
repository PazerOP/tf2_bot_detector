#include "PeriodicActionManager.h"
#include "PeriodicActions.h"
#include "Log.h"

using namespace tf2_bot_detector;

PeriodicActionManager::PeriodicActionManager(ActionManager& actionManager) :
	m_ActionManager(actionManager)
{
}

PeriodicActionManager::~PeriodicActionManager()
{
}

void PeriodicActionManager::Add(std::unique_ptr<IPeriodicAction>&& action)
{
	m_Actions.push_back({ std::move(action), {} });
}

void PeriodicActionManager::Update()
{
	const auto curTime = clock_t::now();

	for (auto& action : m_Actions)
	{
		const auto interval = action.m_Action->GetInterval();
		if (action.m_LastRunTime == time_point_t{})
		{
			const auto delay = action.m_Action->GetInitialDelay();
			action.m_LastRunTime = curTime - interval + delay;
		}

		if ((curTime - action.m_LastRunTime) >= interval)
		{
			if (action.m_Action->Execute(m_ActionManager))
				action.m_LastRunTime = curTime;
			else
				Log("Couldn't run action");
		}
	}
}
