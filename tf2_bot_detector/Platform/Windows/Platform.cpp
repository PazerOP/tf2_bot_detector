#include "Platform/Platform.h"
#include "Log.h"

#include <Windows.h>

using namespace tf2_bot_detector;

Platform::OS Platform::GetOS()
{
	return OS::Windows;
}

Platform::Arch Platform::GetArch()
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
