#pragma once

#include <type_traits>

namespace tf2_bot_detector
{
	enum class TFMatchGroup
	{
		Invalid = -1,

		MVMBootCamp = 0,
		MVMMannUp = 1,

		Competitive6s = 2,
		Competitive9s = 3,
		Competitive12s = 4,

		Casual6s = 5,
		Casual9s = 6,
		Casual12s = 7,

		COUNT = 8,
	};

	enum class TFMatchGroupFlags : uint32_t;
	inline constexpr TFMatchGroupFlags AsFlag(TFMatchGroup group)
	{
		return TFMatchGroupFlags(1 << uint32_t(group));
	}

#undef TF_MATCH_GROUP_FLAG
#define TF_MATCH_GROUP_FLAG(name) name = uint32_t(AsFlag(TFMatchGroup::name))
	enum class TFMatchGroupFlags : uint32_t
	{
		None = 0,

		TF_MATCH_GROUP_FLAG(MVMBootCamp),
		TF_MATCH_GROUP_FLAG(MVMMannUp),
		MVM = MVMBootCamp | MVMMannUp,

		TF_MATCH_GROUP_FLAG(Competitive6s),
		TF_MATCH_GROUP_FLAG(Competitive9s),
		TF_MATCH_GROUP_FLAG(Competitive12s),
		Competitive = Competitive6s | Competitive9s | Competitive12s,

		TF_MATCH_GROUP_FLAG(Casual6s),
		TF_MATCH_GROUP_FLAG(Casual9s),
		TF_MATCH_GROUP_FLAG(Casual12s),
		Casual = Casual6s | Casual9s | Casual12s,
	};
#undef TF_MATCH_GROUP_FLAG

	enum class TFQueueStateChange
	{
		None = 0,

		Entered,
		RequestedEnter,
		Exited,
		RequestedExit,
	};

	inline constexpr TFMatchGroupFlags operator|(TFMatchGroupFlags lhs, TFMatchGroupFlags rhs)
	{
		using ut = std::underlying_type_t<TFMatchGroupFlags>;
		return TFMatchGroupFlags(ut(lhs) | ut(rhs));
	}
	inline constexpr TFMatchGroupFlags operator&(TFMatchGroupFlags lhs, TFMatchGroupFlags rhs)
	{
		using ut = std::underlying_type_t<TFMatchGroupFlags>;
		return TFMatchGroupFlags(ut(lhs) & ut(rhs));
	}
	inline constexpr TFMatchGroupFlags operator&(TFMatchGroupFlags lhs, TFMatchGroup rhs)
	{
		return lhs & AsFlag(rhs);
	}
	inline constexpr TFMatchGroupFlags operator~(TFMatchGroupFlags x)
	{
		using ut = std::underlying_type_t<TFMatchGroupFlags>;
		return TFMatchGroupFlags(~ut(x));
	}
	inline constexpr bool operator!(TFMatchGroupFlags x)
	{
		return x == TFMatchGroupFlags::None;
	}

	inline constexpr TFMatchGroupFlags& operator|=(TFMatchGroupFlags& lhs, TFMatchGroupFlags rhs)
	{
		return lhs = lhs | rhs;
	}
	inline constexpr TFMatchGroupFlags& operator&=(TFMatchGroupFlags& lhs, TFMatchGroupFlags rhs)
	{
		return lhs = lhs & rhs;
	}
}
