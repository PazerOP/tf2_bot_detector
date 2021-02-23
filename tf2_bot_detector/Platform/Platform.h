#pragma once

#include "ReleaseChannel.h"
#include "SteamID.h"
#include "Version.h"

#include <mh/coroutine/task.hpp>
#include <mh/reflection/enum.hpp>

#include <filesystem>
#include <future>
#include <string>
#include <variant>

namespace tf2_bot_detector
{
	class IHTTPClient;
	struct BuildInfo;

	inline namespace Platform
	{
		std::filesystem::path GetCurrentSteamDir();
		SteamID GetCurrentActiveSteamID();

		std::filesystem::path GetCurrentExeDir();
		std::filesystem::path GetRootLocalAppDataDir();
		std::filesystem::path GetRootRoamingAppDataDir();
		std::filesystem::path GetLegacyAppDataDir();
		std::filesystem::path GetRootTempDataDir();

		bool IsDebuggerAttached();

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
		mh::task<InstallUpdate::Result> BeginInstallUpdate(const BuildInfo& bi, const IHTTPClient& client);
		bool IsInstalled(); // As opposed to portable

		bool NeedsElevationToWrite(const std::filesystem::path& path, bool recursive = false);

		namespace Processes
		{
			bool IsTF2Running();
			std::shared_future<std::vector<std::string>> GetTF2CommandLineArgsAsync();
			bool IsSteamRunning();
			void RequireTF2NotRunning();

			void Launch(const std::filesystem::path& executable, const std::vector<std::string>& args = {},
				bool elevated = false);
			void Launch(const std::filesystem::path& executable, const std::string_view& args = {},
					bool elevated = false);
			int GetCurrentProcessID();

			size_t GetCurrentRAMUsage();
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

		namespace ErrorCodes
		{
			extern const std::error_code PRIVILEGE_NOT_HELD;
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
