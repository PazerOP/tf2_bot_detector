#include <ntstatus.h>
#define WIN32_NO_STATUS

#include "Platform/Platform.h"
#include "Platform/PlatformCommon.h"
#include "Util/TextUtils.h"
#include "Log.h"
#include "WindowsHelpers.h"
#include "tf2_bot_detector_winrt.h"

#include <mh/error/ensure.hpp>
#include <mh/error/exception_details.hpp>
#include <mh/text/codecvt.hpp>
#include <mh/text/format.hpp>
#include <mh/text/formatters/error_code.hpp>
#include <mh/text/stringops.hpp>

#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#include <Shlobj.h>
#include <winternl.h>

using namespace tf2_bot_detector;
using namespace std::string_view_literals;

namespace tf2_bot_detector
{
	class WinRT;
}

static std::filesystem::path GetKnownFolderPath(const KNOWNFOLDERID& id)
{
	PWSTR str;
	CHECK_HR(SHGetKnownFolderPath(id, 0, nullptr, &str));

	std::filesystem::path retVal(str);

	CoTaskMemFree(str);
	return retVal;
}

static bool IsReallyWindows10OrGreater()
{
	using RtlGetVersionFn = NTSTATUS(WINAPI*)(PRTL_OSVERSIONINFOW lpVersionInformation);

	static const auto s_RtlGetVersionFn = reinterpret_cast<RtlGetVersionFn>(
		tf2_bot_detector::Platform::GetProcAddressHelper("ntdll.dll", "RtlGetVersion", true));

	RTL_OSVERSIONINFOW info{};
	info.dwOSVersionInfoSize = sizeof(info);
	const auto result = s_RtlGetVersionFn(&info);
	assert(result == STATUS_SUCCESS);

	return info.dwMajorVersion >= 10;
}

static const tf2_bot_detector::WinRT* GetWinRTInterface()
{
	struct WinRTHelper
	{
		WinRTHelper()
		{
			constexpr char WINRT_DLL_NAME[] = "tf2_bot_detector_winrt.dll";
			m_Module = mh_ensure(LoadLibraryA(WINRT_DLL_NAME));

			CreateWinRTInterfaceFn func = reinterpret_cast<CreateWinRTInterfaceFn>(GetProcAddressHelper(WINRT_DLL_NAME, "CreateWinRTInterface"));

			m_WinRT.reset(func());

			struct DummyType {};
			m_ExceptionDetailsHandler = mh::exception_details::add_handler(
				typeid(DummyType), m_WinRT->GetWinRTExceptionDetailsHandler());
		}
		WinRTHelper(WinRTHelper&& other) noexcept :
			m_Module(std::exchange(other.m_Module, nullptr)),
			m_WinRT(std::move(other.m_WinRT))
		{
		}
		WinRTHelper& operator=(WinRTHelper&& other) noexcept
		{
			destroy();

			m_Module = std::exchange(other.m_Module, nullptr);
			m_WinRT = std::move(other.m_WinRT);

			return *this;
		}
		~WinRTHelper()
		{
			destroy();
		}

		void destroy()
		{
			m_WinRT.reset();

			if (m_Module)
			{
				mh_ensure(FreeLibrary(m_Module));
				m_Module = {};
			}
		}

		HMODULE m_Module{};
		std::unique_ptr<WinRT> m_WinRT;
		mh::exception_details::handler m_ExceptionDetailsHandler;
	};

	static const tf2_bot_detector::WinRT* s_Value = []() -> const tf2_bot_detector::WinRT*
	{
		if (IsReallyWindows10OrGreater())
		{
			static WinRTHelper s_Helper;
			return s_Helper.m_WinRT.get();
		}

		return nullptr;
	}();

	return s_Value;
}

std::filesystem::path tf2_bot_detector::Platform::GetCurrentExeDir()
{
	WCHAR path[32768];
	const auto length = GetModuleFileNameW(nullptr, path, (DWORD)std::size(path));

	const auto error = GetLastError();
	if (error != ERROR_SUCCESS)
		throw tf2_bot_detector::Windows::GetLastErrorException(E_FAIL, error, "Call to GetModuleFileNameW() failed");

	if (length == 0)
		throw std::runtime_error("Call to GetModuleFileNameW() failed: return value was 0");

	return std::filesystem::path(path, path + length).remove_filename();
}

std::filesystem::path tf2_bot_detector::Platform::GetLegacyAppDataDir()
{
	if (auto winrt = GetWinRTInterface())
	{
		try
		{
			auto packageFamilyName = GetWinRTInterface()->GetCurrentPackageFamilyName();
			if (packageFamilyName.empty())
				return {};

			return GetKnownFolderPath(FOLDERID_LocalAppData) / "Packages" / packageFamilyName / "LocalCache" / "Roaming";
		}
		catch (...)
		{
			LogException();
		}
	}

	return {};
}

std::filesystem::path tf2_bot_detector::Platform::GetRootLocalAppDataDir()
{
	if (auto winrt = GetWinRTInterface())
	{
		try
		{
			if (winrt->IsInPackage())
				return winrt->GetPackageLocalAppDataDir();
		}
		catch (...)
		{
			LogException();
		}
	}

	return GetKnownFolderPath(FOLDERID_LocalAppData);
}

std::filesystem::path tf2_bot_detector::Platform::GetRootRoamingAppDataDir()
{
	if (auto winrt = GetWinRTInterface())
	{
		try
		{
			if (winrt->IsInPackage())
				return winrt->GetPackageRoamingAppDataDir();
		}
		catch (...)
		{
			LogException();
		}
	}

	return GetKnownFolderPath(FOLDERID_RoamingAppData);
}

std::filesystem::path tf2_bot_detector::Platform::GetRootTempDataDir()
{
	if (auto winrt = GetWinRTInterface())
	{
		try
		{
			if (winrt->IsInPackage())
				return winrt->GetPackageTempDir();
		}
		catch (...)
		{
			LogException();
		}
	}

	return std::filesystem::temp_directory_path();
}
