#pragma once

#include <filesystem>

namespace mh
{
	class exception_details_handler;
}

namespace tf2_bot_detector
{
	class WinRT
	{
	public:
		virtual ~WinRT() = default;

		virtual std::filesystem::path GetPackageLocalAppDataDir() const = 0;
		virtual std::filesystem::path GetPackageRoamingAppDataDir() const = 0;
		virtual std::filesystem::path GetPackageTempDir() const = 0;

		virtual bool IsInPackage() const = 0;
		virtual std::wstring GetCurrentPackageFamilyName() const = 0;

		virtual const mh::exception_details_handler& GetWinRTExceptionDetailsHandler() const = 0;
	};

	using CreateWinRTInterfaceFn = WinRT*(*)();
}
