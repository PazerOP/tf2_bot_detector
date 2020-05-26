#pragma once

#include "Clock.h"

#include <string_view>

namespace tf2_bot_detector
{
	class IConsoleLine;
	class WorldState;

	class IConsoleLineListener
	{
	public:
		virtual ~IConsoleLineListener() = default;

		virtual void OnConsoleLineParsed(WorldState& world, IConsoleLine& line) {}
		virtual void OnConsoleLineUnparsed(WorldState& world, const std::string_view& text) {}
	};
}
