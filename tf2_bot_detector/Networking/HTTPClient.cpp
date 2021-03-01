#define CPPHTTPLIB_OPENSSL_SUPPORT 1
#define CPPHTTPLIB_ZLIB_SUPPORT 1

#include <mh/concurrency/thread_pool.hpp>
#include <mh/error/error_code_exception.hpp>

#include "GlobalDispatcher.h"
#include "HTTPClient.h"
#include "HTTPHelpers.h"

#pragma warning(push, 1)
#include <cpprest/http_client.h>
#include <pplawait.h>
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
		mh::task<std::string> GetStringAsyncImpl(URL url) const;

		mutable std::mutex m_InnerClientMutex;
		mutable std::map<std::string, std::shared_ptr<web::http::client::http_client>> m_InnerClients;
		std::shared_ptr<web::http::client::http_client> GetInnerClient(const URL& url) const;

		mutable std::atomic_uint32_t m_TotalRequestCount = 0;
	};
}

std::string HTTPClientImpl::GetString(const URL& url) const
{
	auto task = GetStringAsync(url);
	task.wait();
	return std::move(task.get());
}

std::shared_ptr<web::http::client::http_client> HTTPClientImpl::GetInnerClient(const URL& url) const
{
	std::lock_guard lock(m_InnerClientMutex);

	const std::string schemeHostPort = url.GetSchemeHostPort();
	if (auto found = m_InnerClients.find(schemeHostPort); found != m_InnerClients.end())
	{
		return found->second;
	}
	else
	{
		auto newClient = std::make_shared<web::http::client::http_client>(utility::conversions::to_string_t(schemeHostPort));
		return m_InnerClients.emplace(schemeHostPort, newClient).first->second;
	}
}

mh::task<std::string> HTTPClientImpl::GetStringAsyncImpl(URL url) const
{
	auto self = shared_from_this();

	++m_TotalRequestCount;

	auto client = GetInnerClient(url);

	const auto startTime = tfbd_clock_t::now();

	auto response = co_await client->request(web::http::methods::GET, utility::conversions::to_string_t(url.m_Path));

	if (response.status_code() >= 400 && response.status_code() < 600)
		throw http_error((HTTPResponseCode)response.status_code(), mh::format("Failed to HTTP GET {}", url));

	std::string stringResponse = co_await response.extract_utf8string(true);

	const auto duration = tfbd_clock_t::now() - startTime;
	DebugLog("[{}ms] HTTP GET: {}", std::chrono::duration_cast<std::chrono::milliseconds>(duration).count(), url);

	co_return stringResponse;
}

static duration_t GetMinRequestInterval(const URL& url)
{
	if (url.m_Host.ends_with("akamaihd.net") ||
		url.m_Host.ends_with("steamstatic.com"))
	{
		return 0ms;
	}
	else if (url.m_Host == "api.steampowered.com" || url.m_Host == "tf2bd-util.pazer.us")
	{
		return 100ms;
	}
	else if (url.m_Host == "steamcommunity.com")
	{
		return 2000ms;
	}

	return 500ms;
}

mh::task<std::string> HTTPClientImpl::GetStringAsync(URL url) const try
{
	auto self = shared_from_this(); // Make sure we don't vanish

	int32_t retryCount = 0;
	while (true)
	{
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
			co_await GetDispatcher().co_delay_until(throttleTime);

		try
		{
			co_return co_await GetStringAsyncImpl(url);
		}
		catch (const http_error& e)
		{
			if (e.code() == HTTPResponseCode::TooManyRequests && retryCount < 10)
			{
				// retry a fair number of times for http 429, some stuff is aggressively throttled
			}
			else if (e.code() == HTTPResponseCode::InternalServerError && retryCount < 5)
			{
				// retry a few times for http 500, might be tf2bd-util being broken
			}
			else
			{
				throw; // give up
			}
		}
		catch (const web::http::http_exception&)
		{
			if (retryCount > 3)
			{
				// Give up after a few socket/timeout errors
				throw;
			}
		}

		// Wait and try again
		co_await GetDispatcher().co_delay_for(10s);
		retryCount++;
		DebugLogWarning("Retry #{} for {}", retryCount, url);
	}
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
