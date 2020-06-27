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

		/// <summary>
		/// Called when a chunk is read from console.log.
		/// </summary>
		/// <param name="world">The associated WorldState.</param>
		/// <param name="consoleLinesParsed">True if console lines were parsed from this chunk.
		/// False if everything ended up going to OnConsoleLineUnparsed().</param>
		virtual void OnConsoleLogChunkParsed(WorldState& world, bool consoleLinesParsed) {}
	};
}
