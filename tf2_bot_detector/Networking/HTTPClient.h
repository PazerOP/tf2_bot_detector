#pragma once

#include <mh/coroutine/task.hpp>

#include <memory>
#include <string>

namespace tf2_bot_detector
{
	class URL;

	// Only intended to be stored if you are doing something async
	class IHTTPClient : public std::enable_shared_from_this<IHTTPClient>
	{
	public:
		virtual ~IHTTPClient() = default;

		static std::shared_ptr<IHTTPClient> Create();

		virtual std::string GetString(const URL& url) const = 0;
		virtual mh::task<std::string> GetStringAsync(URL url) const = 0;

		struct RequestCounts
		{
			uint32_t m_Total;
			uint32_t m_Failed;
			uint32_t m_InProgress;  // Waiting on the server
			uint32_t m_Throttled;   // Locally throttled
		};

		virtual RequestCounts GetRequestCounts() const = 0;
	};

	using HTTPClient = IHTTPClient; // temp, but probably valve time temp if i'm being totally honest
}
