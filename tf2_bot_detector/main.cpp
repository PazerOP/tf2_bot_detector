#include "MainWindow.h"

int main()
{
	tf2_bot_detector::MainWindow window;

	while (!window.ShouldClose())
		window.Update();

	return 0;
}
