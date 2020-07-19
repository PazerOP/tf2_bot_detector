#pragma once

#include "tf2_bot_detector_export.h"

#ifdef WIN32
#include <Windows.h>
#endif

namespace tf2_bot_detector
{
	TF2_BOT_DETECTOR_EXPORT int RunProgram(int argc, const char** argv);

#ifdef WIN32
	TF2_BOT_DETECTOR_EXPORT int RunProgram(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow);
#endif
}
