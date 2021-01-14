#define CPPHTTPLIB_OPENSSL_SUPPORT 1
#include "Platform/Platform.h"

#include <httplib.h>
#include <mh/error/ensure.hpp>
#include <mh/raii/scope_exit.hpp>
#include <mh/source_location.hpp>
#include <mh/text/codecvt.hpp>
#include <mh/text/format.hpp>

#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

#include <Windows.h>

#pragma comment(lib, "Version.lib")

using namespace std::chrono_literals;

void tf2_bot_detector::Platform::WaitForPIDToExit(int pid)
{
	if (auto handle = OpenProcess(SYNCHRONIZE, FALSE, pid))
	{
		mh::scope_exit scopeExit([&]
			{
				CloseHandle(handle);
			});

		std::cerr << "Waiting for PID " << pid << " to exit..." << std::endl;
		WaitForSingleObject(handle, INFINITE);
	}

	std::cerr << "Done waiting for PID " << pid << " to exit, waiting a bit more to be safe..." << std::endl;
	std::this_thread::sleep_for(250ms);
}

tf2_bot_detector::Platform::Arch tf2_bot_detector::Platform::GetArch()
{
	using tf2_bot_detector::Platform::Arch;
	Arch arch = Arch::x86;

	SYSTEM_INFO sysInfo;
	GetNativeSystemInfo(&sysInfo);
	if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
		arch = Arch::x64;

	return arch;
}

namespace
{
	struct HandleDeleter
	{
		void operator()(HANDLE h) const
		{
			mh_ensure(CloseHandle(h));
		}
	};
}

static std::unique_ptr<void, HandleDeleter> LaunchImpl(const std::filesystem::path& executable, const std::string_view& arguments)
{
	std::cerr << "Launching " << executable << "..." << std::endl;

	const auto wArgs = mh::change_encoding<wchar_t>(arguments);

	SHELLEXECUTEINFOW info{};
	info.cbSize = sizeof(info);
	info.lpVerb = L"open";
	info.lpFile = executable.c_str();
	info.lpParameters = wArgs.c_str();
	info.nShow = SW_SHOWNORMAL;
	info.fMask = SEE_MASK_NOCLOSEPROCESS;

	std::error_code ec;
	if (!ShellExecuteExW(&info))
	{
		auto err = GetLastError();
		throw std::system_error(err, std::system_category(), mh::format("ShellExecuteExW failed for {} {}", executable, arguments));
	}

	return std::unique_ptr<void, HandleDeleter>(info.hProcess);
}

void tf2_bot_detector::Processes::Launch(const std::filesystem::path& executable, const std::string_view& arguments)
{
	LaunchImpl(executable, arguments);
}

int tf2_bot_detector::Processes::LaunchAndWait(const std::filesystem::path& executable, const std::string_view& arguments)
{
	auto handle = LaunchImpl(executable, arguments);

	std::cerr << "\tWaiting for " << executable.filename() << " to close..." << std::endl;

	{
		const auto waitResult = WaitForSingleObject(handle.get(), INFINITE);
		switch (waitResult)
		{
		default:
			throw std::runtime_error(mh::format(MH_FMT_STRING("{}: Unexpected result from WaitForSingleObject: {:x}"),
				mh::source_location::current(), waitResult));
		case WAIT_ABANDONED:
			throw std::runtime_error(mh::format("{}: WAIT_ABANDONED (this should never happen???)", mh::source_location::current()));
		case WAIT_OBJECT_0:
			break;
		case WAIT_TIMEOUT:
			throw std::runtime_error(mh::format("{}: WAIT_TIMEOUT (this should never happen???)", mh::source_location::current()));
		case WAIT_FAILED:
		{
			auto err = GetLastError();
			throw std::system_error(err, std::system_category(), mh::format("{}: WAIT_FAILED", mh::source_location::current()));
		}
		}
	}

	DWORD exitCode{};
	if (!GetExitCodeProcess(handle.get(), &exitCode))
	{
		auto err = GetLastError();
		throw std::system_error(err, std::system_category(), mh::format("GetExitCodeProcess failed for {} {}", executable, arguments));
	}

	if (exitCode)
		std::cerr << mh::format(MH_FMT_STRING("\t{} returned 0x{:x}"), executable.filename(), exitCode) << std::endl;

	return exitCode;
}

