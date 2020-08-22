#include "Platform/Platform.h"
#include "WindowsHelpers.h"

#include <mh/text/format.hpp>

#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#include <appmodel.h>
#include <Shlobj.h>

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

std::filesystem::path tf2_bot_detector::Platform::GetRealAppDataDir()
{
	WCHAR path[32768];
	UINT32 pathLength = std::size(path);
	const auto errc = GetCurrentPackagePath(&pathLength, path);

	switch (errc)
	{
	case ERROR_SUCCESS:
		return std::filesystem::path(path, path + pathLength);
	case APPMODEL_ERROR_NO_PACKAGE:
		return "";// GetAppDataDir(); // We're not in a package
	case ERROR_INSUFFICIENT_BUFFER:
		throw std::runtime_error(mh::format("{}: Buffer too small", __FUNCTION__));
	default:
		throw std::runtime_error(mh::format("{}: Unknown error {}", __FUNCTION__, errc));
	}
}

std::filesystem::path tf2_bot_detector::Platform::GetAppDataDir()
{
	auto test = GetRealAppDataDir();
	PWSTR str;
	CHECK_HR(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &str));

	std::filesystem::path retVal(str);

	CoTaskMemFree(str);
	return retVal;
}
