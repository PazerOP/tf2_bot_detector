#pragma once

#include "IActionManager.h"

namespace tf2_bot_detector
{
	class Settings;
	class IWorldState;

	class IRCONActionManager : public IActionManager
	{
	public:
		static std::unique_ptr<IRCONActionManager> Create(const Settings& settings, IWorldState& world);
	};
}
