#pragma once

#include <mh/text/charconv_helper.hpp>
#include <mh/text/format.hpp>

#include <iomanip>
#include <regex>
#include <stdexcept>
#include <string_view>

namespace tf2_bot_detector
{
	using svmatch = std::match_results<std::string_view::const_iterator>;

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

	template<typename TIter, typename T, typename... TArgs>
	inline void from_chars_throw(const std::sub_match<TIter>& match, T& out, TArgs&&... args)
	{
		const auto sv = to_string_view(match);
		auto result = mh::from_chars(sv, out, std::forward<TArgs>(args)...);
		if (!result)
		{
			throw std::runtime_error(mh::format("Failed to parse {} as {}", std::quoted(sv), typeid(T).name()));
		}
	}
}
