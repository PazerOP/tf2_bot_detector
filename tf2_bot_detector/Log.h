#pragma once

#include "Clock.h"

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

	void Log(std::string msg);
	void LogWarning(std::string msg);
	void LogError(std::string msg);
	void Log(std::string msg, const LogMessageColor& color);
	void SetLogTimestamp(time_point_t timestamp);

	void ForEachLogMsg(void(*msgFunc)(const LogMessage& msg, void* userData), void* userData = nullptr);
	inline void ForEachLogMsg(void(*msgFunc)(const LogMessage& msg, const void* userData), const void* userData = nullptr)
	{
		return ForEachLogMsg(reinterpret_cast<void(*)(const LogMessage&, void*)>(msgFunc), const_cast<void*>(userData));
	}

	template<typename TFunc>
	inline void ForEachLogMsg(const TFunc& func)
	{
		return ForEachLogMsg([](const LogMessage& msg, const void* userData)
			{
				(*reinterpret_cast<const TFunc*>(userData))(msg);
			}, &func);
	}
}
