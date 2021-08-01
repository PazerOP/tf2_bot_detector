#include "../Platform.h"
#include "Util/TextUtils.h"
#include "Log.h"

#include <mh/coroutine/future.hpp>
#include <mh/memory/cached_variable.hpp>
#include <mh/error/ensure.hpp>
#include <mh/text/case_insensitive_string.hpp>
#include <mh/text/codecvt.hpp>
#include <mh/text/formatters/error_code.hpp>
#include <mh/text/insertion_conversion.hpp>
#include <mh/text/string_insertion.hpp>

#include <atomic>
#include <cassert>
#include <chrono>
#include <exception>
#include <mutex>
#include <thread>

#include <Windows.h>
#include <shellapi.h>
#include "WindowsHelpers.h"
#include <comutil.h>
#include <Wbemidl.h>
#include <TlHelp32.h>
#include <wrl/client.h>
#include <Psapi.h>

#undef min
#undef max

#ifdef _DEBUG
#pragma comment(lib, "comsuppwd.lib")
#pragma comment(lib, "comsuppd.lib")
#else
#pragma comment(lib, "comsuppw.lib")
#pragma comment(lib, "comsupp.lib")
#endif
#pragma comment(lib, "wbemuuid.lib")

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace tf2_bot_detector;
using namespace tf2_bot_detector::Windows;
using Microsoft::WRL::ComPtr;

#ifdef _DEBUG
namespace tf2_bot_detector
{
	extern bool g_SkipOpenTF2Check;
}
#endif

namespace
{
	struct HandleDeleter
	{
		void operator()(HANDLE handle) const
		{
			if (!CloseHandle(handle))
				LogError("Failed to close Win32 HANDLE");
		}
	};

	using SafeHandle = std::unique_ptr<std::remove_pointer_t<HANDLE>, HandleDeleter>;
}
bool tf2_bot_detector::Processes::IsTF2Running()
{
	return !!FindWindowA("Valve001", nullptr);
}

namespace
{
	class BaseQuerySink : public IWbemObjectSink
	{
	public:
		ULONG STDMETHODCALLTYPE AddRef() override { return ++m_RefCount; }
		ULONG STDMETHODCALLTYPE Release() override
		{
			auto newVal = --m_RefCount;
			if (newVal == 0)
				delete this;

			return newVal;
		}
		bool IsDone() const { return m_Done; }

		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
		{
			IUNKNOWN_QI_TYPE(IUnknown);
			IUNKNOWN_QI_TYPE(IWbemObjectSink);

			return E_NOINTERFACE;
		}

		HRESULT STDMETHODCALLTYPE SetStatus(LONG lFlags, HRESULT hResult, BSTR strParam,
			IWbemClassObject __RPC_FAR* pObjParam) override
		{
			if (lFlags == WBEM_STATUS_COMPLETE)
			{
				//DebugLog(""s << __FUNCTION__ << "(): Call completed");
				assert(!m_Done);
				if (bool expected = false; m_Done.compare_exchange_strong(expected, true))
					OnComplete();
			}
			else if (lFlags == WBEM_STATUS_PROGRESS)
			{
				//DebugLog(""s << __FUNCTION__ << "(): Call in progress");
			}

			return WBEM_S_NO_ERROR;
		}

	protected:
		virtual void OnComplete() = 0;

	private:
		std::atomic<bool> m_Done;
		std::atomic<ULONG> m_RefCount;
	};

