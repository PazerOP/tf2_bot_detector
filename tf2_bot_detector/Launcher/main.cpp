#include "../DLLMain.h"

int main(int argc, const char** argv)
{
	return tf2_bot_detector::RunProgram(argc, argv);
}

#ifdef WIN32
#include <codecvt>
#include <string>
#include <vector>

#include <Windows.h>
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
	int argc;
	auto argvw = CommandLineToArgvW(GetCommandLineW(), &argc);

	std::vector<std::string> argvStrings;
	std::vector<const char*> argv;
	argvStrings.reserve(argc);
	argv.reserve(argc);

	for (int i = 0; i < argc; i++)
	{
		std::wstring_convert<std::codecvt<wchar_t, char, std::mbstate_t>, wchar_t> converter;
		argvStrings.push_back(converter.to_bytes(argvw[i]));
		argv.push_back(argvStrings.back().c_str());
	}

	return main(argc, argv.data());
}
#endif
