#pragma once

#include <mh/text/charconv_helper.hpp>
#include <mh/text/string_insertion.hpp>
#include <iomanip>
#include <regex>
#include <stdexcept>

namespace tf2_bot_detector
{
	template<typename TIter>
	inline std::string_view to_string_view(const std::sub_match<TIter>& match)
	{
		return std::string_view(&*match.first, match.length());
	}

	template<typename TIter, typename T>
	inline auto from_chars(const std::sub_match<TIter>& match, T& out)
	{
		return mh::from_chars(to_string_view(match), out);
	}

	template<typename TIter, typename T>
	inline void from_chars_throw(const std::sub_match<TIter>& match, T& out)
	{
		const auto sv = to_string_view(match);
		auto result = mh::from_chars(sv, out);
		if (!result)
		{
			using namespace std::string_literals;
			throw std::runtime_error("Failed to parse "s << std::quoted(sv) << " as " << typeid(T).name());
		}
	}
}
