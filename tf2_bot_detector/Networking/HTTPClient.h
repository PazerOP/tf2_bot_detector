#pragma once

#include <string>

namespace tf2_bot_detector
{
	class URL;

	class HTTPClient
	{
	public:
		std::string GetString(const URL& url) const;
	};
}
