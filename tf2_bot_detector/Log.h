#pragma once

#include "Clock.h"

#include <cppcoro/generator.hpp>

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

	void Log(std::string msg, const LogMessageColor& color = {});
	void LogWarning(std::string msg);
	void LogError(std::string msg);

	void DebugLog(std::string msg, const LogMessageColor& color = { 1, 1, 1, float(2.0 / 3.0) });
	void DebugLogWarning(std::string msg);

	const std::filesystem::path& GetLogFilename();

	cppcoro::generator<const LogMessage&> GetVisibleLogMsgs();
	void ClearVisibleLogMessages();
}
