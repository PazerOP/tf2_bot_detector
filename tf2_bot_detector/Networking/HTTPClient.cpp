#define CPPHTTPLIB_OPENSSL_SUPPORT 1

#include <mh/concurrency/thread_pool.hpp>
#include <mh/error/error_code_exception.hpp>

#include "HTTPClient.h"
#include "HTTPHelpers.h"

#pragma warning(push, 1)
#include <httplib.h>
#pragma warning(pop)

using namespace std::string_literals;
using namespace tf2_bot_detector;

namespace std
{
	template<> struct is_error_condition_enum<httplib::Error> : true_type {};
}

namespace httplib
{
	std::error_condition make_error_condition(Error e)
	{
		struct Category final : std::error_category
		{
			const char* name() const noexcept override { return "httplib::Error"; }

			std::string message(int condition) const override
			{
				switch ((Error)condition)
				{
				case Error::Success:
					return "Success";

				case Error::Unknown:
					return "Unknown error";
				case Error::Connection:
					return "Connection error";
				case Error::BindIPAddress:
					return "Failed to bind IP address";
				case Error::Read:
					return "Failed to read from a socket";
				case Error::Write:
					return "Failed to write to a socket";
				case Error::ExceedRedirectCount:
					return "Exceeded the maximum number of redirects";
				case Error::Canceled:
					return "Request cancelled";
				case Error::SSLConnection:
					return "SSL connection error";
				case Error::SSLServerVerification:
					return "SSL server verification error";
				case Error::UnsupportedMultipartBoundaryChars:
					return "Unsupported multi-part boundary characters";

				default:
					return "<UNKNOWN ERROR>";
				}
			}

		} static const s_Category;

		return std::error_condition(static_cast<int>(e), s_Category);
	}
}

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
		throw http_error(response.error(), mh::format("Failed to HTTP GET {}", url));

	if (response->status >= 400 && response->status < 600)
		throw http_error((HTTPResponseCode)response->status, mh::format("Failed to HTTP GET {}", url));

	return response->body;
}
catch (...)
{
	LogException(MH_SOURCE_LOCATION_CURRENT(), "{}", url);
	throw;
}

mh::task<std::string> HTTPClient::GetStringAsync(URL url) const try
{
	static mh::thread_pool s_HTTPThreadPool(4);

	auto self = shared_from_this(); // Make sure we don't vanish
	co_await s_HTTPThreadPool.co_add_task();
	co_return self->GetString(url);
}
catch (const http_error&)
{
	DebugLogException(MH_SOURCE_LOCATION_CURRENT(), "{}", url);
	throw;
}
catch (...)
{
	LogException(MH_SOURCE_LOCATION_CURRENT(), "{}", url);
	throw;
}
