#pragma once

#include <ctime>
#include <memory>
#include <string_view>

namespace tf2_bot_detector
{
	class IConsoleLine;

	class IConsoleLineListener
	{
	public:
		virtual ~IConsoleLineListener() = default;

		virtual void OnConsoleLineParsed(IConsoleLine& line) {}
		virtual void OnConsoleLineUnparsed(std::time_t timestamp, const std::string_view& text) {}
	};
}
