#include "Log.h"
#include "Util/PathUtils.h"
#include "Filesystem.h"

#include <imgui.h>
#include <mh/compiler.hpp>
#include <mh/error/exception_details.hpp>
#include <mh/text/codecvt.hpp>
#include <mh/text/fmtstr.hpp>
#include <mh/text/format.hpp>
#include <mh/text/string_insertion.hpp>
#include <mh/text/stringops.hpp>
#include <SDL2/SDL_messagebox.h>

#include <atomic>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

#undef ERROR
#undef max

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace tf2_bot_detector;

namespace
{
	class LogManager final : public ILogManager
	{
	public:
		void Init() override;

		void Log(std::string msg, const LogMessageColor& color, LogSeverity severity,
			LogVisibility visibility = LogVisibility::Default, time_point_t timestamp = tfbd_clock_t::now()) override;
		void LogToStream(std::string msg, std::ostream& output, time_point_t timestamp = tfbd_clock_t::now(), bool skipScrub = false) const;

		const std::filesystem::path& GetFileName() const override { return m_FileName; }
		mh::generator<const LogMessage&> GetVisibleMsgs() const override;
		void ClearVisibleMsgs() override;

		std::ostream& GetLogStream();

		void LogConsoleOutput(const std::string_view& consoleOutput) override;

		void CleanupLogFiles() override;

		void AddSecret(std::string value, std::string replace) override;

	private:
		bool m_IsInit = false;
		void EnsureInit(MH_SOURCE_LOCATION_AUTO(location)) const;

