#define CPPHTTPLIB_OPENSSL_SUPPORT 1

#include "HTTPHelpers.h"

#include <mh/text/charconv_helper.hpp>
#include <mh/text/string_insertion.hpp>

#pragma warning(push, 1)
#include <httplib.h>
#pragma warning(pop)

#include <iomanip>

using namespace std::string_literals;
using namespace tf2_bot_detector;

std::string HTTP::GetString(const URL& url)
{
	httplib::SSLClient client(url.m_Host, url.m_Port);
	client.set_follow_location(true);

	httplib::Headers headers =
	{
		{ "User-Agent", "curl/7.58.0" }
	};

	auto response = client.Get(url.m_Path.c_str(), headers);
	if (!response)
		throw std::runtime_error("Failed to HTTP GET "s << url);

	return response->body;
}

nlohmann::json HTTP::GetJSON(const URL& url)
{
	return nlohmann::json::parse(GetString(url));
}

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
