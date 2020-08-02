#pragma once

#include <type_traits>

namespace tf2_bot_detector
{
	enum class TFQueueType
	{
		None = 0,

		Casual      = (1 << 0),  // 12v12
		Competitive = (1 << 1),  // 6v6
		MVM         = (1 << 2),  // 6v26 (27?)
	};

	enum class TFQueueStateChange
	{
		None = 0,

		Entered,
		RequestedEnter,
		Exited,
		RequestedExit,
	};

	inline constexpr TFQueueType operator|(TFQueueType lhs, TFQueueType rhs)
	{
		using ut = std::underlying_type_t<TFQueueType>;
		return TFQueueType(ut(lhs) | ut(rhs));
	}
	inline constexpr TFQueueType operator&(TFQueueType lhs, TFQueueType rhs)
	{
		using ut = std::underlying_type_t<TFQueueType>;
		return TFQueueType(ut(lhs) & ut(rhs));
	}
	inline constexpr TFQueueType operator~(TFQueueType x)
	{
		using ut = std::underlying_type_t<TFQueueType>;
		return TFQueueType(~ut(x) & (0b111));
	}

	inline constexpr TFQueueType& operator|=(TFQueueType& lhs, TFQueueType rhs)
	{
		return lhs = lhs | rhs;
	}
	inline constexpr TFQueueType& operator&=(TFQueueType& lhs, TFQueueType rhs)
	{
		return lhs = lhs & rhs;
	}
}
