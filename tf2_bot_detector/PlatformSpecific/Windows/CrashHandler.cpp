#ifdef _WIN32

#include <ctime>
#include <mutex>
#include <thread>

#include <Windows.h>

#pragma comment(lib, "DbgHelp.lib")
#include <DbgHelp.h>

namespace
{
	static LONG WINAPI ExceptionFilter(_EXCEPTION_POINTERS* exception);
	struct Init final
	{
		Init()
		{
			m_IsInitialized = SymInitialize(GetCurrentProcess(), nullptr, TRUE);
			m_OldFilter = SetUnhandledExceptionFilter(&ExceptionFilter);
		}
		~Init()
		{
			SetUnhandledExceptionFilter(m_OldFilter);
		}

		bool IsInitialized() const { return m_IsInitialized; }
		auto& GetMutex() { return m_Mutex; }

	private:
		std::recursive_mutex m_Mutex;
		bool m_IsInitialized = false;
		LPTOP_LEVEL_EXCEPTION_FILTER m_OldFilter = nullptr;

	} static s_DbgHelp;

	static void ExceptionHandlerThreadFunc(DWORD threadID, _EXCEPTION_POINTERS* exception)
	{
		std::lock_guard lock(s_DbgHelp.GetMutex());

		wchar_t filename[MAX_PATH] = L"tfbd_crash.mdmp";
		{
			auto ts = std::time(nullptr);
			tm localTS{};
			localtime_s(&localTS, &ts);
			wcsftime(filename, std::size(filename), L"tfbd_crash_%F-%H-%M-%S.mdmp", &localTS);
		}

		HANDLE file = CreateFileW(
			filename,
			GENERIC_WRITE,          // write perms
			0,                      // don't share
			nullptr,                // security attributes
			CREATE_ALWAYS,          // overwrite
			FILE_ATTRIBUTE_NORMAL,  // normal attributes
			nullptr                 // no template file
		);

		if (file != INVALID_HANDLE_VALUE)
		{
			MINIDUMP_EXCEPTION_INFORMATION info{};
			info.ThreadId = threadID;
			info.ExceptionPointers = exception;

			MINIDUMP_USER_STREAM_INFORMATION userStreams{};

			MiniDumpWriteDump(
				GetCurrentProcess(),
				GetProcessId(GetCurrentProcess()),
				file,
				MiniDumpNormal,
				&info,
				&userStreams,
				nullptr);

			CloseHandle(file);
		}
	}

	static std::thread s_ExceptionHandlerThread;
	static LONG WINAPI ExceptionFilter(_EXCEPTION_POINTERS* exception)
	{
		// Handle the minidump writing on another thread. This improves reliability
		// if we died because of a stack overflow or something.
		s_ExceptionHandlerThread = std::thread(&ExceptionHandlerThreadFunc, GetThreadId(GetCurrentThread()), exception);
		s_ExceptionHandlerThread.join();
		return EXCEPTION_CONTINUE_SEARCH;
	}
}

#endif
