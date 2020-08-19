#include "Log.h"
#include "Util/PathUtils.h"

#include <imgui.h>
#include <mh/compiler.hpp>
#include <mh/text/format.hpp>
#include <mh/text/string_insertion.hpp>
#include <mh/text/stringops.hpp>

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

		void Log(std::string msg, const LogMessageColor& color, LogSeverity severity,
			LogVisibility visibility = LogVisibility::Default, time_point_t timestamp = clock_t::now()) override;
		void LogToStream(std::string msg, std::ostream& output, time_point_t timestamp = clock_t::now(), bool skipScrub = false) const;

		const std::filesystem::path& GetFileName() const override { return m_FileName; }
		cppcoro::generator<const LogMessage&> GetVisibleMsgs() const override;
		void ClearVisibleMsgs() override;

		std::ofstream& GetFile() { return m_File; }

		void LogConsoleOutput(const std::string_view& consoleOutput) override;

		void CleanupLogFiles() override;

		void AddSecret(std::string value, std::string replace) override;

	private:
		std::filesystem::path m_FileName;
		std::ofstream m_File;
		mutable std::recursive_mutex m_LogMutex;
		std::deque<LogMessage> m_LogMessages;
		size_t m_VisibleLogMessagesStart = 0;

		struct Secret
		{
			std::string m_Value;
			std::string m_Replacement;
		};
		std::vector<Secret> m_Secrets;
		void ReplaceSecrets(std::string& str) const;

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
			this->Log(mh::format("Failed to create directory {}. Log output will go to stdout.", logPath),
				COLOR_WARNING, LogSeverity::Warning);
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
		{
			this->Log(mh::format("Failed to open log file {}. Log output will go to stdout only.", m_FileName),
				COLOR_WARNING, LogSeverity::Warning);
		}
	}

	{
		auto logDir = std::filesystem::path("logs") / "console";
		std::error_code ec;
		std::filesystem::create_directories(logDir, ec);
		if (ec)
		{
			this->Log(
				mh::format("Failed to create one or more directory in the path {}. Console output will not be logged.", logDir),
				COLOR_WARNING, LogSeverity::Warning);
		}
		else
		{
			auto logPath = logDir / mh::format("console_{}.log", timestampStr);
			m_ConsoleLogFile = std::ofstream(logPath, std::ofstream::ate | std::ofstream::binary);
			if (!m_ConsoleLogFile.good())
			{
				this->Log(
					mh::format("Failed to open console log file {}. Console output will not be logged.", logPath),
					COLOR_WARNING, LogSeverity::Warning);
			}
		}
	}
}

void LogManager::LogToStream(std::string msg, std::ostream& output, time_point_t timestamp, bool skipScrub) const
{
	std::lock_guard lock(m_LogMutex);

	if (!skipScrub)
		ReplaceSecrets(msg);

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

void LogManager::AddSecret(std::string value, std::string replace)
{
	if (value.empty())
		return;

	std::lock_guard lock(m_LogMutex);
	for (auto& scrubbed : m_Secrets)
	{
		if (scrubbed.m_Value == value)
		{
			scrubbed.m_Replacement = std::move(replace);
			return;
		}
	}

	m_Secrets.push_back(Secret
		{
			.m_Value = std::move(value),
			.m_Replacement = std::move(replace)
		});
}

void LogManager::ReplaceSecrets(std::string& msg) const
{
	std::lock_guard lock(m_LogMutex);
	for (const auto& scrubbed : m_Secrets)
	{
		size_t index = msg.find(scrubbed.m_Value);
		while (index != msg.npos)
		{
			msg.erase(index, scrubbed.m_Value.size());
			msg.insert(index, scrubbed.m_Replacement);
			index = msg.find(scrubbed.m_Value, index + scrubbed.m_Replacement.size());
		}
	}
}

void tf2_bot_detector::LogException(const mh::source_location& location, const std::exception& e,
	const std::string_view& msg)
{
	LogError(location, "{}: {}: {}", msg, typeid(e).name(), e.what());
}

void LogManager::Log(std::string msg, const LogMessageColor & color,
	LogSeverity severity, LogVisibility visibility, time_point_t timestamp)
{
	std::lock_guard lock(m_LogMutex);
	ReplaceSecrets(msg);

	LogToStream(msg, m_File, timestamp, true);

	if (!(visibility == LogVisibility::Debug && !mh::is_debug))
	{
		m_LogMessages.push_back({ timestamp, std::move(msg), { color.r, color.g, color.b, color.a } });

		if (m_LogMessages.size() > MAX_LOG_MESSAGES)
		{
			m_LogMessages.erase(m_LogMessages.begin(),
				std::next(m_LogMessages.begin(), m_LogMessages.size() - MAX_LOG_MESSAGES));
		}
	}
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
