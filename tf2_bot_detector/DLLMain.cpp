#include "DLLMain.h"

#include "MainWindow.h"
#include "Log.h"

#include <mh/text/string_insertion.hpp>

#ifdef WIN32
#include "Platform/Windows/WindowsHelpers.h"
#include <Windows.h>
#endif

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

TF2_BOT_DETECTOR_EXPORT int tf2_bot_detector::RunProgram(int argc, const char** argv)
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

	DebugLog(""s << __func__ << "(): Graceful shutdown");
	return 0;
}

#ifdef WIN32
TF2_BOT_DETECTOR_EXPORT int tf2_bot_detector::RunProgram(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
	using tf2_bot_detector::Windows::GetLastErrorException;
	try
	{
		const auto langID = MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT);

		//if (!SetThreadLocale(langID))
		//	throw std::runtime_error("Failed to SetThreadLocale()");
		if (SetThreadUILanguage(langID) != langID)
			throw std::runtime_error("Failed to SetThreadUILanguage()");
		//if (ULONG langs = 1; !SetThreadPreferredUILanguages(MUI_LANGUAGE_NAME, L"pl-PL\0", &langs))
		//	throw std::runtime_error("Failed to SetThreadPreferredUILanguages()");

		const auto err = std::error_code(10035, std::system_category());
		const auto errMsg = err.message();

		CHECK_HR(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

		CHECK_HR(CoInitializeSecurity(NULL,
			-1,                          // COM authentication
			NULL,                        // Authentication services
			NULL,                        // Reserved
			RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication
			RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation
			NULL,                        // Authentication info
			EOAC_NONE,                   // Additional capabilities
			NULL)                        // Reserved
		);
	}
	catch (const std::exception& e)
	{
		MessageBoxA(nullptr, e.what(), "Initialization failed", MB_OK);
		return 1;
	}

	int argc;
	auto argvw = CommandLineToArgvW(GetCommandLineW(), &argc);

	std::vector<std::string> argvStrings;
	std::vector<const char*> argv;
	argvStrings.reserve(argc);
	argv.reserve(argc);

	for (int i = 0; i < argc; i++)
	{
		argvStrings.push_back(tf2_bot_detector::ToMB(argvw[i]));
		argv.push_back(argvStrings.back().c_str());
	}

	return RunProgram(argc, argv.data());
}
#endif
