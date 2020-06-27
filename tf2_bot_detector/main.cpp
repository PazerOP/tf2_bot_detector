#include "MainWindow.h"

#ifdef _DEBUG
namespace tf2_bot_detector
{
	uint32_t g_StaticRandomSeed = 0;
	bool g_SkipOpenTF2Check = false;
}
#endif

int main(int argc, const char** argv)
{
	for (int i = 1; i < argc; i++)
	{
#ifdef _DEBUG
		if (!strcmp(argv[i], "--static-seed") && (i + 1) < argc)
			tf2_bot_detector::g_StaticRandomSeed = atoi(argv[i + 1]);
		else if (!strcmp(argv[i], "--allow-open-tf2"))
			tf2_bot_detector::g_SkipOpenTF2Check = true;
#endif
	}

	tf2_bot_detector::MainWindow window;

	while (!window.ShouldClose())
		window.Update();

	return 0;
}
