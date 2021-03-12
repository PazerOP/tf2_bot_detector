#include "Clock.h"

#include <mh/chrono/chrono_helpers.hpp>
#include <mh/math/interpolation.hpp>

using namespace tf2_bot_detector;

tm tf2_bot_detector::ToTM(const time_point_t& ts)
{
	return mh::chrono::to_tm(ts, mh::chrono::time_zone::local);
}

tm tf2_bot_detector::GetLocalTM()
{
	return mh::chrono::current_tm(mh::chrono::time_zone::local);
}

time_point_t tf2_bot_detector::GetLocalTimePoint()
{
	return mh::chrono::current_time_point();
}

float tf2_bot_detector::TimeSine(float interval, float min, float max)
{
	const auto elapsed = clock_t::now().time_since_epoch() % std::chrono::duration_cast<clock_t::duration>(std::chrono::duration<float>(interval));
	const auto progress = std::chrono::duration<float>(elapsed).count() / interval;
	return mh::remap(std::sin(progress * 6.28318530717958647693f), -1.0f, 1.0f, min, max);
}