	class CommandLineArgsQuerySink final : public BaseQuerySink
	{
	public:
		HRESULT STDMETHODCALLTYPE Indicate(LONG lObjectCount,
			IWbemClassObject __RPC_FAR* __RPC_FAR* apObjArray) override
		{
			try
			{
				for (LONG i = 0; i < lObjectCount; i++)
				{
					VARIANT name, cmdLine;
					CHECK_HR(apObjArray[i]->Get(_bstr_t(L"Name"), 0, &name, nullptr, nullptr));
					CHECK_HR(apObjArray[i]->Get(_bstr_t(L"CommandLine"), 0, &cmdLine, nullptr, nullptr));

					std::lock_guard lock(m_ArgsMutex);
					m_Args.push_back(ToMB(cmdLine.bstrVal ? cmdLine.bstrVal : L""));
					//DebugLog("Name: "s << std::wstring_view(name.bstrVal) << ", Command Line: " << m_Args.back());
				}
			}
			catch (const GetLastErrorException& e)
			{
				LogException(e);
				m_Promise.set_exception(std::current_exception());
				return WBEM_E_FAILED;
			}

			return WBEM_S_NO_ERROR;
		}

		mh::task<std::vector<std::string>> GetTask() const
		{
			return m_Promise.get_task();
		}

	protected:
		void OnComplete() override
		{
			m_Promise.set_value(std::move(m_Args));
		}

	private:
		mutable std::mutex m_ArgsMutex;
		std::vector<std::string> m_Args;

		mh::promise<std::vector<std::string>> m_Promise;
	};
}

// https://docs.microsoft.com/en-us/windows/win32/wmisdk/example--getting-wmi-data-from-the-local-computer
mh::task<std::vector<std::string>> tf2_bot_detector::Processes::GetTF2CommandLineArgsAsync()
{
	try
	{
		CHECK_HR(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

		ComPtr<IWbemLocator> pLoc;
		CHECK_HR(CoCreateInstance(
			CLSID_WbemLocator,
			0,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(pLoc.ReleaseAndGetAddressOf())));

		ComPtr<IWbemServices> pSvc;
		CHECK_HR(pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"),
			NULL,
			NULL,
			0,
			NULL,
			0,
			0,
			pSvc.ReleaseAndGetAddressOf()));

		CHECK_HR(CoSetProxyBlanket(pSvc.Get(),  // Indicates the proxy to set
			RPC_C_AUTHN_WINNT,                  // RPC_C_AUTHN_xxx
			RPC_C_AUTHZ_NONE,                   // RPC_C_AUTHZ_xxx
			NULL,                               // Server principal name
			RPC_C_AUTHN_LEVEL_CALL,             // RPC_C_AUTHN_LEVEL_xxx
			RPC_C_IMP_LEVEL_IMPERSONATE,        // RPC_C_IMP_LEVEL_xxx
			NULL,                               // client identity
			EOAC_NONE));                        // proxy capabilities

		ComPtr<CommandLineArgsQuerySink> pResponseSink(new CommandLineArgsQuerySink());

		const bstr_t query(
			"SELECT Name,CommandLine,CreationDate FROM Win32_Process "
			"WHERE Name = \"hl2.exe\" "
			//"ORDER BY CreationDate DESC "
			//"LIMIT 1"
		);

		CHECK_HR(pSvc->ExecQueryAsync(bstr_t("WQL"),
			query,
			WBEM_FLAG_BIDIRECTIONAL,
			NULL,
			pResponseSink.Get()));

		return pResponseSink->GetTask();
	}
	catch (const GetLastErrorException& e)
	{
		LogException(e);
		throw;
	}
}

bool tf2_bot_detector::Processes::IsSteamRunning()
{
	static mh::cached_variable m_CachedValue(std::chrono::seconds(1), []() { return IsProcessRunning("Steam.exe"); });
	return m_CachedValue.get();
}

bool tf2_bot_detector::Processes::IsProcessRunning(const std::string_view& processName)
{
	const SafeHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));

	PROCESSENTRY32 entry{};
	entry.dwSize = sizeof(entry);

	if (!Process32First(snapshot.get(), &entry))
	{
		auto error = GetLastErrorCode();
		LogError("Failed to enumerate processes: {}", error);
		return false;
	}

	do
	{
		if (mh::case_insensitive_compare(std::string_view(entry.szExeFile), processName))
			return true;

	} while (Process32Next(snapshot.get(), &entry));

	return false;
}

