#pragma once

#include "ReleaseChannel.h"
#include "SteamID.h"
#include "Version.h"

#include <mh/reflection/enum.hpp>

#include <filesystem>
#include <future>
#include <optional>
#include <string>
#include <variant>

namespace tf2_bot_detector
{
	class HTTPClient;
	struct BuildInfo;

	inline namespace Platform
	{
		std::filesystem::path GetCurrentSteamDir();
		SteamID GetCurrentActiveSteamID();

		std::filesystem::path GetCurrentExeDir();
		std::filesystem::path GetAppDataDir();
		std::filesystem::path GetRealAppDataDir();

		enum class OS
		{
			Windows,
			Linux,
		};
		OS GetOS();

		enum class Arch
		{
			x86,
			x64,
		};
		Arch GetArch();

		namespace InstallUpdate
		{
			struct Success {};

			// We think we've started the update, but there's no way to determine if it succeeded or not.
			struct StartedNoFeedback {};

			struct NeedsUpdateTool
			{
				std::string m_UpdateToolArgs;
			};

			using Result = std::variant<
				Success,
				StartedNoFeedback,
				NeedsUpdateTool>;
		};

		bool CanInstallUpdate(const BuildInfo& bi);
		std::future<InstallUpdate::Result> BeginInstallUpdate(const BuildInfo& bi, const HTTPClient& client);
		bool IsInstalled(); // As opposed to portable

		namespace Processes
		{
			bool IsTF2Running();
			std::shared_future<std::vector<std::string>> GetTF2CommandLineArgsAsync();
			bool IsSteamRunning();
			void RequireTF2NotRunning();

			void Launch(const std::filesystem::path& executable, const std::vector<std::string>& args = {});
			int GetCurrentProcessID();
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

MH_ENUM_REFLECT_BEGIN(tf2_bot_detector::Platform::OS)
	MH_ENUM_REFLECT_VALUE(Windows)
	MH_ENUM_REFLECT_VALUE(Linux)
MH_ENUM_REFLECT_END()

MH_ENUM_REFLECT_BEGIN(tf2_bot_detector::Platform::Arch)
	MH_ENUM_REFLECT_VALUE(x86)
	MH_ENUM_REFLECT_VALUE(x64)
MH_ENUM_REFLECT_END()
