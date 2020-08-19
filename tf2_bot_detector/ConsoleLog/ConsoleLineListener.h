#pragma once

#include "Clock.h"

#include <string_view>

namespace tf2_bot_detector
{
	class IConsoleLine;
	class IWorldState;

	class IConsoleLineListener
	{
	public:
		virtual ~IConsoleLineListener() = default;

		virtual void OnConsoleLineParsed(IWorldState& world, IConsoleLine& line) = 0;
		virtual void OnConsoleLineUnparsed(IWorldState& world, const std::string_view& text) = 0;

		/// <summary>
		/// Called when a chunk is read from console.log.
		/// </summary>
		/// <param name="world">The associated WorldState.</param>
		/// <param name="consoleLinesParsed">True if console lines were parsed from this chunk.
		/// False if everything ended up going to OnConsoleLineUnparsed().</param>
		virtual void OnConsoleLogChunkParsed(IWorldState& world, bool consoleLinesParsed) = 0;
	};

	class BaseConsoleLineListener : public IConsoleLineListener
	{
	public:
		void OnConsoleLineParsed(IWorldState& world, IConsoleLine& line) override {}
		void OnConsoleLineUnparsed(IWorldState& world, const std::string_view& text) override {}

		void OnConsoleLogChunkParsed(IWorldState& world, bool consoleLinesParsed) override {}
	};

	class AutoConsoleLineListener : public BaseConsoleLineListener
	{
	public:
		AutoConsoleLineListener(IWorldState& world);
		AutoConsoleLineListener(const AutoConsoleLineListener& other);
		AutoConsoleLineListener& operator=(const AutoConsoleLineListener& other);
		AutoConsoleLineListener(AutoConsoleLineListener&& other);
		AutoConsoleLineListener& operator=(AutoConsoleLineListener&& other);
		~AutoConsoleLineListener();

	private:
		IWorldState* m_World = nullptr;
	};
}
