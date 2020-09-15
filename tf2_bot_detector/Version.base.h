#pragma once

#include <nlohmann/json_fwd.hpp>

#include <compare>
#include <cstdint>
#include <optional>
#include <ostream>

namespace tf2_bot_detector
{
	struct Version
	{
		using value_type = uint16_t;

		constexpr Version() = default;

		explicit constexpr Version(value_type major, value_type minor, value_type patch = 0, value_type build = 0) :
			m_Major(major),
			m_Minor(minor),
			m_Patch(patch),
			m_Build(build)
		{
		}

		static std::optional<Version> Parse(const char* str);

		constexpr auto operator<=>(const Version&) const = default;

		constexpr bool IsCustomBuild() const { return m_Build == 65535; }

		value_type m_Major{};
		value_type m_Minor{};
		value_type m_Patch{};
		value_type m_Build{};
	};

	static constexpr Version VERSION(
		${CMAKE_PROJECT_VERSION_MAJOR},
		${CMAKE_PROJECT_VERSION_MINOR},
		${CMAKE_PROJECT_VERSION_PATCH},
		${CMAKE_PROJECT_VERSION_TWEAK}
	);

	void to_json(nlohmann::json& j, const Version& d);
	void from_json(const nlohmann::json& j, Version& d);
}

template<typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const tf2_bot_detector::Version& v)
{
	os << v.m_Major << '.' << v.m_Minor << '.' << v.m_Patch;

	if (v.IsCustomBuild())
		os << " (Custom Build)";
	else
		os << '.' << v.m_Build;

	return os;
}
