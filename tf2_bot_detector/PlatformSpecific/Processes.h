#pragma once

#include <future>
#include <string>
#include <system_error>

namespace tf2_bot_detector::Processes
{
	bool IsTF2Running();
	std::shared_future<std::vector<std::string>> GetTF2CommandLineArgsAsync();
	bool IsSteamRunning();
	void RequireTF2NotRunning();
}