void tf2_bot_detector::Processes::RequireTF2NotRunning()
{
	if (!IsTF2Running())
		return;

#ifdef _DEBUG
	if (g_SkipOpenTF2Check)
	{
		LogWarning("TF2 was found running, but --allow-open-tf2 was on the command line. Letting execution proceed.");
	}
	else
#endif
	{
		MessageBoxA(nullptr, "TF2 Bot Detector must be started before Team Fortress 2.",
			"TF2 Currently Open", MB_OK | MB_ICONEXCLAMATION);

		std::exit(1);
	}
}

void tf2_bot_detector::Processes::Launch(const std::filesystem::path& executable,
	const std::vector<std::string>& args, bool elevated)
{
	std::string cmdLine;

	for (const auto& arg : args)
		cmdLine << std::quoted(arg) << ' ';

	return Launch(executable, cmdLine, elevated);
}

void tf2_bot_detector::Processes::Launch(const std::filesystem::path& executable,
	const std::string_view& args, bool elevated)
{
	DebugLog("ShellExecute({}, {}) (elevated = {})", executable, args, elevated);

	const auto cmdLineWide = mh::change_encoding<wchar_t>(args);

	const auto result = ShellExecuteW(
		NULL,
		elevated ? L"runas" : L"open",
		executable.c_str(),
		cmdLineWide.c_str(),
		nullptr,
		SW_SHOWDEFAULT);

	if (reinterpret_cast<intptr_t>(result) <= 32)
	{
		std::string errorCode;
		switch (reinterpret_cast<intptr_t>(result))
		{
		case 0:                       errorCode = "<out of resources>"; break;
		case ERROR_FILE_NOT_FOUND:    errorCode = "ERROR_FILE_NOT_FOUND"; break;
		case ERROR_PATH_NOT_FOUND:    errorCode = "ERROR_PATH_NOT_FOUND"; break;
		case ERROR_BAD_FORMAT:        errorCode = "ERROR_BAD_FORMAT"; break;
		case SE_ERR_ACCESSDENIED:     errorCode = "SE_ERR_ACCESSDENIED"; break;
		case SE_ERR_ASSOCINCOMPLETE:  errorCode = "SE_ERR_ASSOCINCOMPLETE"; break;
		case SE_ERR_DDEBUSY:          errorCode = "SE_ERR_DDEBUSY"; break;
		case SE_ERR_DDEFAIL:          errorCode = "SE_ERR_DDEFAIL"; break;
		case SE_ERR_DDETIMEOUT:       errorCode = "SE_ERR_DDETIMEOUT"; break;
		case SE_ERR_DLLNOTFOUND:      errorCode = "SE_ERR_DLLNOTFOUND"; break;
		//case SE_ERR_FNF:              errorCode = "SE_ERR_FNF"; break;
		case SE_ERR_NOASSOC:          errorCode = "SE_ERR_NOASSOC"; break;
		case SE_ERR_OOM:              errorCode = "SE_ERR_OOM"; break;
		//case SE_ERR_PNF:              errorCode = "SE_ERR_PNF"; break;
		case SE_ERR_SHARE:            errorCode = "SE_ERR_SHARE"; break;
		default:
			errorCode << reinterpret_cast<intptr_t>(result);
			break;
		}
		auto exception = std::runtime_error(mh::format("ShellExecuteW returned {}", errorCode));

		LogException(MH_SOURCE_LOCATION_CURRENT(), exception);
		throw exception;
	}
}

int tf2_bot_detector::Processes::GetCurrentProcessID()
{
	return ::GetCurrentProcessId();
}

size_t tf2_bot_detector::Processes::GetCurrentRAMUsage()
{
	PROCESS_MEMORY_COUNTERS counters{};
	counters.cb = sizeof(counters);
	mh_ensure(GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)));
	return counters.WorkingSetSize;
}
