#include "Platform/Platform.h"
#include "Networking/HTTPClient.h"
#include "Networking/HTTPHelpers.h"
#include "Util/TextUtils.h"
#include "Log.h"
#include "UpdateManager.h"
#include "Version.h"

#include <mh/future.hpp>

#include <Windows.h>

using namespace tf2_bot_detector;

bool tf2_bot_detector::Platform::IsInstalled()
{
	static const bool s_IsInstalled = []() -> bool
	{
		using func_type = LONG(WINAPI*)(UINT32* packageFullNameLength, PWSTR packageFullName);
		constexpr const char FUNC_NAME[] = "GetCurrentPackageFullName";
		constexpr const char MODULE_NAME[] = "Kernel32.dll";

		HMODULE kernel32 = GetModuleHandleA(MODULE_NAME);
		if (!kernel32)
		{
			LogError(MH_SOURCE_LOCATION_CURRENT(), "Failed to get module handle for {}", MODULE_NAME);
			return false;
		}

		const void* rawFnPtr = GetProcAddress(kernel32, FUNC_NAME);

		if (!rawFnPtr)
		{
			DebugLog(MH_SOURCE_LOCATION_CURRENT(), "Unable to find {} in {}", FUNC_NAME, MODULE_NAME);
			return false;
		}

		const auto fnPtr = reinterpret_cast<func_type>(rawFnPtr);

		UINT32 length = 0;
		const LONG result = fnPtr(&length, nullptr);

		if (result == ERROR_INSUFFICIENT_BUFFER)
			return true;
		else if (result == APPMODEL_ERROR_NO_PACKAGE)
			return false;
		else
		{
			LogError(MH_SOURCE_LOCATION_CURRENT(), "Unknown error code returned by {}: {}", FUNC_NAME, result);
			return false;
		}
	}();

	return s_IsInstalled;
}

bool tf2_bot_detector::Platform::CanInstallUpdate(const BuildInfo& bi)
{
	if (!IsInstalled())
		return false;

	if (bi.m_MSIXBundleURL.empty())
		return false;

	return true;
}

mh::task<InstallUpdate::Result> tf2_bot_detector::Platform::BeginInstallUpdate(
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

	DebugLogWarning(MH_SOURCE_LOCATION_CURRENT(),
		"Microsoft has ensured that we can't have anything nice, requesting update tool");

	co_return InstallUpdate::NeedsUpdateTool
	{
		.m_UpdateToolArgs = mh::format("--update-type MSIX --source-path {}", std::quoted(bundleUri)),
	};
}
