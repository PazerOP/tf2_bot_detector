#pragma once

#include <system_error>

namespace tf2_bot_detector
{
	enum class ErrorCode
	{
		Success = 0,

		InternetConnectivityDisabled,
	};
}

namespace std
{
	template<> struct is_error_condition_enum<tf2_bot_detector::ErrorCode> : std::bool_constant<true> {};
	std::error_condition make_error_condition(tf2_bot_detector::ErrorCode e);
}
