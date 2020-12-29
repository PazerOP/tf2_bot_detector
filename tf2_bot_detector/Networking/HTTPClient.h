#pragma once

#include <mh/coroutine/task.hpp>

#include <memory>
#include <string>

namespace tf2_bot_detector
{
	class URL;

	// Only intended to be stored if you are doing something async
	class HTTPClient : public std::enable_shared_from_this<HTTPClient>
	{
	public:
		std::string GetString(const URL& url) const;
		mh::task<std::string> GetStringAsync(const URL& url) const;
	};
}
