#pragma once

#include "Clock.h"

#include <chrono>

namespace tf2_bot_detector
{
	template<typename T, typename TUpdateFunc>
	class CachedVariable final
	{
	public:
		CachedVariable(duration_t updateInterval, TUpdateFunc updateFunc) :
			m_LastUpdated(clock_t::now()),
			m_UpdateInterval(updateInterval),
			m_UpdateFunc(std::move(updateFunc)),
			m_Value(std::invoke(m_UpdateFunc))
		{
		}

		const T& GetAndUpdate()
		{
			const auto now = clock_t::now();
			if ((now - m_LastUpdated) >= m_UpdateInterval)
				m_Value = std::invoke(m_UpdateFunc);

			return m_Value;
		}

	private:
		time_point_t m_LastUpdated{};
		duration_t m_UpdateInterval{};
		[[no_unique_address]] TUpdateFunc m_UpdateFunc{};
		T m_Value{};
	};

	template<typename TUpdateFunc>
	CachedVariable(duration_t d, TUpdateFunc f) -> CachedVariable<std::invoke_result_t<TUpdateFunc>, TUpdateFunc>;
}
