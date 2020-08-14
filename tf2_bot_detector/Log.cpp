#include "Log.h"
#include "Util/PathUtils.h"

#include <imgui.h>
#include <mh/text/format.hpp>
#include <mh/text/string_insertion.hpp>

#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

#undef max

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace tf2_bot_detector;

namespace
{
	class LogManager final : public ILogManager
	{
	public:
		LogManager();
		LogManager(const LogManager&) = delete;
		LogManager& operator=(const LogManager&) = delete;

		void Log(std::string msg, const LogMessageColor& color = {}, time_point_t timestamp = clock_t::now()) override;
		void LogToStream(const std::string_view& msg, std::ostream& output, time_point_t timestamp = clock_t::now()) const;

		const std::filesystem::path& GetFileName() const override { return m_FileName; }
		cppcoro::generator<const LogMessage&> GetVisibleMsgs() const override;
		void ClearVisibleMsgs() override;

		std::ofstream& GetFile() { return m_File; }

		void LogConsoleOutput(const std::string_view& consoleOutput) override;

		void CleanupLogFiles() override;

	private:
		std::filesystem::path m_FileName;
		std::ofstream m_File;
		mutable std::recursive_mutex m_LogMutex;
		std::deque<LogMessage> m_LogMessages;
		size_t m_VisibleLogMessagesStart = 0;

		static constexpr size_t MAX_LOG_MESSAGES = 500;

		mutable std::recursive_mutex m_ConsoleLogMutex;
		std::ofstream m_ConsoleLogFile;
	};

	static LogManager& GetLogState()
	{
		static LogManager s_LogState;
		return s_LogState;
	}
}

ILogManager& ILogManager::GetInstance()
{
	return ::GetLogState();
}

static constexpr LogMessageColor COLOR_DEFAULT = { 1, 1, 1, 1 };
static constexpr LogMessageColor COLOR_WARNING = { 1, 0.5, 0, 1 };
static constexpr LogMessageColor COLOR_ERROR = { 1, 0.25, 0, 1 };

LogManager::LogManager()
{
	const auto t = ToTM(clock_t::now());
	const auto timestampStr = mh::format("{}", std::put_time(&t, "%Y-%m-%d_%H-%M-%S"));

	// Pick file name
	{
		std::filesystem::path logPath = "logs";

		std::error_code ec;
		std::filesystem::create_directories(logPath, ec);
		if (ec)
		{
			Log("Failed to create directory "s << logPath << ". Log output will go to stdout.", COLOR_WARNING);
		}
		else
		{
			m_FileName = logPath / mh::format("{}.log", timestampStr);
		}
	}

	// Try open file
	if (!m_FileName.empty())
	{
		m_File = std::ofstream(m_FileName, std::ofstream::ate | std::ofstream::app | std::ofstream::out | std::ofstream::binary);
		if (!m_File.good())
			Log("Failed to open log file "s << m_FileName << ". Log output will go to stdout only.", COLOR_WARNING);
	}

	{
		auto logDir = std::filesystem::path("logs") / "console";
		std::error_code ec;
		std::filesystem::create_directories(logDir, ec);
		if (ec)
		{
			Log(mh::format("Failed to create one or more directory in the path {}. Console output will not be logged.", logDir), COLOR_WARNING);
		}
		else
		{
			auto logPath = logDir / mh::format("console_{}.log", timestampStr);
			m_ConsoleLogFile = std::ofstream(logPath, std::ofstream::ate | std::ofstream::binary);
			if (!m_ConsoleLogFile.good())
				Log(mh::format("Failed to open console log file {}. Console output will not be logged.", logPath), COLOR_WARNING);
		}
	}
}

void LogManager::LogToStream(const std::string_view& msg, std::ostream& output, time_point_t timestamp) const
{
	std::lock_guard lock(m_LogMutex);

	tm t = ToTM(timestamp);
	const auto WriteToStream = [&](std::ostream& str)
	{
		str << '[' << std::put_time(&t, "%T") << "] " << msg << std::endl;
	};

	WriteToStream(output);
	if (&output != &std::cout)
		WriteToStream(std::cout);

#ifdef _WIN32
	OutputDebugStringA(mh::format("Log: {}\n", msg).c_str());
#endif
}

void LogManager::Log(std::string msg, const LogMessageColor& color, time_point_t timestamp)
{
	std::lock_guard lock(m_LogMutex);
	LogToStream(msg, m_File, timestamp);
	m_LogMessages.push_back({ timestamp, std::move(msg), { color.r, color.g, color.b, color.a } });

	if (m_LogMessages.size() > MAX_LOG_MESSAGES)
	{
		m_LogMessages.erase(m_LogMessages.begin(),
			std::next(m_LogMessages.begin(), m_LogMessages.size() - MAX_LOG_MESSAGES));
	}
}