void tf2_bot_detector::RebootComputer()
{
	std::unique_ptr<void, HandleDeleter> tokenHandle;
	if (HANDLE hToken; OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
	{
		tokenHandle.reset(hToken);
	}
	else
	{
		auto err = GetLastError();
		throw std::system_error(err, std::system_category(), mh::format("{}: OpenProcessToken failed", mh::source_location::current()));
	}

	LUID shutdownPrivilege;
	if (!LookupPrivilegeValue(nullptr, SE_SHUTDOWN_NAME, &shutdownPrivilege))
	{
		auto err = GetLastError();
		throw std::system_error(err, std::system_category(), mh::format("{}: LookupPrivilegeValue failed", mh::source_location::current()));
	}

	TOKEN_PRIVILEGES privileges{};
	privileges.PrivilegeCount = 1;
	privileges.Privileges[0].Luid = shutdownPrivilege;
	privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	AdjustTokenPrivileges(tokenHandle.get(), false, &privileges, sizeof(privileges), nullptr, nullptr);
	{
		auto err = GetLastError();
		if (err != ERROR_SUCCESS)
			throw std::system_error(err, std::system_category(), mh::format("{}: AdjustTokenPrivileges failed", mh::source_location::current()));
	}

	if (!ExitWindowsEx(EWX_REBOOT | EWX_RESTARTAPPS, SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_MINOR_INSTALLATION | SHTDN_REASON_FLAG_PLANNED))
	{
		auto err = GetLastError();
		throw std::system_error(err, std::system_category(), "ExitWindowsEx failed");
	}
}

MH_ENUM_REFLECT_BEGIN(httplib::Error)
	MH_ENUM_REFLECT_VALUE(Success)
	MH_ENUM_REFLECT_VALUE(Unknown)
	MH_ENUM_REFLECT_VALUE(Connection)
	MH_ENUM_REFLECT_VALUE(BindIPAddress)
	MH_ENUM_REFLECT_VALUE(Read)
	MH_ENUM_REFLECT_VALUE(Write)
	MH_ENUM_REFLECT_VALUE(ExceedRedirectCount)
	MH_ENUM_REFLECT_VALUE(Canceled)
	MH_ENUM_REFLECT_VALUE(SSLConnection)
	MH_ENUM_REFLECT_VALUE(SSLLoadingCerts)
	MH_ENUM_REFLECT_VALUE(SSLServerVerification)
	MH_ENUM_REFLECT_VALUE(UnsupportedMultipartBoundaryChars)
MH_ENUM_REFLECT_END()

namespace
{
	union Version
	{
		uint64_t v64{};

		struct
		{
			uint32_t lo;
			uint32_t hi;
		} v32;

		struct
		{
			uint16_t revision;
			uint16_t patch;
			uint16_t minor;
			uint16_t major;
		};

		friend constexpr std::strong_ordering operator<=>(const Version& lhs, const Version& rhs)
		{
			if (auto result = lhs.major <=> rhs.major; !std::is_eq(result))
				return result;
			if (auto result = lhs.minor <=> rhs.minor; !std::is_eq(result))
				return result;
			if (auto result = lhs.patch <=> rhs.patch; !std::is_eq(result))
				return result;
			if (auto result = lhs.revision <=> rhs.revision; !std::is_eq(result))
				return result;

			return std::strong_ordering::equal;
		}

		friend constexpr bool operator==(const Version& lhs, const Version& rhs)
		{
			return lhs.v64 == rhs.v64;
		}
	};

	template<typename CharT, typename Traits>
	std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const Version& v)
	{
		return os << +v.major << '.' << +v.minor << '.' << +v.patch << '.' << +v.revision;
	}
}

static Version GetFileProductVersion(const std::filesystem::path& filename) try
{
	DWORD dummy;
	const DWORD fileVersionInfoSize = GetFileVersionInfoSizeW(filename.c_str(), &dummy);
	if (fileVersionInfoSize == 0)
	{
		auto err = GetLastError();
		throw std::system_error(err, std::system_category(), "GetFileVersionInfoSize failed");
	}

	auto buf = std::make_unique<std::byte[]>(fileVersionInfoSize);

	if (!GetFileVersionInfoW(filename.c_str(), 0, fileVersionInfoSize, buf.get()))
	{
		auto err = GetLastError();
		throw std::system_error(err, std::system_category(), "GetFileVersionInfo failed");
	}

	VS_FIXEDFILEINFO* fileInfo = nullptr;
	UINT fileInfoSize{};
	if (!VerQueryValueW(buf.get(), L"\\", reinterpret_cast<LPVOID*>(&fileInfo), &fileInfoSize))
	{
		auto err = GetLastError();
		throw std::system_error(err, std::system_category(), "VerQueryValue failed");
	}

	Version version;
	version.v32.lo = fileInfo->dwProductVersionLS;
	version.v32.hi = fileInfo->dwProductVersionMS;

	return version;
}
catch (const std::exception& e)
{
	std::cerr << mh::format(MH_FMT_STRING("Unhandled exception ({}) in {}: {}"),
		typeid(e).name(), __FUNCTION__, e.what()) << std::endl;
	throw;
}

