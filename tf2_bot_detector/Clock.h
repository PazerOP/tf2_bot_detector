#pragma once

#include <chrono>
#include <ctime>

namespace tf2_bot_detector
{
	using clock_t = std::chrono::system_clock;
	using time_point_t = clock_t::time_point;
	using duration_t = clock_t::duration;

	template<typename T = double, typename TRep, typename TPeriod>
	inline constexpr T to_seconds(const std::chrono::duration<TRep, TPeriod>& dur)
	{
		return std::chrono::duration_cast<std::chrono::duration<T>>(dur).count();
	}

	template<typename TClock, typename TClockDuration, typename TDurRep, typename TDurPeriod>
	inline constexpr std::chrono::time_point<TClock, TClockDuration> round_time_point(
		const std::chrono::time_point<TClock, TClockDuration>& time,
		const std::chrono::duration<TDurRep, TDurPeriod>& roundDuration)
	{
		const auto clockDur = time.time_since_epoch();

		const auto delta = time.time_since_epoch() % roundDuration;

		const auto floored = time.time_since_epoch() - delta;

		using ret_t = std::chrono::time_point<TClock, TClockDuration>;

		if (delta < (roundDuration / 2))
			return ret_t(floored);
		else
			return ret_t(floored + roundDuration);
	}

	tm ToTM(const time_point_t& ts);
	tm GetLocalTM();
	time_point_t GetLocalTimePoint();
}
