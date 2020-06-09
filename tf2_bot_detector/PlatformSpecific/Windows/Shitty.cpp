#include "../Shitty.h"

#include <exception>

#include <Windows.h>

void tf2_bot_detector::RequireTF2NotRunning()
{
	if (!FindWindowA("Valve001", nullptr))
		return;

	MessageBoxA(nullptr, "TF2 Bot Detector must be started before Team Fortress 2.",
		"TF2 Currently Open", MB_OK | MB_ICONEXCLAMATION);

	std::exit(1);
}
