#include "Clock.h"

using namespace tf2_bot_detector;

tm tf2_bot_detector::ToTM(const time_point_t& ts)
{
	tm t{};
	const auto ts_timet = clock_t::to_time_t(ts);

#ifdef _WIN32
	localtime_s(&t, &ts_timet);
#else
	if (auto tPtr = std::localtime(&ts_timet))
		t = *tPtr;
#endif

	return t;
}

tm tf2_bot_detector::GetLocalTM()
{
	tm retVal{};

	auto time = std::time(nullptr);
#ifdef _WIN32
	localtime_s(&retVal, &time);
#else
	if (auto tPtr = std::localtime(&time))
		retVal = *tPtr;
#endif

	return retVal;
}

time_point_t tf2_bot_detector::GetLocalTimePoint()
{
	return clock_t::from_time_t(std::time(nullptr));
}
