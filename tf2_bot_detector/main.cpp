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

#ifdef _WIN32
#include <Windows.h>
#include "TextUtils.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
	int argc;
	auto argvw = CommandLineToArgvW(GetCommandLineW(), &argc);

	std::vector<std::string> argvStrings;
	std::vector<const char*> argv;

	for (int i = 0; i < argc; i++)
	{
		argvStrings.push_back(tf2_bot_detector::ToMB(argvw[i]));
		argv.push_back(argvStrings.back().c_str());
	}

	return main(argc, argv.data());
}
#endif
