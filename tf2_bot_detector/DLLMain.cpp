#include "DLLMain.h"

#include "Application.h"
#include "Tests/Tests.h"
#include "UI/MainWindow.h"
#include "Util/TextUtils.h"
#include "Log.h"
#include "Filesystem.h"

#include <mh/text/string_insertion.hpp>

#ifdef WIN32
#include "Platform/Windows/WindowsHelpers.h"
#include <Windows.h>
#include <Objbase.h>
#include <shellapi.h>
#endif

using namespace std::string_literals;

namespace tf2_bot_detector
{
#ifdef _DEBUG
	uint32_t g_StaticRandomSeed = 0;
	bool g_SkipOpenTF2Check = false;
#endif

	static void ImGuiDesktopLogFunc(const std::string_view& msg, const mh::source_location& location)
	{
		DebugLog(location, "[ImGuiDesktop] {}", msg);
	}
}

TF2_BOT_DETECTOR_EXPORT int tf2_bot_detector::RunProgram(int argc, const char** argv)
{
	{
#ifdef _WIN32
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
#endif

		IFilesystem::Get().Init();
		ILogManager::GetInstance().Init();

		for (int i = 1; i < argc; i++)
		{
#ifdef _DEBUG
			if (!strcmp(argv[i], "--static-seed") && (i + 1) < argc)
				tf2_bot_detector::g_StaticRandomSeed = atoi(argv[i + 1]);
			else if (!strcmp(argv[i], "--allow-open-tf2"))
				tf2_bot_detector::g_SkipOpenTF2Check = true;
			else if (!strcmp(argv[i], "--run-tests"))
			{
#ifdef TF2BD_ENABLE_TESTS
				return tf2_bot_detector::RunTests();
#else
				LogError("--run-tests was on the command line, but tests were not compiled in");
#endif
			}
#endif
		}

#if defined(_DEBUG) && defined(TF2BD_ENABLE_TESTS)
		// Always run the tests debug builds (but don't quit afterwards)
		tf2_bot_detector::RunTests();
#endif

		ImGuiDesktop::SetLogFunction(&tf2_bot_detector::ImGuiDesktopLogFunc);

		DebugLog("Initializing TF2BDApplication...");
		TF2BDApplication app;

		DebugLog("Entering event loop...");
		while (!app.ShouldQuit())
			app.Update();
	}

	DebugLog("Graceful shutdown");
	return 0;
}

#ifdef WIN32
TF2_BOT_DETECTOR_EXPORT int tf2_bot_detector::RunProgram(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
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
