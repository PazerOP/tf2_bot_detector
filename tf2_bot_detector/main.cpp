#include "MainWindow.h"
#include "Log.h"

#include <mh/text/string_insertion.hpp>

using namespace std::string_literals;

namespace tf2_bot_detector
{
#ifdef _DEBUG
	uint32_t g_StaticRandomSeed = 0;
	bool g_SkipOpenTF2Check = false;
#endif

	static void ImGuiDesktopLogFunc(const std::string_view& msg)
	{
		DebugLog("[ImGuiDesktop] "s << msg);
	}
}

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

	ImGuiDesktop::SetLogFunction(&tf2_bot_detector::ImGuiDesktopLogFunc);

	tf2_bot_detector::MainWindow window;

	while (!window.ShouldClose())
		window.Update();

	return 0;
}
