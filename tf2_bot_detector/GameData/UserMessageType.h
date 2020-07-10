#pragma once

#include <ostream>

namespace tf2_bot_detector
{
	enum class UserMessageType
	{
		TextMsg = 4,

		CallVoteFailed = 45,
		VoteStart = 46,
		VotePass = 47,
		VoteFailed = 48,
		VoteSetup = 49,
	};
}

template<typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, tf2_bot_detector::UserMessageType type)
{
	using tf2_bot_detector::UserMessageType;
#undef OS_CASE
#define OS_CASE(v) case v : return os << #v
	switch (type)
	{
		OS_CASE(UserMessageType::TextMsg);
		OS_CASE(UserMessageType::CallVoteFailed);
		OS_CASE(UserMessageType::VoteStart);
		OS_CASE(UserMessageType::VotePass);
		OS_CASE(UserMessageType::VoteFailed);
		OS_CASE(UserMessageType::VoteSetup);

	default:
		return os << "UserMessageType(" << +std::underlying_type_t<UserMessageType>(type) << ')';
	}
#undef OS_CASE
}
