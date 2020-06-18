#pragma once

#include <compare>
#include <ostream>

namespace tf2_bot_detector
{
	struct Version
	{
		auto operator<=>(const Version&) const = default;

		int m_Major;
		int m_Minor;
		int m_Patch;
		int m_Preview;
	};

	static constexpr char VERSION_STRING[] = "${CMAKE_PROJECT_VERSION}";
	static constexpr Version VERSION =
	{
		.m_Major = ${CMAKE_PROJECT_VERSION_MAJOR},
		.m_Minor = ${CMAKE_PROJECT_VERSION_MINOR},
		.m_Patch = ${CMAKE_PROJECT_VERSION_PATCH},
		.m_Preview = ${CMAKE_PROJECT_VERSION_TWEAK},
	};
}

template<typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const tf2_bot_detector::Version& v)
{
	return os << v.m_Major << '.' << v.m_Minor << '.' << v.m_Patch << '.' << v.m_Preview;
}
