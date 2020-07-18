#pragma once

#include "Clock.h"

#include <cppcoro/generator.hpp>
#include <mh/source_location.hpp>

#include <filesystem>
#include <string>

struct ImVec4;

namespace tf2_bot_detector
{
	struct LogMessageColor
	{
		constexpr LogMessageColor() = default;
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

	static constexpr LogMessageColor DEBUG_MSG_COLOR = { 1, 1, 1, float(2.0 / 3.0) };

	void Log(std::string msg, const LogMessageColor& color = {});
	void Log(const mh::source_location& location, const std::string_view& msg, const LogMessageColor& color = {});
	void LogWarning(std::string msg);
	void LogWarning(const mh::source_location& location, const std::string_view& msg);
	void LogError(std::string msg);
	void LogError(const mh::source_location& location, const std::string_view& msg);

	void DebugLog(std::string msg, const LogMessageColor& color = DEBUG_MSG_COLOR);
	void DebugLog(const mh::source_location& location, const std::string_view& msg, const LogMessageColor& color = DEBUG_MSG_COLOR);
	void DebugLogWarning(std::string msg);
	void DebugLogWarning(const mh::source_location& location, const std::string_view& msg);

	const std::filesystem::path& GetLogFilename();

	cppcoro::generator<const LogMessage&> GetVisibleLogMsgs();
	void ClearVisibleLogMessages();
}
