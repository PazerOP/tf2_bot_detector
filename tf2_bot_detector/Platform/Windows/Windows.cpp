#include "Platform/Platform.h"
#include "Util/TextUtils.h"
#include "Log.h"
#include "WindowsHelpers.h"

#include <mh/text/format.hpp>
#include <mh/text/stringops.hpp>

#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#include <appmodel.h>
#include <Shlobj.h>

using namespace std::string_view_literals;

std::filesystem::path tf2_bot_detector::Platform::GetCurrentExeDir()
{
	WCHAR path[32768];
	const auto length = GetModuleFileNameW(nullptr, path, std::size(path));

	const auto error = GetLastError();
	if (error != ERROR_SUCCESS)
		throw tf2_bot_detector::Windows::GetLastErrorException(E_FAIL, error, "Call to GetModuleFileNameW() failed");

	if (length == 0)
		throw std::runtime_error("Call to GetModuleFileNameW() failed: return value was 0");

	return std::filesystem::path(path, path + length).remove_filename();
}

namespace tf2_bot_detector
{
	static std::wstring GetCurrentPackageFamilyName()
	{
		WCHAR name[PACKAGE_FAMILY_NAME_MAX_LENGTH + 1];
		UINT32 nameLength = std::size(name);
		const auto errc = ::GetCurrentPackageFamilyName(&nameLength, name);

		switch (errc)
		{
		case ERROR_SUCCESS:
			return std::wstring(name, nameLength > 0 ? (nameLength - 1) : 0);
		case APPMODEL_ERROR_NO_PACKAGE:
			return {};
		case ERROR_INSUFFICIENT_BUFFER:
			throw std::runtime_error(mh::format("{}: Buffer too small", __FUNCTION__));
		default:
			throw std::runtime_error(mh::format("{}: Unknown error {}", __FUNCTION__, errc));
		}
	}

	static std::filesystem::path GetCurrentPackagePath()
	{
		WCHAR path[32768];
		UINT32 pathLength = std::size(path);
		const auto errc = ::GetCurrentPackagePath(&pathLength, path);

		switch (errc)
		{
		case ERROR_SUCCESS:
			return std::filesystem::path(path, path + (pathLength > 0 ? (pathLength - 1) : 0));
		case APPMODEL_ERROR_NO_PACKAGE:
			return "";// GetAppDataDir(); // We're not in a package
		case ERROR_INSUFFICIENT_BUFFER:
			throw std::runtime_error(mh::format("{}: Buffer too small", __FUNCTION__));
		default:
			throw std::runtime_error(mh::format("{}: Unknown error {}", __FUNCTION__, errc));
		}
	}
}

static std::filesystem::path GetKnownFolderPath(const KNOWNFOLDERID& id)
{
	PWSTR str;
	CHECK_HR(SHGetKnownFolderPath(id, 0, nullptr, &str));

	std::filesystem::path retVal(str);

	CoTaskMemFree(str);
	return retVal;
}

std::filesystem::path tf2_bot_detector::Platform::GetRealAppDataDir()
{
	const auto lad = GetKnownFolderPath(FOLDERID_LocalAppData);

	const auto packageAppDataDir = lad / "Packages" / GetCurrentPackageFamilyName() / "LocalCache" / "Roaming";

	if (std::filesystem::exists(packageAppDataDir))
		return packageAppDataDir;
	else
		return GetAppDataDir();
}

std::filesystem::path tf2_bot_detector::Platform::GetAppDataDir()
{
	return GetKnownFolderPath(FOLDERID_RoamingAppData);
}
