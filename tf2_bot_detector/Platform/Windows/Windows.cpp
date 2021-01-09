#include "Platform/Platform.h"
#include "Util/TextUtils.h"
#include "Log.h"
#include "WindowsHelpers.h"
#include "tf2_bot_detector_winrt.h"

#include <mh/error/ensure.hpp>
#include <mh/text/codecvt.hpp>
#include <mh/text/format.hpp>
#include <mh/text/formatters/error_code.hpp>
#include <mh/text/stringops.hpp>

#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#include <minappmodel.h>
#include <Shlobj.h>
#include <versionhelpers.h>

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

static void* GetProcAddressHelper(const wchar_t* moduleName, const char* symbolName, bool isCritical = false, MH_SOURCE_LOCATION_AUTO(location))
{
	if (!moduleName)
		throw std::invalid_argument("moduleName was nullptr");
	if (!moduleName[0])
		throw std::invalid_argument("moduleName was empty");
	if (!symbolName)
		throw std::invalid_argument("symbolName was nullptr");
	if (!symbolName[0])
		throw std::invalid_argument("symbolName was empty");

	HMODULE moduleHandle = GetModuleHandleW(moduleName);
	if (!moduleHandle)
	{
		auto err = GetLastError();
		throw std::system_error(err, std::system_category(), mh::format("Failed to GetModuleHandle({})", mh::change_encoding<char>(moduleName)));
	}

	auto address = GetProcAddress(moduleHandle, symbolName);
	if (!address)
	{
		auto err = GetLastError();
		auto ec = std::error_code(err, std::system_category());

		auto msg = mh::format("{}: Failed to find function {} in {}", location, symbolName, mh::change_encoding<char>(moduleName));

		if (!isCritical)
			DebugLogWarning(location, "{}: {}", msg, ec);
		else
			throw std::system_error(ec, msg);
	}

	return address;
}

namespace tf2_bot_detector
{
	static const std::wstring& GetCurrentPackageFamilyName()
	{
		static const std::wstring s_CurrentPackageFamilyName = []() -> std::wstring
		{
			WCHAR name[PACKAGE_FAMILY_NAME_MAX_LENGTH + 1];
			UINT32 nameLength = UINT32(std::size(name));

			using func_type = LONG(*)(UINT32* packageFamilyNameLength, PWSTR packageFamilyName);

			const auto func = reinterpret_cast<func_type>(GetProcAddressHelper(L"Kernel32.dll", "GetCurrentPackageFamilyName", true));

			const auto errc = func(&nameLength, name);

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
		}();

		return s_CurrentPackageFamilyName;
	}
}

namespace
{
	class FallbackWinRTInterface final : public tf2_bot_detector::WinRT
	{
	public:
		std::filesystem::path GetLocalAppDataDir() const override
		{
			return GetKnownFolderPath(FOLDERID_LocalAppData);
		}
		std::filesystem::path GetRoamingAppDataDir() const override
		{
			return GetKnownFolderPath(FOLDERID_RoamingAppData);
		}
		std::filesystem::path GetTempDir() const override
		{
			return std::filesystem::temp_directory_path();
		}
	};
}

static const tf2_bot_detector::WinRT* GetWinRTInterface()
{
	struct WinRTHelper
	{
		WinRTHelper()
		{
			constexpr wchar_t WINRT_DLL_NAME[] = L"tf2_bot_detector_winrt.dll";
			m_Module = mh_ensure(LoadLibraryW(WINRT_DLL_NAME));

			CreateWinRTInterfaceFn func = reinterpret_cast<CreateWinRTInterfaceFn>(GetProcAddressHelper(WINRT_DLL_NAME, "CreateWinRTInterface"));

			m_WinRT.reset(func());
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
	};

	static std::optional<WinRTHelper> s_Helper = []() -> std::optional<WinRTHelper>
	{
		std::optional<WinRTHelper> helper;
		if (IsWindows10OrGreater())
			helper.emplace();

		return std::move(helper);
	}();
	static const FallbackWinRTInterface s_FallbackInterface;

	return s_Helper.has_value() ? s_Helper->m_WinRT.get() : &s_FallbackInterface;
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

std::filesystem::path tf2_bot_detector::Platform::GetRootLocalAppDataDir()
{
#if 0
	const auto lad = GetKnownFolderPath(FOLDERID_LocalAppData);

	const auto packageAppDataDir = lad / "Packages" / GetCurrentPackageFamilyName() / "LocalCache" / "Roaming";

	if (std::filesystem::exists(packageAppDataDir))
		return packageAppDataDir;
	else
		return GetAppDataDir();
#else
	return GetWinRTInterface()->GetLocalAppDataDir();
#endif
}

std::filesystem::path tf2_bot_detector::Platform::GetRootRoamingAppDataDir()
{
	return GetWinRTInterface()->GetRoamingAppDataDir();
}

std::filesystem::path tf2_bot_detector::Platform::GetRootTempDataDir()
{
	return GetWinRTInterface()->GetTempDir();
}