static Version GetInstalledVCRedistVersion(const Version& downloadedVersion) try
{
	char buf[256]{};
	DWORD bufSize = std::size(buf);
	DWORD type{};

	const auto arch = tf2_bot_detector::Platform::GetArch();
	std::string keyPath = "Installer\\Dependencies\\VC,redist.";
	{
		switch (arch)
		{
		case tf2_bot_detector::Platform::Arch::x64:
			keyPath += "x64,amd64";
			break;
		case tf2_bot_detector::Platform::Arch::x86:
			keyPath += "x86,x86";
			break;
		default:
			throw std::logic_error(mh::format(MH_FMT_STRING("Unsupported arch {} in {}"), ((int)arch), __FUNCTION__));
		}

		keyPath += mh::format(MH_FMT_STRING(",{}.{},bundle"), downloadedVersion.major, downloadedVersion.minor);
	}

	LSTATUS result = RegGetValueA(HKEY_CLASSES_ROOT, keyPath.c_str(), "Version",
		RRF_RT_REG_SZ, &type, buf, &bufSize);

	if (result != ERROR_SUCCESS)
	{
		throw std::system_error(result, std::system_category(),
			mh::format(MH_FMT_STRING("RegGetValueA for {} failed"), std::quoted(keyPath)));
	}

	Version installedVersion;
	{
		const auto scannedCount = sscanf_s(buf, "%hi.%hi.%hi.%hi",
			&installedVersion.major, &installedVersion.minor, &installedVersion.patch, &installedVersion.revision);

		if (scannedCount != 4)
		{
			throw std::runtime_error(mh::format(
				MH_FMT_STRING("Registry key {} contained {}, which could not be parsed into 4 uint16"),
				std::quoted(keyPath), std::quoted(buf)));
		}
	}

	return installedVersion;
}
catch (const std::exception& e)
{
	std::cerr << mh::format(MH_FMT_STRING("Unhandled exception ({}) in {}: {}"),
		typeid(e).name(), __FUNCTION__, e.what()) << std::endl;
	throw;
}

static std::filesystem::path DownloadLatestVCRedist() try
{
	auto arch = tf2_bot_detector::Platform::GetArch();
	const auto filename = mh::format(MH_FMT_STRING("vc_redist.{:v}.exe"), mh::enum_fmt(arch));
	const auto urlPath = mh::format(MH_FMT_STRING("/vs/16/release/{}"), filename);
	std::cerr << mh::format(MH_FMT_STRING("Downloading latest vcredist from https://aka.ms{}..."), urlPath) << std::endl;

	httplib::SSLClient downloader("aka.ms", 443);
	downloader.set_follow_location(true);

	httplib::Headers headers =
	{
		{ "User-Agent", "curl/7.58.0" }
	};

	httplib::Result result = downloader.Get(urlPath.c_str(), headers);

	if (!result)
		throw std::runtime_error(mh::format(MH_FMT_STRING("Failed to download latest vcredist: {}"), mh::enum_fmt(result.error())));

	auto savePath = std::filesystem::temp_directory_path() / "TF2 Bot Detector" / "Updater_VCRedist";
	std::cerr << "Creating " << savePath << "..." << std::endl;
	std::filesystem::create_directories(savePath);

	savePath /= filename;
	std::cerr << "Saving latest vcredist to " << savePath << "..." << std::endl;
	{
		std::ofstream file;
		file.exceptions(std::ios::badbit | std::ios::failbit);
		file.open(savePath, std::ios::binary);
		file.write(result->body.c_str(), result->body.size());
	}

	return savePath;
}
catch (const std::exception& e)
{
	std::cerr << mh::format(MH_FMT_STRING("Unhandled exception ({}) in {}: {}"),
		typeid(e).name(), __FUNCTION__, e.what()) << std::endl;
	throw;
}

