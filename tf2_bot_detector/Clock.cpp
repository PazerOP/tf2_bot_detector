#include "Clock.h"

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
