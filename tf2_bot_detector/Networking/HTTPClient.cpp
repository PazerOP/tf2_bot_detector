#define CPPHTTPLIB_OPENSSL_SUPPORT 1

#include "HTTPClient.h"
#include "HTTPHelpers.h"

#pragma warning(push, 1)
#include <httplib.h>
#pragma warning(pop)

using namespace std::string_literals;
using namespace tf2_bot_detector;

std::string HTTPClient::GetString(const URL& url) const try
{
	httplib::SSLClient client(url.m_Host, url.m_Port);
	client.set_follow_location(true);
	client.set_read_timeout(10);

	httplib::Headers headers =
	{
		{ "User-Agent", "curl/7.58.0" }
	};

	auto response = client.Get(url.m_Path.c_str(), headers);
	if (!response)
		throw http_error(mh::format("Failed to HTTP GET {}", url));

	if (response->status >= 400 && response->status < 600)
		throw http_error(response->status);

	return response->body;
}
catch (const std::exception& e)
{
	LogException(MH_SOURCE_LOCATION_CURRENT(), e, "{}", url);
	throw;
}