		std::filesystem::path m_FileName;
		std::optional<std::stringstream> m_TempLogs = std::stringstream();   // Logs before we have been initialized
		std::optional<std::ofstream> m_File;
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

void LogManager::Init()
{
	assert(!m_IsInit);

	if (!m_IsInit)
	{
		::DebugLog("Initializing LogManager...");
		std::lock_guard lock(m_LogMutex);

		const auto t = ToTM(tfbd_clock_t::now());
		const mh::fmtstr<128> timestampStr("{}", std::put_time(&t, "%Y-%m-%d_%H-%M-%S"));

		// Pick file name
		{
			std::filesystem::path logPath = IFilesystem::Get().GetLogsDir();

			std::error_code ec;
			std::filesystem::create_directories(logPath, ec);
			if (ec)
			{
				::LogWarning("Failed to create one or more directory in the path {}. Log output will go to stdout.",
					logPath);
			}
			else
			{
				m_FileName = logPath / mh::fmtstr<128>("{}.log", timestampStr).view();
			}
		}

		// Try open file
		if (!m_FileName.empty())
		{
			m_File = std::ofstream(m_FileName, std::ofstream::ate | std::ofstream::app | std::ofstream::out | std::ofstream::binary);
			if (!m_File->good())
			{
				::LogWarning("Failed to open log file {}. Log output will go to stdout only.", m_FileName);
			}
			else
			{
				// Dump all log messages being held in memory to the file, if it was successfully opened
				m_File.value() << m_TempLogs.value().str();
				m_TempLogs.reset();

				::DebugLog("Dumped all pending log messages to {}.", m_FileName);
			}
		}

		{
			auto logDir = std::filesystem::path("logs") / "console";
			std::error_code ec;
			std::filesystem::create_directories(logDir, ec);
			if (ec)
			{
				::LogWarning("Failed to create one or more directory in the path {}. Console output will not be logged.",
					logDir);
			}
			else
			{
				auto logPath = logDir / mh::fmtstr<128>("console_{}.log", timestampStr).view();
				m_ConsoleLogFile = std::ofstream(logPath, std::ofstream::ate | std::ofstream::binary);
				if (!m_ConsoleLogFile.good())
					::LogWarning("Failed to open console log file {}. Console output will not be logged.", logPath);
			}
		}

		m_IsInit = true;
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
	EnsureInit();

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

void tf2_bot_detector::LogFatalError(const mh::source_location& location, const std::string_view& msg)
{
	LogError(location, msg);

	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal error",
		mh::format(
R"({}

Error source filename: {}:{}
Error source function: {})"
		, msg, location.file_name(), location.line(), location.function_name()
		).c_str(), nullptr);

	std::exit(1);
}

void tf2_bot_detector::LogException(const mh::source_location& location, const std::exception_ptr& e,
	LogSeverity severity, LogVisibility visibility, const std::string_view& msg)
{
	const mh::exception_details details(e);

	LogMessageColor color = LogColors::EXCEPTION;
	if (visibility == LogVisibility::Debug)
		color = color.WithAlpha(LogColors::DEBUG_ALPHA);

	detail::log_h::LogImpl(LogColors::ERROR, severity, visibility, location,
		msg.empty() ? "{1}: {2}"sv : "{0}: {1}: {2}"sv, msg, details.type_name(), details.m_Message);

	if (severity == LogSeverity::Fatal)
	{
		auto dialogText = mh::format(
			R"({}

Exception type: {}
Exception message: {}

Exception source filename: {}:{}
Exception source function: {})",

msg,

details.type_name(),
details.m_Message,

location.file_name(), location.line(),
location.function_name());

		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Unhandled exception", dialogText.c_str(), nullptr);

		std::exit(1);
	}
}

void detail::log_h::LogImpl(const LogMessageColor& color, LogSeverity severity, LogVisibility visibility, std::string str)
{
	ILogManager::GetInstance().Log(std::move(str), color, severity, visibility);
}

void tf2_bot_detector::detail::log_h::LogImpl(const LogMessageColor& color, LogSeverity severity, LogVisibility visibility,
	const mh::source_location& location, const std::string_view& str)
{
	LogImpl(color, severity, visibility, mh::format(MH_FMT_STRING("{}: {}"sv), location, str));
}

void detail::log_h::LogImplBase(const LogMessageColor& color, LogSeverity severity, LogVisibility visibility,
	const std::string_view& fmtStr, const mh::format_args& args)
{
	LogImpl(color, severity, visibility, mh::try_vformat(fmtStr, args));
}

void detail::log_h::LogImplBase(const LogMessageColor& color, LogSeverity severity, LogVisibility visibility,
	const mh::source_location& location, const std::string_view& fmtStr, const mh::format_args& args)
{
	LogImpl(color, severity, visibility, location, mh::try_vformat(fmtStr, args));
}

void LogManager::Log(std::string msg, const LogMessageColor& color,
	LogSeverity severity, LogVisibility visibility, time_point_t timestamp)
{
	std::lock_guard lock(m_LogMutex);
	ReplaceSecrets(msg);

	LogToStream(msg, GetLogStream(), timestamp, true);

	if (!(visibility == LogVisibility::Debug && !mh::is_debug))
	{
		m_LogMessages.push_back({ timestamp, std::move(msg), { color.r, color.g, color.b, color.a } });

		if (m_IsInit && m_LogMessages.size() > MAX_LOG_MESSAGES)
		{
			m_LogMessages.erase(m_LogMessages.begin(),
				std::next(m_LogMessages.begin(), m_LogMessages.size() - MAX_LOG_MESSAGES));
		}
	}
}

mh::generator<const LogMessage&> LogManager::GetVisibleMsgs() const
{
	EnsureInit();

	std::lock_guard lock(m_LogMutex);

	size_t start = m_VisibleLogMessagesStart;
	if (m_LogMessages.size() > MAX_LOG_MESSAGES)
		start = std::max(start, m_LogMessages.size() - MAX_LOG_MESSAGES);

	for (size_t i = start; i < m_LogMessages.size(); i++)
		co_yield m_LogMessages[i];
}

void LogManager::ClearVisibleMsgs()
{
	EnsureInit();

	std::lock_guard lock(m_LogMutex);
	DebugLog("Clearing visible log messages...");
	m_VisibleLogMessagesStart = m_LogMessages.size();
}

std::ostream& LogManager::GetLogStream() try
{
	if (m_File.has_value())
		return *m_File;

	return m_TempLogs.value();
}
catch (...)
{
	LogFatalException();
}

void LogManager::LogConsoleOutput(const std::string_view& consoleOutput)
{
	EnsureInit();

	std::lock_guard lock(m_ConsoleLogMutex);
	m_ConsoleLogFile << consoleOutput << std::flush;
}

void LogManager::CleanupLogFiles() try
{
	EnsureInit();

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

void LogManager::EnsureInit(const mh::source_location& location) const
{
	if (!m_IsInit)
	{
		assert(false);
		LogFatalError(location, "LogManager::EnsureInit failed");
	}
}

LogMessageColor::LogMessageColor(const ImVec4& vec) :
	LogMessageColor(vec.x, vec.y, vec.z, vec.w)
{
}

#define LOG_DEFINITION_HELPER(name, defaultColor, severity, visibility) \
	void name(const mh::source_location& location) \
	{ \
		name((defaultColor), location); \
	} \
	void name(const LogMessageColor& color, const std::string_view& msg, const mh::source_location& location) \
	{ \
		name(color, location, msg); \
	} \
	void name(const std::string_view& msg, const mh::source_location& location) \
	{ \
		name(location, msg); \
	} \
	void name(const LogMessageColor& color, const mh::source_location& location) \
	{ \
		name(color, location, std::string_view{}); \
	} \

namespace tf2_bot_detector
{
	LOG_DEFINITION_HELPER(Log, LogColors::DEFAULT, LogSeverity::Info, LogVisibility::Default);
	LOG_DEFINITION_HELPER(DebugLog, LogColors::DEFAULT_DEBUG, LogSeverity::Info, LogVisibility::Debug);
	LOG_DEFINITION_HELPER(LogWarning, LogColors::WARN, LogSeverity::Warning, LogVisibility::Default);
	LOG_DEFINITION_HELPER(DebugLogWarning, LogColors::WARN_DEBUG, LogSeverity::Warning, LogVisibility::Debug);
	LOG_DEFINITION_HELPER(LogError, LogColors::ERROR, LogSeverity::Error, LogVisibility::Default);
}
#undef LOG_DEFINITION_HELPER

#define LOG_DEFINITION_HELPER(name, severity, visibility) \
	void name(const mh::source_location& location) \
	{ \
		name(location, std::string_view{}); \
	} \
	void name(const std::exception& e, const mh::source_location& location) \
	{ \
		name(location, e, std::string_view{}); \
	}

namespace tf2_bot_detector
{
	LOG_DEFINITION_HELPER(DebugLogException, LogSeverity::Error, LogVisibility::Debug);
	LOG_DEFINITION_HELPER(LogException, LogSeverity::Error, LogVisibility::Default);
	LOG_DEFINITION_HELPER(LogFatalException, LogSeverity::Fatal, LogVisibility::Default);
}
#undef LOG_DEFINITION_HELPER
