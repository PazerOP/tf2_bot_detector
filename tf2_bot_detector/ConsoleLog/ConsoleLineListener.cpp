#include "ConsoleLineListener.h"
#include "WorldState.h"

using namespace tf2_bot_detector;

AutoConsoleLineListener::AutoConsoleLineListener(IWorldState& world) :
	m_World(&world)
{
	m_World->AddConsoleLineListener(this);
}

AutoConsoleLineListener::AutoConsoleLineListener(const AutoConsoleLineListener& other) :
	m_World(other.m_World)
{
	m_World->AddConsoleLineListener(this);
}

AutoConsoleLineListener& AutoConsoleLineListener::operator=(const AutoConsoleLineListener& other)
{
	m_World->RemoveConsoleLineListener(this);
	m_World = other.m_World;
	m_World->AddConsoleLineListener(this);
	return *this;
}

AutoConsoleLineListener::AutoConsoleLineListener(AutoConsoleLineListener&& other) :
	m_World(other.m_World)
{
	m_World->AddConsoleLineListener(this);
}

AutoConsoleLineListener& AutoConsoleLineListener::operator=(AutoConsoleLineListener&& other)
{
	m_World->RemoveConsoleLineListener(this);
	m_World = other.m_World;
	m_World->AddConsoleLineListener(this);
	return *this;
}

AutoConsoleLineListener::~AutoConsoleLineListener()
{
	m_World->RemoveConsoleLineListener(this);
}
