#include "WorldEventListener.h"
#include "WorldState.h"

using namespace tf2_bot_detector;

AutoWorldEventListener::AutoWorldEventListener(IWorldState& world) :
	m_World(&world)
{
	m_World->AddWorldEventListener(this);
}

AutoWorldEventListener::AutoWorldEventListener(const AutoWorldEventListener& other) :
	m_World(other.m_World)
{
	m_World->AddWorldEventListener(this);
}

AutoWorldEventListener& AutoWorldEventListener::operator=(const AutoWorldEventListener& other)
{
	m_World->RemoveWorldEventListener(this);
	m_World = other.m_World;
	m_World->AddWorldEventListener(this);
	return *this;
}

AutoWorldEventListener::AutoWorldEventListener(AutoWorldEventListener&& other) :
	m_World(other.m_World)
{
	m_World->AddWorldEventListener(this);
}

AutoWorldEventListener& AutoWorldEventListener::operator=(AutoWorldEventListener&& other)
{
	m_World->RemoveWorldEventListener(this);
	m_World = other.m_World;
	m_World->AddWorldEventListener(this);
	return *this;
}

AutoWorldEventListener::~AutoWorldEventListener()
{
	m_World->RemoveWorldEventListener(this);
}
