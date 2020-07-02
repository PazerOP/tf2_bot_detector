#include "Log.h"

#include <imgui.h>
#include <mh/text/string_insertion.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

using namespace std::string_literals;
using namespace tf2_bot_detector;

static std::recursive_mutex s_LogMutex;
static std::vector<LogMessage> s_LogMessages;
static size_t s_VisibleLogMessagesStart = 0;

namespace tf2_bot_detector
{
	static void LogToStream(const std::string_view& msg, std::ostream& output, time_point_t timestamp = clock_t::now())
	{
		std::lock_guard lock(s_LogMutex);

		tm t = ToTM(timestamp);
		output << '[' << std::put_time(&t, "%T") << "] " << msg << std::endl;

#ifdef _WIN32
		{
			std::string dbgMsg = "Log: "s << msg << '\n';
			OutputDebugStringA(dbgMsg.c_str());
		}
#endif
	}

	static void LogInternal(std::string msg, const LogMessageColor& color, std::ostream& output,
		time_point_t timestamp = clock_t::now())
	{
		std::lock_guard lock(s_LogMutex);
		LogToStream(msg, output, timestamp);
		s_LogMessages.push_back({ timestamp, std::move(msg), { color.r, color.g, color.b, color.a } });
	}
}

const std::filesystem::path& tf2_bot_detector::GetLogFilename()
{
	static const std::filesystem::path s_Path = []() -> std::filesystem::path
	{
		std::filesystem::path logPath = "logs";

		std::error_code ec;
		std::filesystem::create_directories(logPath, ec);
		if (ec)
		{
			LogInternal("Failed to create directory "s << logPath << ". Log output will go to stdout.", {}, std::cout);
			return {};
		}

		auto t = ToTM(clock_t::now());
		return logPath / (""s << std::put_time(&t, "%Y-%m-%d_%H-%M-%S") << ".log");
	}();

	return s_Path;
}

namespace tf2_bot_detector
{
	static std::ostream& GetLogFile()
	{
		static std::ostream& s_LogFile = []() -> std::ostream&
		{
			const auto& logPath = GetLogFilename();
			if (logPath.empty())
				return std::cout;

			static std::ofstream s_LogFileLocal(logPath, std::ofstream::ate | std::ofstream::app | std::ofstream::out | std::ofstream::binary);
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

static constexpr LogMessageColor COLOR_DEFAULT = { 1, 1, 1, 1 };
static constexpr LogMessageColor COLOR_WARNING = { 1, 0.5, 0, 1 };
static constexpr LogMessageColor COLOR_ERROR =   { 1, 0.25, 0, 1 };

void tf2_bot_detector::Log(std::string msg, const LogMessageColor& color)
{
	LogInternal(std::move(msg), color, GetLogFile());
}

void tf2_bot_detector::LogWarning(std::string msg)
{
	Log(std::move(msg), COLOR_WARNING);
}

void tf2_bot_detector::LogError(std::string msg)
{
	Log(std::move(msg), COLOR_ERROR);
}

void tf2_bot_detector::DebugLog(std::string msg, const LogMessageColor& color)
{
#ifdef _DEBUG
	Log(std::move(msg), color);
#else
	LogToStream(msg, GetLogFile());
#endif
}

void tf2_bot_detector::DebugLogWarning(std::string msg)
{
	Log(std::move(msg), { COLOR_WARNING.r, COLOR_WARNING.g, COLOR_WARNING.b, 0.67f });
}

auto tf2_bot_detector::GetVisibleLogMsgs() -> cppcoro::generator<const LogMessage&>
{
	std::lock_guard lock(s_LogMutex);

	for (size_t i = s_VisibleLogMessagesStart; i < s_LogMessages.size(); i++)
		co_yield s_LogMessages[i];
}

void tf2_bot_detector::ClearVisibleLogMessages()
{
	std::lock_guard lock(s_LogMutex);
	DebugLog("Clearing visible log messages...");
	s_VisibleLogMessagesStart = s_LogMessages.size();
}

LogMessageColor::LogMessageColor(const ImVec4& vec) :
	LogMessageColor(vec.x, vec.y, vec.z, vec.w)
{
}
