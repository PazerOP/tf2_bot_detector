#include "Log.h"

#include <imgui.h>

#include <mutex>
#include <vector>

using namespace tf2_bot_detector;

static tf2_bot_detector::time_point_t s_LogTimestamp;
static std::recursive_mutex s_LogMutex;
static std::vector<LogMessage> s_LogMessages;

void tf2_bot_detector::Log(std::string msg)
{
	return Log(std::move(msg), { 1, 1, 1, 1 });
}

void tf2_bot_detector::Log(std::string msg, const LogMessageColor& color)
{
	std::lock_guard lock(s_LogMutex);
	s_LogMessages.push_back({ s_LogTimestamp, std::move(msg), { color.r, color.g, color.b, color.a } });
}

void tf2_bot_detector::SetLogTimestamp(time_point_t timestamp)
{
	s_LogTimestamp = timestamp;
}

void tf2_bot_detector::ForEachLogMsg(void(*msgFunc)(const LogMessage& msg, void* userData), void* userData)
{
	std::lock_guard lock(s_LogMutex);

	for (const auto& msg : s_LogMessages)
		msgFunc(msg, userData);
}

LogMessageColor::LogMessageColor(const ImVec4& vec) :
	LogMessageColor(vec.x, vec.y, vec.z, vec.w)
{
}