namespace
{
	template<typename CharT, typename Traits>
	std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const mh::source_location& location)
	{
		return os << location.file_name() << '@' << location.line() << ':' << location.function_name() << "()";
	}
}

void tf2_bot_detector::Log(std::string msg, const LogMessageColor& color)
{
	GetLogState().Log(std::move(msg), color);
}

static std::string FormatSourceLocation(const mh::source_location& location, const std::string_view& msg)
{
	std::string fullMsg;
	fullMsg << location;

	if (!msg.empty())
		fullMsg << ": " << msg;

	return fullMsg;
}

void tf2_bot_detector::Log(const mh::source_location& location, const std::string_view& msg, const LogMessageColor& color)
{
	Log(FormatSourceLocation(location, msg), color);
}

void tf2_bot_detector::LogWarning(std::string msg)
{
	Log(std::move(msg), COLOR_WARNING);
}

void tf2_bot_detector::LogWarning(const mh::source_location& location, const std::string_view& msg)
{
	Log(location, msg, COLOR_WARNING);
}

void tf2_bot_detector::LogError(std::string msg)
{
	Log(std::move(msg), COLOR_ERROR);
}

void tf2_bot_detector::LogError(const mh::source_location& location, const std::string_view& msg)
{
	Log(location, msg, COLOR_ERROR);
}

static std::string FormatException(const std::string_view& msg, const std::exception& e)
{
	return mh::format("{}: {}: {}", msg, typeid(e).name(), e.what());
}

void tf2_bot_detector::LogException(const std::string_view& msg, const std::exception& e)
{
	LogError(FormatException(msg, e));
}

void tf2_bot_detector::LogException(const mh::source_location& location, const std::string_view& msg,
	const std::exception& e)
{
	LogError(location, FormatException(msg, e));
}

void tf2_bot_detector::DebugLog(std::string msg, const LogMessageColor& color)
{
#ifdef _DEBUG
	Log(std::move(msg), color);
#else
	auto& state = GetLogState();
	state.LogToStream(msg, state.GetFile());
#endif
}

void tf2_bot_detector::DebugLog(const mh::source_location& location, const std::string_view& msg, const LogMessageColor& color)
{
	DebugLog(FormatSourceLocation(location, msg), color);
}

void tf2_bot_detector::DebugLogWarning(std::string msg)
{
	DebugLog(std::move(msg), { COLOR_WARNING.r, COLOR_WARNING.g, COLOR_WARNING.b, 0.67f });
}

void tf2_bot_detector::DebugLogWarning(const mh::source_location& location, const std::string_view& msg)
{
	DebugLogWarning(FormatSourceLocation(location, msg));
}

cppcoro::generator<const LogMessage&> LogManager::GetVisibleMsgs() const
{
	std::lock_guard lock(m_LogMutex);

	size_t start = m_VisibleLogMessagesStart;
	if (m_LogMessages.size() > MAX_LOG_MESSAGES)
		start = std::max(start, m_LogMessages.size() - MAX_LOG_MESSAGES);

	for (size_t i = start; i < m_LogMessages.size(); i++)
		co_yield m_LogMessages[i];
}

void LogManager::ClearVisibleMsgs()
{
	std::lock_guard lock(m_LogMutex);
	DebugLog("Clearing visible log messages...");
	m_VisibleLogMessagesStart = m_LogMessages.size();
}

void LogManager::LogConsoleOutput(const std::string_view& consoleOutput)
{
	std::lock_guard lock(m_ConsoleLogMutex);
	m_ConsoleLogFile << consoleOutput << std::flush;
}

void LogManager::CleanupLogFiles() try
{
	constexpr auto MAX_LOG_LIFETIME = 24h * 7;
	{
		std::lock_guard lock(m_LogMutex);
		DeleteOldFiles("logs", MAX_LOG_LIFETIME);
	}

	{
		std::lock_guard lock(m_ConsoleLogMutex);
		DeleteOldFiles("logs/console", MAX_LOG_LIFETIME);
	}
}
catch (const std::filesystem::filesystem_error& e)
{
	LogError(MH_SOURCE_LOCATION_CURRENT(), e.what());
}

LogMessageColor::LogMessageColor(const ImVec4& vec) :
	LogMessageColor(vec.x, vec.y, vec.z, vec.w)
{
}
