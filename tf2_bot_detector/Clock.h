#pragma once

#include <chrono>
#include <ctime>
#include <ostream>

namespace tf2_bot_detector
{
	using clock_t = std::chrono::system_clock;
	using time_point_t = clock_t::time_point;
	using duration_t = clock_t::duration;

	namespace detail::Clock_h
	{
		template<intmax_t num, intmax_t den, typename TRep = std::chrono::seconds::rep>
		using make_duration_t = std::chrono::duration<TRep, std::ratio<num, den>>;
	}

	using second_t = std::chrono::seconds;
	using minute_t = std::chrono::minutes;
	using hour_t = std::chrono::hours;
	//using test = hour_t::
	using day_t = std::chrono::duration<hour_t::rep, std::ratio_multiply<hour_t::period, std::ratio<24>>>;
	using week_t = std::chrono::duration<day_t::rep, std::ratio_multiply<day_t::period, std::ratio<7>>>;
	using year_t = std::chrono::duration<day_t::rep, std::ratio_multiply<day_t::period, std::ratio<365>>>;
	using month_t = std::chrono::duration<year_t::rep, std::ratio_divide<year_t::period, std::ratio<12>>>;

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

	template<typename TRep, typename TPeriod>
	struct HumanDuration
	{
		constexpr HumanDuration(std::chrono::duration<TRep, TPeriod> duration) : m_Duration(duration) {}

		std::chrono::duration<TRep, TPeriod> m_Duration{};
	};
}

template<typename CharT, typename Traits, typename TRep, typename TPeriod>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os,
	const tf2_bot_detector::HumanDuration<TRep, TPeriod>& humanDuration)
{
#if 0
	using namespace std::chrono_literals;

	constexpr auto SECOND = 1s;
	constexpr auto MINUTE = 1min;
	constexpr auto HOUR = 1h;
	constexpr auto DAY = HOUR * 24;
	constexpr auto WEEK = DAY * 7;
	constexpr auto YEAR = DAY * 365;
	constexpr auto MONTH = YEAR / 12;

	const auto& duration = humanDuration.m_Duration;

	int printCount = 0;

	const auto Print = [&](auto& value, const auto divisor, const auto& text)
	{
		if (printCount >= 3)
			return;

		const auto divided = value / divisor;
		const auto count = (value / divisor).count();
		if (count <= 0)
			return;

		if (printCount > 0)
			os << ", ";

		os << count << ' ' << text;

		if (count > 1)
			os << 's';

		printCount++;
		value -=
	};

	Print(duration, YEAR, "year");
	Print(duration, MONTH, "month");
	Print(duration, WEEK, "week");
	Print(duration, DAY, "day");
	Print(duration, HOUR, "hour");
	Print(duration, MINUTE, "minute");
	Print(duration, SECOND, "second");

	return os;
#else
	int printCount = 0;

	const auto Print = [&](auto& value, auto period, const auto& text)
	{
		if (printCount >= 3)
			return;

		using TPeriod = std::decay_t<decltype(period)>;
		const auto truncated = std::chrono::duration_cast<TPeriod>(value);
		const auto count = truncated.count();
		if (count <= 0)
			return;

		if (printCount > 0)
			os << ", ";

		os << count << ' ' << text;

		if (count > 1)
			os << 's';

		printCount++;
		value -= truncated;
	};

	auto duration = humanDuration.m_Duration;
	Print(duration, tf2_bot_detector::year_t{}, "year");
	Print(duration, tf2_bot_detector::month_t{}, "month");
	Print(duration, tf2_bot_detector::week_t{}, "week");
	Print(duration, tf2_bot_detector::day_t{}, "day");
	Print(duration, std::chrono::hours{}, "hour");
	Print(duration, std::chrono::minutes{}, "minute");
	Print(duration, std::chrono::seconds{}, "second");

	return os;
#endif
}
