#include "MainWindow.h"

#ifdef _DEBUG
namespace tf2_bot_detector
{
	bool g_StaticRandomSeed = false;
}
#endif

extern "C" int SDL_main(int argc, const char** argv)
{
	for (int i = 1; i < argc; i++)
	{
#ifdef _DEBUG
		if (!strcmp(argv[i], "-staticseed"))
			tf2_bot_detector::g_StaticRandomSeed = true;
#endif
	}

	tf2_bot_detector::MainWindow window;

	while (!window.ShouldClose())
		window.Update();

	return 0;
}
