#include "tf2_bot_detector_winrt.h"
#include "tf2_bot_detector_winrt_export.h"

#include <winrt/Windows.Storage.h>

namespace
{
	class WinRTImpl final : public tf2_bot_detector::WinRT
	{
	public:
		std::filesystem::path GetLocalAppDataDir() const override;
		std::filesystem::path GetRoamingAppDataDir() const override;
		std::filesystem::path GetTempDir() const override;
	};

	std::filesystem::path WinRTImpl::GetLocalAppDataDir() const
	{
		auto appData = winrt::Windows::Storage::ApplicationData::Current();
		auto path = appData.LocalFolder().Path();
		return std::filesystem::path(path.begin(), path.end());
	}

	std::filesystem::path WinRTImpl::GetRoamingAppDataDir() const
	{
		auto appData = winrt::Windows::Storage::ApplicationData::Current();
		auto path = appData.RoamingFolder().Path();
		return std::filesystem::path(path.begin(), path.end());
	}

	std::filesystem::path WinRTImpl::GetTempDir() const
	{
		auto appData = winrt::Windows::Storage::ApplicationData::Current();
		auto path = appData.TemporaryFolder().Path();
		return std::filesystem::path(path.begin(), path.end());
	}
}

extern "C" TF2_BOT_DETECTOR_WINRT_EXPORT tf2_bot_detector::WinRT* CreateWinRTInterface()
{
	return new WinRTImpl();
}
