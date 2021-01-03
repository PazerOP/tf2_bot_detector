#include "Platform/Platform.h"

#include <mh/error/ensure.hpp>
#include <mh/raii/scope_exit.hpp>
#include <mh/source_location.hpp>
#include <mh/text/codecvt.hpp>
#include <mh/text/format.hpp>

#include <chrono>
#include <iostream>
#include <thread>

#include <Windows.h>

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
