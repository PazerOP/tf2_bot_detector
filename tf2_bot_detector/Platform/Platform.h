#pragma once

#include "ReleaseChannel.h"
#include "SteamID.h"

#include <filesystem>
#include <future>
#include <optional>
#include <string>
#include <system_error>

namespace tf2_bot_detector
{
	inline namespace Platform
	{
		std::filesystem::path GetCurrentSteamDir();
		SteamID GetCurrentActiveSteamID();

		std::filesystem::path GetCurrentExeDir();
		std::filesystem::path GetAppDataDir();
		std::filesystem::path GetRealAppDataDir();
		std::optional<ReleaseChannel> GetPlatformUpdateChannel();

		namespace Processes
		{
			bool IsTF2Running();
			std::shared_future<std::vector<std::string>> GetTF2CommandLineArgsAsync();
			bool IsSteamRunning();
			void RequireTF2NotRunning();
		}

		namespace Shell
		{
			std::vector<std::string> SplitCommandLineArgs(const std::string_view& cmdline);
			std::filesystem::path BrowseForFolderDialog();
			void ExploreToAndSelect(std::filesystem::path path);

			void ExploreTo(const std::filesystem::path& path);
			void OpenURL(const char* url);
			inline void OpenURL(const std::string& url) { return OpenURL(url.c_str()); }
		}
	}
}
