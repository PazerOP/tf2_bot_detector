#include "../DLLMain.h"

#ifdef WIN32
#include <Windows.h>
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
	return tf2_bot_detector::RunProgram(hInstance, hPrevInstance, pCmdLine, nCmdShow);
}
#else
int main(int argc, const char** argv)
{
	return tf2_bot_detector::RunProgram(argc, argv);
}
#endif
