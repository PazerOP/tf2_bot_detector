#pragma once

#include "Clock.h"

#include <mh/coroutine/generator.hpp>
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
		Fatal,
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

		virtual mh::generator<const LogMessage&> GetVisibleMsgs() const = 0;
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
		NOINLINE void LogImplBase(const LogMessageColor& color, LogSeverity severity, LogVisibility visibility,
			const std::string_view& fmtStr, const mh::format_args& args);
		NOINLINE void LogImplBase(const LogMessageColor& color, LogSeverity severity, LogVisibility visibility,
			const mh::source_location& location, const std::string_view& fmtStr, const mh::format_args& args);
		template<typename... TArgs, typename = std::enable_if_t<(sizeof...(TArgs) > 0)>>
		NOINLINE inline auto LogImpl(const LogMessageColor& color, LogSeverity severity, LogVisibility visibility,
			const std::string_view& fmtStr, const TArgs&... args) ->
			decltype(mh::try_format(fmtStr, args...), void())
		{
			LogImplBase(color, severity, visibility, fmtStr, mh::make_format_args(args...));
		}
		template<typename... TArgs>
		NOINLINE inline auto LogImpl(const LogMessageColor& color, LogSeverity severity, LogVisibility visibility,
			const mh::source_location& location, const std::string_view& fmtStr, const TArgs&... args) ->
			decltype(mh::try_format(fmtStr, args...), void())
		{
			LogImplBase(color, severity, visibility, location, fmtStr, mh::make_format_args(args...));
		}

		struct src_location_wrapper
		{
			template<typename T, typename = std::enable_if_t<std::is_constructible_v<std::string_view, T>>>
			constexpr src_location_wrapper(const T& value, MH_SOURCE_LOCATION_AUTO(location)) :
				m_Value(value), m_Location(location)
			{
			}

			std::string_view m_Value;
			mh::source_location m_Location;
		};
	}

#undef LOG_DEFINITION_HELPER
#define LOG_DEFINITION_HELPER(name, defaultColor, severity, visibility) \
	template<typename... TArgs> \
	inline auto name(const LogMessageColor& color, const mh::source_location& location, const std::string_view& fmtStr, const TArgs&... args) \
	{ \
		detail::log_h::LogImpl(color, (severity), (visibility), location, fmtStr, args...); \
	} \
	template<typename... TArgs> \
	inline auto name(const LogMessageColor& color, const detail::log_h::src_location_wrapper& fmtStr, const TArgs&... args) \
	{ \
		name(color, fmtStr.m_Location, fmtStr.m_Value, args...); \
	} \
	template<typename... TArgs> \
	inline auto name(const mh::source_location& location, const std::string_view& fmtStr, const TArgs&... args) \
	{ \
		name((defaultColor), location, fmtStr, args...); \
	} \
	template<typename... TArgs> \
	inline auto name(const detail::log_h::src_location_wrapper& fmtStr, const TArgs&... args) \
	{ \
		name(fmtStr.m_Location, fmtStr.m_Value, args...); \
	} \
	inline void name(const LogMessageColor& color, const std::string_view& msg, MH_SOURCE_LOCATION_AUTO(location)) \
	{ \
		name(color, location, msg); \
	} \
	inline void name(const std::string_view& msg, MH_SOURCE_LOCATION_AUTO(location)) \
	{ \
		name(location, msg); \
	} \
	inline void name(const LogMessageColor& color, MH_SOURCE_LOCATION_AUTO(location)) \
	{ \
		name(color, location, std::string_view{}); \
	} \
	inline void name(MH_SOURCE_LOCATION_AUTO(location)) \
	{ \
		name((defaultColor), location); \
	}

	LOG_DEFINITION_HELPER(Log, LogColors::DEFAULT, LogSeverity::Info, LogVisibility::Default);
	LOG_DEFINITION_HELPER(DebugLog, LogColors::DEFAULT_DEBUG, LogSeverity::Info, LogVisibility::Debug);
	LOG_DEFINITION_HELPER(LogWarning, LogColors::WARN, LogSeverity::Warning, LogVisibility::Default);
	LOG_DEFINITION_HELPER(DebugLogWarning, LogColors::WARN_DEBUG, LogSeverity::Warning, LogVisibility::Debug);
	LOG_DEFINITION_HELPER(LogError, LogColors::ERROR, LogSeverity::Error, LogVisibility::Default);

#undef LOG_DEFINITION_HELPER

	void LogException(const mh::source_location& location, const std::exception_ptr& e,
		LogSeverity severity, LogVisibility visibility, const std::string_view& msg = {});

#define LOG_DEFINITION_HELPER(name, attr, severity, visibility) \
	template<typename... TArgs> \
	attr void name(const mh::source_location& location, const std::exception_ptr& e, \
		const std::string_view& fmtStr = {}, const TArgs&... args) \
	{ \
		LogException(location, e, severity, visibility, mh::try_format(fmtStr, args...)); \
	} \
	\
	template<typename... TArgs> \
	attr void name(const mh::source_location& location, const std::exception& e, \
		const std::string_view& fmtStr = {}, const TArgs&... args) \
	{ \
		name(location, std::make_exception_ptr(e), fmtStr, args...); \
	} \
	\
	template<typename... TArgs> \
	attr void name(const mh::source_location& location, const std::string_view& fmtStr = {}, const TArgs&... args) \
	{ \
		name(location, std::current_exception(), fmtStr, args...); \
	}

	LOG_DEFINITION_HELPER(DebugLogException, , LogSeverity::Error, LogVisibility::Debug);
	LOG_DEFINITION_HELPER(LogException, , LogSeverity::Error, LogVisibility::Default);
	LOG_DEFINITION_HELPER(LogFatalException, [[noreturn]], LogSeverity::Fatal, LogVisibility::Default);

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
