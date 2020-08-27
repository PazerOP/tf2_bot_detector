#include "Platform/Platform.h"
#include "Networking/HTTPClient.h"
#include "Networking/HTTPHelpers.h"
#include "Util/TextUtils.h"
#include "Log.h"
#include "Version.h"

#include <mh/future.hpp>

#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Management.Deployment.h>
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

std::future<std::optional<Version>> tf2_bot_detector::Platform::CheckForPlatformUpdate(
	ReleaseChannel rc, const HTTPClient& client)
{
	if (!IsInstalled())
	{
		DebugLog(MH_SOURCE_LOCATION_CURRENT(), "Not installed");
		return mh::make_ready_future(std::optional<Version>{});
	}

	auto clientPtr = client.shared_from_this();

	return std::async([rc, clientPtr]() -> std::optional<Version>
		{
			auto result = clientPtr->GetString(mh::format("https://tf2bd-util.pazer.us/AppInstaller/LatestVersion.txt?type={:v}", rc));
			auto version = Version::Parse(result.c_str());
			if (version > VERSION)
				return version;

			return std::nullopt;
		});
}

void tf2_bot_detector::Platform::BeginPlatformUpdate(ReleaseChannel rc, const HTTPClient& client)
{
	using namespace winrt::Windows::ApplicationModel;
	using namespace winrt::Windows::Foundation;
	using namespace winrt::Windows::Foundation::Collections;
	using namespace winrt::Windows::Management::Deployment;

	PackageManager mgr;
	auto appInstallerPackages = mgr.FindPackages(L"Microsoft.DesktopAppInstaller_8wekyb3d8bbwe");
	bool appInstallerFound = false;
	for (const Package& pkg : appInstallerPackages)
	{
		appInstallerFound = true;
		break;
	}

	if (appInstallerFound)
	{
		Shell::OpenURL(mh::format(
			"ms-appinstaller:?source=https://tf2bd-util.pazer.us/AppInstaller/{:v}.msixbundle", rc));
	}
	else
	{
		DebugLogWarning(MH_SOURCE_LOCATION_CURRENT(), "App installer not found, attempting to install via API...");
		const Uri uri = mh::format(L"https://tf2bd-util.pazer.us/AppInstaller/{:v}.msixbundle", rc);

		IVector<Uri> deps{ winrt::single_threaded_vector<Uri>() };
		deps.Append(Uri(L"https://tf2bd-util.pazer.us/AppInstaller/vcredist.x64.msix"));

		auto task = mgr.UpdatePackageAsync(uri, deps, DeploymentOptions::ForceApplicationShutdown);
		auto result = task.get();
		throw "Not implemented";
	}
}
