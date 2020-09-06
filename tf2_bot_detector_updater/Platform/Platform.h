#pragma once

#include <filesystem>

namespace tf2_bot_detector
{
	inline namespace Platform
	{
		void WaitForPIDToExit(int pid);

		namespace Processes
		{
			void Launch(const std::filesystem::path& executable);
		}
	}
}
