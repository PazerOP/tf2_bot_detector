#include "HTTPHelpers.h"

#include <mh/text/charconv_helper.hpp>
#include <mh/text/string_insertion.hpp>

#include <iomanip>

using namespace std::string_literals;
using namespace tf2_bot_detector;

URL::URL(const std::string_view& url)
{
	auto schemeEnd = url.find("://");
	if (schemeEnd != url.npos)
	{
		schemeEnd += 3;
		m_Scheme = std::string(url.substr(0, schemeEnd));
	}
	else
	{
		m_Scheme = "https://"; // Assume https
		schemeEnd = 0;
	}

	if (m_Scheme == "https://")
		m_Port = 443;
	else if (m_Scheme == "http://")
		m_Port = 80;

	auto firstSlash = url.find('/', schemeEnd);
	auto firstColon = url.find(':', schemeEnd);

	if (firstSlash != firstColon)
	{
		m_Host = std::string(url.substr(schemeEnd, std::min(firstSlash, firstColon) - schemeEnd));
	}
	else
	{
		assert(firstSlash == url.npos);
		assert(firstColon == url.npos);
		m_Host = std::string(url.substr(schemeEnd));
		return;
	}

	if (firstColon < firstSlash)
	{
		auto portStr = url.substr(firstColon, firstSlash);
		if (!mh::from_chars(portStr, m_Port))
			throw std::invalid_argument("Failed to parse port from "s << std::quoted(url));
	}

	m_Path = std::string(url.substr(firstSlash));
}

http_error::http_error(int statusCode) :
	http_error("HTTP error "s << statusCode)
{
}

http_error::http_error(std::string msg) :
	std::runtime_error(std::move(msg))
{
}
