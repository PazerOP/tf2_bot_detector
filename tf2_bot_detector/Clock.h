#pragma once

#include <chrono>

namespace tf2_bot_detector
{
	using clock_t = std::chrono::steady_clock;
	using time_point_t = clock_t::time_point;
	using duration_t = clock_t::duration;
}
