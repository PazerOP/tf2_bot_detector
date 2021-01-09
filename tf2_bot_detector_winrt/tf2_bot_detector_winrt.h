#pragma once

#include <filesystem>

namespace tf2_bot_detector
{
	class WinRT
	{
	public:
		virtual ~WinRT() = default;

		virtual std::filesystem::path GetLocalAppDataDir() const = 0;
		virtual std::filesystem::path GetRoamingAppDataDir() const = 0;
		virtual std::filesystem::path GetTempDir() const = 0;
	};

	using CreateWinRTInterfaceFn = WinRT*(*)();
}
