#pragma once

#include <cassert>
#include <chrono>
#include <ctime>
#include <ostream>

namespace tf2_bot_detector
{
	using clock_t = std::chrono::system_clock;
	using tfbd_clock_t = clock_t;
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

	template<typename CharT, typename Traits, typename TRep, typename TPeriod>
	inline std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os,
		const tf2_bot_detector::HumanDuration<TRep, TPeriod>& humanDuration)
	{
		int printCount = 0;

		using TInputDuration = std::chrono::duration<TRep, TPeriod>;
		const auto Print = [&](TInputDuration& value, auto periodType, const char* text, auto prevPeriodType)
		{
			if (printCount >= 2)
				return;

			using TPeriod = std::decay_t<decltype(periodType)>;
			using TPrevPeriod = std::decay_t<decltype(prevPeriodType)>;
			static constexpr bool HAS_PREV_PERIOD = !std::is_same_v<TPrevPeriod, std::nullptr_t>;
			constexpr TPeriod prevPeriod = []()
			{
				if constexpr (HAS_PREV_PERIOD)
					return std::chrono::duration_cast<TPeriod>(TPrevPeriod(1));
				else
					return TPeriod{};
			}();

			const auto truncated = std::chrono::duration_cast<TPeriod>(value);
			const auto count = truncated.count();
			if (count <= 0)
			{
				if (printCount > 0)
					printCount++;

				return;
			}

			if (printCount > 0)
				os << ", ";

			os << count << ' ' << text;

			if (count > 1)
				os << 's';

			if constexpr (HAS_PREV_PERIOD)
			{
				assert(truncated <= prevPeriod);
				assert(count <= prevPeriod.count());
			}

			printCount++;
			value -= std::chrono::duration_cast<TInputDuration>(truncated);
		};

		auto duration = humanDuration.m_Duration;

		Print(duration, year_t{}, "year", nullptr);
		Print(duration, month_t{}, "month", year_t{});
		Print(duration, week_t{}, "week", month_t{});
		Print(duration, day_t{}, "day", week_t{});
		Print(duration, hour_t{}, "hour", day_t{});
		Print(duration, minute_t{}, "minute", hour_t{});
		Print(duration, second_t{}, "second", minute_t{});

		return os;
	}
}
