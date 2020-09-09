#include "Platform/Platform.h"

#include <mh/raii/scope_exit.hpp>
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

void tf2_bot_detector::Processes::Launch(const std::filesystem::path& executable)
{
	const auto result = ShellExecuteW(
		NULL,
		L"open",
		executable.c_str(),
		L"",
		nullptr,
		SW_SHOWNORMAL);

	if (const auto iResult = reinterpret_cast<intptr_t>(result); iResult <= 32)
	{
		auto msg = mh::format("{}: ShellExecuteW returned {}", __FUNCTION__, iResult);
		std::cerr << msg << std::endl;
		throw std::runtime_error(msg);
	}
}
