#include "PeriodicActionManager.h"
#include "PeriodicActions.h"

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

void PeriodicActionManager::Update(time_point_t curTime)
{
	for (auto& action : m_Actions)
	{
		if ((curTime - action.m_LastRunTime) >= action.m_Action->GetInterval())
		{
			if (action.m_Action->Execute(m_ActionManager))
				action.m_LastRunTime = curTime;
		}
	}
}
