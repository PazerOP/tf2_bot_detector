#include "Platform/Platform.h"
#include "Log.h"

#include <Windows.h>

const std::error_code tf2_bot_detector::Platform::ErrorCodes::PRIVILEGE_NOT_HELD(ERROR_PRIVILEGE_NOT_HELD, std::system_category());

tf2_bot_detector::Platform::OS tf2_bot_detector::Platform::GetOS()
{
	return OS::Windows;
}

tf2_bot_detector::Platform::Arch tf2_bot_detector::Platform::GetArch()
{
	static const Platform::Arch s_Arch = []
	{
		SYSTEM_INFO sysInfo;
		GetNativeSystemInfo(&sysInfo);
		switch (sysInfo.wProcessorArchitecture)
		{
		default:
			LogError(MH_SOURCE_LOCATION_CURRENT(),
				"Failed to identify processor architecture: wProcessorArchitecture = {}",
				sysInfo.wProcessorArchitecture);

			[[fallthrough]];
		case PROCESSOR_ARCHITECTURE_INTEL:
			return Arch::x86;
		case PROCESSOR_ARCHITECTURE_AMD64:
			return Arch::x64;
		}
	}();

	return s_Arch;
}

bool tf2_bot_detector::Platform::IsDebuggerAttached()
{
	return ::IsDebuggerPresent();
}
