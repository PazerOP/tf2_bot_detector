#pragma once

#include <nlohmann/json.hpp>

#include <compare>
#include <ostream>
#include <string>

namespace tf2_bot_detector
{
	class URL final
	{
	public:
		URL() = default;
		URL(std::nullptr_t) {}
		URL(const char* str) : URL(std::string_view(str)) {}
		URL(const std::string_view& url);
		URL(const std::string& str) : URL(std::string_view(str)) {}

		auto operator<=>(const URL&) const = default;

		std::string m_Scheme;
		std::string m_Host;
		unsigned int m_Port = 80;
		std::string m_Path;
	};

	class http_error : public std::runtime_error
	{
	public:
		http_error(int statusCode);
		http_error(std::string msg);
	};
}

template<typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const tf2_bot_detector::URL& url)
{
	return os << url.m_Scheme << url.m_Host << url.m_Port << url.m_Path;
}
