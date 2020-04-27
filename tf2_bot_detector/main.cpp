#include "MainWindow.h"

extern "C" int SDL_main(int argc, const char** argv)
{
	tf2_bot_detector::MainWindow window;

	while (!window.ShouldClose())
		window.Update();

	return 0;
}
