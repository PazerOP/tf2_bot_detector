#pragma once

#include <mh/error/error_code_exception.hpp>
#include <nlohmann/json.hpp>

#include <compare>
#include <optional>
#include <ostream>
#include <string>
#include <system_error>

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

		std::string ToString() const;
		std::string GetSchemeHostPort() const;

		std::string m_Scheme;
		std::string m_Host;
		unsigned int m_Port = 80;
		std::string m_Path;
	};

	enum class HTTPResponseCode
	{
		Continue = 100,
		SwitchingProtocol = 101,
		Processing = 102,
		EarlyHints = 103,

		OK = 200,
		Created = 201,
		Accepted = 202,

		MultipleChoice = 300,
		MovedPermanently = 301,
		MovedTemporarily = 302,
		SeeOther = 303,
		NotModified = 304,
		TemporaryRedirect = 307,
		PermanentRedirect = 308,

		BadRequest = 400,
		Unauthorized = 401,
		PaymentRequired = 402,
		Forbidden = 403,
		NotFound = 404,
		TooManyRequests = 429,

		InternalServerError = 500,
		NotImplemented = 501,
		BadGateway = 502,
		ServiceUnavailable = 503,
		GatewayTimeout = 504,
	};

	std::error_condition make_error_condition(HTTPResponseCode e);

	class http_error : public mh::error_condition_exception
	{
		using super = mh::error_condition_exception;
	public:
		using super::super;
	};

	template<typename CharT, typename Traits>
	std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const tf2_bot_detector::URL& url)
	{
		return os << url.m_Scheme << url.m_Host << ':' << url.m_Port << url.m_Path;
	}
}

namespace std
{
	template<> struct is_error_condition_enum<tf2_bot_detector::HTTPResponseCode> : true_type {};
}
