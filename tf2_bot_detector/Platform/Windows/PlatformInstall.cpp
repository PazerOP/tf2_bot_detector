#include "Platform/Platform.h"
#include "Networking/HTTPClient.h"
#include "Networking/HTTPHelpers.h"
#include "Util/TextUtils.h"
#include "Log.h"
#include "UpdateManager.h"
#include "Version.h"

#include <mh/future.hpp>

#include <Windows.h>
#include <appmodel.h>

using namespace tf2_bot_detector;

bool tf2_bot_detector::Platform::IsInstalled()
{
	PACKAGE_ID id{};
	UINT32 bufSize = sizeof(id);
	const auto result = GetCurrentPackageId(&bufSize, reinterpret_cast<BYTE*>(&id));

	if (result == ERROR_SUCCESS)
	{
		return true;
	}
	else if (result == APPMODEL_ERROR_NO_PACKAGE)
	{
		return false;
	}
	else
	{
		LogError(MH_SOURCE_LOCATION_CURRENT(), "Unknown error code returned by GetCurrentPackageId(): {}", result);
		return false;
	}
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

	const auto bundleUri = buildInfo.m_MSIXBundleURL;

	return std::async([bundleUri]() -> InstallUpdate::Result
		{
			{
				DebugLogWarning(MH_SOURCE_LOCATION_CURRENT(),
					"Microsoft has ensured that we can't have anything nice, requesting update tool");

				return InstallUpdate::NeedsUpdateTool
				{
					.m_UpdateToolArgs = mh::format("--update-type MSIX --source-path {}", std::quoted(bundleUri)),
				};
			}
		});
}
