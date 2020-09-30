#pragma once

#include <system_error>

namespace tf2_bot_detector
{
	enum class ErrorCode
	{
		Success = 0,

		InternetConnectivityDisabled,
		LazyValueUninitialized,
		UnknownError, // I SWORE I WOULD NEVER TYPE THESE WORDS
		LogicError,
	};

	std::error_condition make_error_condition(tf2_bot_detector::ErrorCode e);
}

namespace std
{
	template<> struct is_error_condition_enum<tf2_bot_detector::ErrorCode> : std::bool_constant<true> {};
}
