#pragma once

#include "Clock.h"

#include <cppcoro/generator.hpp>
#include <mh/text/format.hpp>
#include <mh/source_location.hpp>

#include <filesystem>
#include <string>

struct ImVec4;

namespace tf2_bot_detector
{
	struct LogMessageColor
	{
		constexpr LogMessageColor() = default;
		constexpr LogMessageColor(const LogMessageColor& rgb, float a_) :
			r(rgb.r), g(rgb.g), b(rgb.b), a(a_)
		{
		}
		constexpr LogMessageColor(float r_, float g_, float b_, float a_ = 1) :
			r(r_), g(g_), b(b_), a(a_)
		{
		}
		LogMessageColor(const ImVec4& vec);

		float r = 1;
		float g = 1;
		float b = 1;
		float a = 1;
	};

	struct LogMessage
	{
		time_point_t m_Timestamp;
		std::string m_Text;
		LogMessageColor m_Color;
	};

	namespace LogColors
	{
		constexpr float DEBUG_ALPHA = float(2.0 / 3.0);
		constexpr LogMessageColor DEFAULT = { 1, 1, 1, 1 };
		constexpr LogMessageColor DEFAULT_DEBUG = { DEFAULT, DEBUG_ALPHA };
		constexpr LogMessageColor WARN = { 1, 1, 0, 1 };
		constexpr LogMessageColor WARN_DEBUG = { WARN, DEBUG_ALPHA };
#undef ERROR
		constexpr LogMessageColor ERROR = { 1, 0.25, 0, 1 };
	}

	enum class LogSeverity
	{
		Info,
		Warning,
		Error,
	};

	enum class LogVisibility
	{
		Default,
		Debug,
	};

	class ILogManager
	{
	public:
		virtual ~ILogManager() = default;

		static ILogManager& GetInstance();

		virtual void Log(std::string msg, const LogMessageColor& color,
			LogSeverity severity, LogVisibility visibility,
			time_point_t timestamp = clock_t::now()) = 0;

		virtual const std::filesystem::path& GetFileName() const = 0;

		virtual cppcoro::generator<const LogMessage&> GetVisibleMsgs() const = 0;
		virtual void ClearVisibleMsgs() = 0;

		virtual void LogConsoleOutput(const std::string_view& consoleOutput) = 0;

		virtual void CleanupLogFiles() = 0;

		virtual void AddSecret(std::string value, std::string replace) = 0;
	};

#pragma push_macro("NOINLINE")
#undef NOINLINE
#ifdef _MSC_VER
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE
#endif

	namespace detail::log_h
	{
		template<typename... TArgs, typename = std::enable_if_t<(sizeof...(TArgs) > 0)>>
		NOINLINE inline void LogImpl(const LogMessageColor& color, LogSeverity severity, LogVisibility visibility,
			const std::string_view& fmtStr, const TArgs&... args)
		{
			ILogManager::GetInstance().Log(mh::try_format(fmtStr, args...), color, severity, visibility);
		}
		template<typename... TArgs>
		NOINLINE inline void LogImpl(const LogMessageColor& color, LogSeverity severity, LogVisibility visibility,
			const mh::source_location& location, const std::string_view& fmtStr, const TArgs&... args)
		{
			using namespace std::string_view_literals;
			if (!fmtStr.empty())
				LogImpl(color, severity, visibility, "{}: {}"sv, location, mh::try_format(fmtStr, args...));
			else
				LogImpl(color, severity, visibility, "{}"sv, location);
		}
	}

#undef LOG_DEFINITION_HELPER
#define LOG_DEFINITION_HELPER(name, defaultColor, severity, visibility) \
	template<typename... TArgs, typename = std::enable_if_t<(sizeof...(TArgs) > 0)>> \
	inline void name(const LogMessageColor& color, const std::string_view& fmtStr, const TArgs&... args) \
	{ \
		detail::log_h::LogImpl(color, (severity), (visibility), fmtStr, args...); \
	} \
	inline void name(const LogMessageColor& color, std::string msg) \
	{ \
		ILogManager::GetInstance().Log(std::move(msg), color, (severity), (visibility)); \
	} \
	template<typename... TArgs, typename = std::enable_if_t<(sizeof...(TArgs) > 0)>> \
	inline void name(const std::string_view& fmtStr, const TArgs&... args) \
	{ \
		name((defaultColor), fmtStr, args...); \
	} \
	inline void name(std::string msg) \
	{ \
		name((defaultColor), std::move(msg)); \
	} \
	\
	template<typename... TArgs> \
	NOINLINE inline void name(const LogMessageColor& color, const mh::source_location& location, const std::string_view& fmtStr, const TArgs&... args) \
	{ \
		detail::log_h::LogImpl(color, (severity), (visibility), location, fmtStr, args...); \
	} \
	template<typename... TArgs> \
	inline void name(const mh::source_location& location, const std::string_view& fmtStr, const TArgs&... args) \
	{ \
		name((defaultColor), location, fmtStr, args...); \
	} \
	inline void name(const LogMessageColor& color, const mh::source_location& location) \
	{ \
		name(color, location, std::string_view{}); \
	} \
	inline void name(const mh::source_location& location) \
	{ \
		name((defaultColor), location); \
	}

	LOG_DEFINITION_HELPER(Log, LogColors::DEFAULT, LogSeverity::Info, LogVisibility::Default);
	LOG_DEFINITION_HELPER(DebugLog, LogColors::DEFAULT_DEBUG, LogSeverity::Info, LogVisibility::Debug);
	LOG_DEFINITION_HELPER(LogWarning, LogColors::WARN, LogSeverity::Warning, LogVisibility::Default);
	LOG_DEFINITION_HELPER(DebugLogWarning, LogColors::WARN_DEBUG, LogSeverity::Warning, LogVisibility::Debug);
	LOG_DEFINITION_HELPER(LogError, LogColors::ERROR, LogSeverity::Error, LogVisibility::Default);

#undef LOG_DEFINITION_HELPER

#define LOG_DEFINITION_HELPER(name, attr) \
	attr void name(const mh::source_location& location, const std::exception& e, const std::string_view& msg = {}); \
	\
	template<typename... TArgs, typename = std::enable_if_t<(sizeof...(TArgs) > 0)>> \
	attr void name(const mh::source_location& location, const std::exception& e, \
		const std::string_view& fmtStr, const TArgs&... args) \
	{ \
		name(location, e, mh::try_format(fmtStr, args...)); \
	}

	LOG_DEFINITION_HELPER(DebugLogException, );
	LOG_DEFINITION_HELPER(LogException, );
	LOG_DEFINITION_HELPER(LogFatalException, [[noreturn]]);

#undef LOG_DEFINITION_HELPER

#undef NOINLINE
#pragma pop_macro("NOINLINE")

	[[noreturn]] void LogFatalError(const mh::source_location& location, const std::string_view& msg);
	template<typename... TArgs, typename = std::enable_if_t<(sizeof...(TArgs) > 0)>>
	[[noreturn]] void LogFatalError(const mh::source_location& location,
			const std::string_view& fmtStr, const TArgs&... args)
	{
		LogFatalError(location, mh::try_format(fmtStr, args...));
	}
}
