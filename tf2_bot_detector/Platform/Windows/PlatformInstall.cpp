#include "Platform/Platform.h"
#include "Networking/HTTPClient.h"
#include "Networking/HTTPHelpers.h"
#include "Util/TextUtils.h"
#include "Log.h"
#include "UpdateManager.h"
#include "Version.h"

#include <mh/future.hpp>

#include <Windows.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Management.Deployment.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#pragma comment(lib, "windowsapp")

using namespace tf2_bot_detector;

static winrt::Windows::ApplicationModel::Package GetAppPackage()
{
	static auto s_Package = std::invoke([]() -> winrt::Windows::ApplicationModel::Package
		{
			try
			{
				using namespace winrt::Windows::ApplicationModel;
				auto package = Package::Current();
				if (!package)
				{
					DebugLog(MH_SOURCE_LOCATION_CURRENT(), "We are not an installed package");
					return nullptr;
				}

				return package;
			}
			catch (const winrt::hresult_error& e)
			{
				//DebugLog(MH_SOURCE_LOCATION_CURRENT(), "Unable to confirm we are an installed package: {}", ToMB(e.message()));
				return nullptr;
			}
		});

	return s_Package;
}

bool tf2_bot_detector::Platform::IsInstalled()
{
	return !!GetAppPackage();
}

bool tf2_bot_detector::Platform::CanInstallUpdate(const BuildInfo& bi)
{
	if (!IsInstalled())
		return false;

	if (bi.m_MSIXBundleURL.empty())
		return false;

	return true;
}

std::future<InstallUpdate::Result> tf2_bot_detector::Platform::BeginInstallUpdate(
	const BuildInfo& buildInfo, const HTTPClient& client)
{
	if (!IsInstalled())
	{
		constexpr const char ERROR_MSG[] = "Attempted to call " __FUNCTION__ "() when we aren't installed."
			" This should never happen.";
		LogError(MH_SOURCE_LOCATION_CURRENT(), ERROR_MSG);
		throw std::logic_error(ERROR_MSG);
	}

	if (buildInfo.m_MSIXBundleURL.empty())
	{
		constexpr const char ERROR_MSG[] = "BuildInfo's msix bundle url is empty."
			" This should never happen; CanInstallUpdate() should have returned false";
		LogError(MH_SOURCE_LOCATION_CURRENT(), ERROR_MSG);
		throw std::invalid_argument(ERROR_MSG);
	}

	using winrt::Windows::ApplicationModel::Package;
	using winrt::Windows::Foundation::Uri;
	using winrt::Windows::System::Launcher;

	const auto bundleUri = buildInfo.m_MSIXBundleURL;

	return std::async([bundleUri]() -> InstallUpdate::Result
		{
#if 0
			const Uri uri = mh::format(L"ms-appinstaller:?source={}", ToWC(bundleUri));

			DebugLog(MH_SOURCE_LOCATION_CURRENT(), "Attempting to LaunchUriAsync for {}", ToMB(uri.ToString()));
			auto result = Launcher::LaunchUriAsync(uri);

			if (result.get())
			{
				return InstallUpdate::StartedNoFeedback{};
			}
			else
#endif
			{
				DebugLogWarning(MH_SOURCE_LOCATION_CURRENT(), "LaunchUriAsync returned false, requesting update tool");
				return InstallUpdate::NeedsUpdateTool
				{
					.m_UpdateToolArgs = mh::format("--msix-bundle-url {}", std::quoted(bundleUri)),
				};
			}
		});
}
