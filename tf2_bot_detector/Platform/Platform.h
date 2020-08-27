#pragma once

#include "SteamID.h"
#include "Version.h"

#include <filesystem>
#include <future>
#include <optional>
#include <string>

namespace tf2_bot_detector
{
	class HTTPClient;
	enum class ReleaseChannel;

	inline namespace Platform
	{
		std::filesystem::path GetCurrentSteamDir();
		SteamID GetCurrentActiveSteamID();

		std::filesystem::path GetCurrentExeDir();
		std::filesystem::path GetAppDataDir();
		std::filesystem::path GetRealAppDataDir();

		/// <summary>
		/// Is there an update available via a platform-specific update mechanism?
		/// msix(bundle) on Windows, APT on linux
		/// </summary>
		std::future<std::optional<Version>> CheckForPlatformUpdate(ReleaseChannel rc, const HTTPClient& client);
		void BeginPlatformUpdate(ReleaseChannel rc, const HTTPClient& client);
		bool IsInstalled(); // As opposed to portable

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
