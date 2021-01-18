#define CPPHTTPLIB_OPENSSL_SUPPORT 1

#include <mh/concurrency/thread_pool.hpp>
#include <mh/error/error_code_exception.hpp>

#include "HTTPClient.h"
#include "HTTPHelpers.h"

#pragma warning(push, 1)
#include <httplib.h>
#pragma warning(pop)

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace tf2_bot_detector;

namespace
{
	class HTTPClientImpl final : public IHTTPClient
	{
	public:
		std::string GetString(const URL& url) const override;
		mh::task<std::string> GetStringAsync(URL url) const override;

		uint32_t GetTotalRequestCount() const override { return m_TotalRequestCount; }

	private:
		mutable std::atomic_uint32_t m_TotalRequestCount = 0;
	};
}

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

std::string HTTPClientImpl::GetString(const URL& url) const try
{
	++m_TotalRequestCount;

	httplib::SSLClient client(url.m_Host, url.m_Port);
	client.set_follow_location(true);
	client.set_read_timeout(10);

	httplib::Headers headers =
	{
		{ "User-Agent", "curl/7.58.0" }
	};

	DebugLog("HTTP GET: {}", url);

	auto response = client.Get(url.m_Path.c_str(), headers);
	if (!response)
		throw http_error(response.error(), mh::format("Failed to HTTP GET {}", url));

	if (response->status >= 400 && response->status < 600)
		throw http_error((HTTPResponseCode)response->status, mh::format("Failed to HTTP GET {}", url));

	return response->body;
}
catch (const http_error&)
{
	DebugLogException("{}", url);
	throw;
}
catch (...)
{
	LogException("{}", url);
	throw;
}

static duration_t GetMinRequestInterval(const URL& url)
{
	if (url.m_Host.ends_with("akamaihd.net") ||
		url.m_Host.ends_with("steamstatic.com"))
	{
		return 50ms;
	}
	else if (url.m_Host == "api.steampowered.com")
	{
		return 100ms;
	}

	return 500ms;
}

mh::task<std::string> HTTPClientImpl::GetStringAsync(URL url) const try
{
	auto self = shared_from_this(); // Make sure we don't vanish

	static mh::thread_pool s_HTTPThreadPool(2);

	using throttle_time_t = mh::thread_pool::clock_t::time_point;
	throttle_time_t throttleTime{};
	{
		const duration_t MIN_INTERVAL = GetMinRequestInterval(url);

		static std::mutex s_ThrottleMutex;
		static std::map<std::string, throttle_time_t> s_ThrottleDomains;
		std::lock_guard lock(s_ThrottleMutex);

		auto now = mh::thread_pool::clock_t::now();

		if (auto found = s_ThrottleDomains.find(url.m_Host);
			found != s_ThrottleDomains.end())
		{
			if (now > found->second)
			{
				found->second = now + MIN_INTERVAL;
			}
			else
			{
				throttleTime = found->second;
				found->second += MIN_INTERVAL;
			}
		}
		else
		{
			s_ThrottleDomains.emplace(url.m_Host, now + MIN_INTERVAL);
		}
	}

	if (throttleTime != throttle_time_t{})
	{
		const auto startTime = mh::thread_pool::clock_t::now();
		const auto intendedSleepTime = throttleTime - startTime;
		co_await s_HTTPThreadPool.co_delay_until(throttleTime);
		const auto actualSleepTime = mh::thread_pool::clock_t::now() - startTime;

		DebugLog(LogMessageColor(1, 0, 1), "Intended to sleep for {}ms, actually slept for {}ms",
			std::chrono::duration_cast<std::chrono::milliseconds>(intendedSleepTime).count(),
			std::chrono::duration_cast<std::chrono::milliseconds>(actualSleepTime).count());
	}

	co_await s_HTTPThreadPool.co_add_task();
	co_return self->GetString(url);
}
catch (const http_error&)
{
	DebugLogException("{}", url);
	throw;
}
catch (...)
{
	LogException("{}", url);
	throw;
}

std::shared_ptr<IHTTPClient> tf2_bot_detector::IHTTPClient::Create()
{
	return std::make_shared<HTTPClientImpl>();
}
