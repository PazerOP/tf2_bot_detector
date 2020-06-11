#include "../Shitty.h"
#include "Log.h"

#include <exception>

#include <Windows.h>

#ifdef _DEBUG
namespace tf2_bot_detector
{
	extern bool g_SkipOpenTF2Check;
}
#endif

void tf2_bot_detector::RequireTF2NotRunning()
{
	if (!FindWindowA("Valve001", nullptr))
		return;

#ifdef _DEBUG
	if (g_SkipOpenTF2Check)
	{
		LogWarning("TF2 was found running, but --allow-open-tf2 was on the command line. Letting execution proceed.");
	}
	else
#endif
	{
		MessageBoxA(nullptr, "TF2 Bot Detector must be started before Team Fortress 2.",
			"TF2 Currently Open", MB_OK | MB_ICONEXCLAMATION);

		std::exit(1);
	}
}