static tf2_bot_detector::Platform::UpdateSystemDependenciesResult InstallVCRedist(const std::filesystem::path& vcredistPath) try
{
	using Result = tf2_bot_detector::Platform::UpdateSystemDependenciesResult;

	std::cerr << "Installing latest vcredist..." << std::endl;
	auto vcRedistResult = tf2_bot_detector::Platform::Processes::LaunchAndWait(vcredistPath, "/install /quiet /norestart");
	const auto vcRedistErrorCode = std::error_code(vcRedistResult, std::system_category());

	switch (vcRedistResult)
	{
	case ERROR_SUCCESS:
		std::cerr << "vcredist install successful." << std::endl;
		break;

	case ERROR_PRODUCT_VERSION:
		std::cerr << "vcredist already installed." << std::endl;
		break;

	case ERROR_SUCCESS_REBOOT_REQUIRED:
	{
		const auto result = MessageBoxA(nullptr,
			"TF2 Bot Detector Updater has successfully installed the latest Microsoft Visual C++ Redistributable. Some programs (including TF2 Bot Detector) might not work properly until you restart your computer.\n\nWould you like to restart your computer now?",
			"Reboot Recommended", MB_YESNO | MB_ICONINFORMATION);

		return result == IDYES ? Result::RebootRequired : Result::Success;
	}

	default:
		MessageBoxA(nullptr,
			mh::format("TF2 Bot Detector attempted to install the latest Microsoft Visual C++ Redistributable, but there was an error:\n\n{}\n\nTF2 Bot Detector might not work correctly. If possible, please report this error on the TF2 Bot Detector discord server.", vcRedistErrorCode).c_str(), "Unknown Error", MB_OK | MB_ICONWARNING);
		break;
	}

	std::cerr << "Finished installing latest vcredist.\n\n" << std::flush;
	return Result::Success;
}
catch (const std::exception& e)
{
	std::cerr << mh::format(MH_FMT_STRING("Unhandled exception ({}) in {}: {}"),
		typeid(e).name(), __FUNCTION__, e.what()) << std::endl;
	throw;
}

static bool ShouldInstallVCRedist(const std::filesystem::path& vcredistPath) try
{
	std::cerr << "\nChecking downloaded vcredist version..." << std::endl;
	const auto downloadedVCRedistVersion = GetFileProductVersion(vcredistPath);
	std::cerr << "\tDownloaded vcredist version: " << downloadedVCRedistVersion << std::endl;

	std::cerr << "\nChecking installed vcredist version..." << std::endl;
	Version installedVCRedistVersion;
	try
	{
		installedVCRedistVersion = GetInstalledVCRedistVersion(downloadedVCRedistVersion);
	}
	catch (...)
	{
		std::cerr << "\tGetInstalledVCRedistVersion() failed, installing vcredist..." << std::endl;
		return true;
	}

	std::cerr << "\tInstalled vcredist version: " << installedVCRedistVersion << std::endl;

	if (installedVCRedistVersion < downloadedVCRedistVersion)
	{
		std::cerr << "\nInstalled vcredist version older, installing vcredist..." << std::endl;
		return true;
	}
	else
	{
		std::cerr << "\nInstalled vcredist version newer or identical, skipping vcredist install." << std::endl;
		return false;
	}
}
catch (const std::exception& e)
{
	std::cerr << mh::format(MH_FMT_STRING("Unhandled exception ({}) in {}: {}"),
		typeid(e).name(), __FUNCTION__, e.what()) << std::endl;

	std::cerr << "\nChoosing to install vcredist due to unknown error determining vcredist versions." << std::endl;
	return true;
}

tf2_bot_detector::Platform::UpdateSystemDependenciesResult tf2_bot_detector::Platform::UpdateSystemDependencies() try
{
	const auto vcredistPath = DownloadLatestVCRedist();

	if (ShouldInstallVCRedist(vcredistPath))
		return InstallVCRedist(vcredistPath);
	else
		return tf2_bot_detector::Platform::UpdateSystemDependenciesResult::Success;
}
catch (const std::exception& e)
{
	std::cerr << mh::format(MH_FMT_STRING("Unhandled exception ({}) in {}: {}"),
		typeid(e).name(), __FUNCTION__, e.what()) << std::endl;
	throw;
}
