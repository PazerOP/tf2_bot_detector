#pragma once

#include <chrono>

namespace tf2_bot_detector
{
	using clock_t = std::chrono::system_clock;
	using time_point_t = clock_t::time_point;
	using duration_t = clock_t::duration;

	template<typename T = double, typename TRep, typename TPeriod>
	inline T to_seconds(const std::chrono::duration<TRep, TPeriod>& dur)
	{
		return std::chrono::duration_cast<std::chrono::duration<T>>(dur).count();
	}
}
