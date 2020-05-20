#pragma once

#include <string>

namespace tf2_bot_detector
{
	class ICommandWriter
	{
	public:
		virtual ~ICommandWriter() = default;

		virtual void Write(std::string cmd, std::string args) = 0;
		virtual void Write(std::string cmd) { return Write(cmd, {}); }
	};

	class ICommandSource
	{
	public:
		virtual ~ICommandSource() = default;

		virtual void WriteCommands(ICommandWriter& writer) const = 0;
	};
}
