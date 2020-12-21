#include "Platform/Platform.h"

#include <mh/raii/scope_exit.hpp>
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

void tf2_bot_detector::Processes::Launch(const std::filesystem::path& executable, const std::string_view& arguments)
{
	const auto result = ShellExecuteW(
		NULL,
		L"open",
		executable.c_str(),
		mh::change_encoding<wchar_t>(arguments).c_str(),
		nullptr,
		SW_SHOWNORMAL);

	if (const auto iResult = reinterpret_cast<intptr_t>(result); iResult <= 32)
	{
		auto msg = mh::format("{}: ShellExecuteW returned {} (", __FUNCTION__, iResult);

#define ERROR_HELPER(value) \
	case value: msg += #value; \
	break;

		switch (iResult)
		{
		case 0:
			msg += "The operating system is out of memory or resources.";
			break;

			ERROR_HELPER(ERROR_FILE_NOT_FOUND);
			ERROR_HELPER(ERROR_PATH_NOT_FOUND);
			ERROR_HELPER(ERROR_BAD_FORMAT);
			ERROR_HELPER(SE_ERR_ACCESSDENIED);
			ERROR_HELPER(SE_ERR_ASSOCINCOMPLETE);
			ERROR_HELPER(SE_ERR_DDEBUSY);
			ERROR_HELPER(SE_ERR_DDEFAIL);
			ERROR_HELPER(SE_ERR_DDETIMEOUT);
			ERROR_HELPER(SE_ERR_DLLNOTFOUND);
			//ERROR_HELPER(SE_ERR_FNF); // Same as ERROR_FILE_NOT_FOUND
			ERROR_HELPER(SE_ERR_NOASSOC);
			ERROR_HELPER(SE_ERR_OOM);
			//ERROR_HELPER(SE_ERR_PNF); // Same as ERROR_PATH_NOT_FOUND
			ERROR_HELPER(SE_ERR_SHARE);

		default:
			msg += "unknown error";
			break;
		}

		msg += ")";

		std::cerr << msg << std::endl;
		throw std::runtime_error(msg);
	}
}
