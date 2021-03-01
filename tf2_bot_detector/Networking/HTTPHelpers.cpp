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

std::string tf2_bot_detector::URL::ToString() const
{
	std::string retVal;
	retVal << *this;
	return retVal;
}

std::string tf2_bot_detector::URL::GetSchemeHostPort() const
{
	std::string retVal;
	retVal << m_Scheme << m_Host << ':' << m_Port;
	return retVal;
}

std::error_condition tf2_bot_detector::make_error_condition(HTTPResponseCode e)
{
	struct Category final : std::error_category
	{
		const char* name() const noexcept override { return "http response codes"; }

		std::string message(int condition) const override
		{
			switch (static_cast<HTTPResponseCode>(condition))
			{
				// 100
			case HTTPResponseCode::Continue:
				return "Continue";
			case HTTPResponseCode::SwitchingProtocol:
				return "Switching Protocol";
			case HTTPResponseCode::Processing:
				return "Processing";
			case HTTPResponseCode::EarlyHints:
				return "Early Hints";

				// 200
			case HTTPResponseCode::OK:
				return "OK";
			case HTTPResponseCode::Created:
				return "Created";
			case HTTPResponseCode::Accepted:
				return "Accepted";

				// 300
			case HTTPResponseCode::MultipleChoice:
				return "Multiple Choice";
			case HTTPResponseCode::MovedPermanently:
				return "Moved Permanently";
			case HTTPResponseCode::MovedTemporarily:
				return "Moved Temporarily";
			case HTTPResponseCode::SeeOther:
				return "See Other";
			case HTTPResponseCode::NotModified:
				return "Not Modified";
			case HTTPResponseCode::TemporaryRedirect:
				return "Temporary Redirect";
			case HTTPResponseCode::PermanentRedirect:
				return "Permanent Redirect";

				// 400
			case HTTPResponseCode::BadRequest:
				return "Bad Request";
			case HTTPResponseCode::Unauthorized:
				return "Unauthorized";
			case HTTPResponseCode::PaymentRequired:
				return "Payment Required";
			case HTTPResponseCode::Forbidden:
				return "Forbidden";
			case HTTPResponseCode::NotFound:
				return "Not Found";
			case HTTPResponseCode::TooManyRequests:
				return "Too Many Requests";

				// 500
			case HTTPResponseCode::InternalServerError:
				return "Internal Server Error";
			case HTTPResponseCode::NotImplemented:
				return "Not Implemented";
			case HTTPResponseCode::BadGateway:
				return "Bad Gateway";
			case HTTPResponseCode::ServiceUnavailable:
				return "Service Unavailable";
			case HTTPResponseCode::GatewayTimeout:
				return "Gateway Timeout";
			}

			return mh::format("<UNKNOWN>(HTTP {})", condition);
		}

	} static const s_Category;

	return std::error_condition(static_cast<int>(e), s_Category);
}
