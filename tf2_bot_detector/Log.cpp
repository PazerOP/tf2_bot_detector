#include "Log.h"

#include <imgui.h>
#include <mh/text/string_insertion.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <vector>

using namespace std::string_literals;
using namespace tf2_bot_detector;

static tf2_bot_detector::time_point_t s_LogTimestamp;
static std::recursive_mutex s_LogMutex;
static std::vector<LogMessage> s_LogMessages;

namespace tf2_bot_detector
{
	static void LogInternal(std::string msg, const LogMessageColor& color, std::ostream& output)
	{
		tm t = ToTM(s_LogTimestamp);

		std::lock_guard lock(s_LogMutex);
		output << '[' << std::put_time(&t, "%T") << "] " << msg << std::endl;

		s_LogMessages.push_back({ s_LogTimestamp, std::move(msg), { color.r, color.g, color.b, color.a } });
	}

	static std::ostream& GetLogFile()
	{
		static std::ostream& s_LogFile = []() -> std::ostream&
		{
			std::filesystem::path logPath = "logs";

			std::error_code ec;
			std::filesystem::create_directories(logPath, ec);
			if (ec)
			{
				LogInternal("Failed to create directory "s << logPath << ". Log output will go to stdout.", {}, std::cout);
				return std::cout;
			}

			auto t = ToTM(clock_t::now());
			logPath /= (""s << std::put_time(&t, "%Y-%m-%d_%H-%M-%S") << ".log");
			static std::ofstream s_LogFileLocal(logPath, std::ofstream::ate | std::ofstream::app | std::ofstream::out);
			if (!s_LogFileLocal.good())
			{
				LogInternal("Failed to open log file "s << logPath << ". Log output will go to stdout.", {}, std::cout);
				return std::cout;
			}

			return s_LogFileLocal;
		}();

		return s_LogFile;
	}
}

void tf2_bot_detector::Log(std::string msg)
{
	return Log(std::move(msg), { 1, 1, 1, 1 });
}

void tf2_bot_detector::LogWarning(std::string msg)
{
	Log(std::move(msg), { 1, 0.5, 0, 1 });
}

void tf2_bot_detector::LogError(std::string msg)
{
	Log(std::move(msg), { 1, 0.25, 0, 1 });
}

void tf2_bot_detector::Log(std::string msg, const LogMessageColor& color)
{
	LogInternal(std::move(msg), color, GetLogFile());
}

void tf2_bot_detector::SetLogTimestamp(time_point_t timestamp)
{
	s_LogTimestamp = timestamp;
}

auto tf2_bot_detector::GetLogMsgs() -> mh::generator<const LogMessage*>
{
	std::lock_guard lock(s_LogMutex);

	for (const auto& msg : s_LogMessages)
		co_yield &msg;
}

LogMessageColor::LogMessageColor(const ImVec4& vec) :
	LogMessageColor(vec.x, vec.y, vec.z, vec.w)
{
}
