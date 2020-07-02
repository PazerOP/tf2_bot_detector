#include "../Platform.h"
#include "Log.h"
#include "TextUtils.h"

#include <mh/text/insertion_conversion.hpp>
#include <mh/text/string_insertion.hpp>

#include <atomic>
#include <cassert>
#include <chrono>
#include <exception>
#include <mutex>
#include <thread>

#include <Windows.h>
#include "WindowsHelpers.h"
#include <comutil.h>
#include <Wbemidl.h>
#include <TlHelp32.h>
#include <wrl/client.h>

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
				LogError(""s << __FUNCTION__ << "(): " << e.what());
				return WBEM_E_FAILED;
			}

			return WBEM_S_NO_ERROR;
		}

		const auto& GetFuture() const { return m_Future; }

	protected:
		void OnComplete() override
		{
			m_Promise.set_value(std::move(m_Args));
		}

	private:
		mutable std::mutex m_ArgsMutex;
		std::vector<std::string> m_Args;

		std::promise<std::vector<std::string>> m_Promise;
		std::shared_future<std::vector<std::string>> m_Future{ m_Promise.get_future().share() };
	};
}

// https://docs.microsoft.com/en-us/windows/win32/wmisdk/example--getting-wmi-data-from-the-local-computer
std::shared_future<std::vector<std::string>> tf2_bot_detector::Processes::GetTF2CommandLineArgsAsync()
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

		return pResponseSink->GetFuture();
	}
	catch (const GetLastErrorException& e)
	{
		LogError(std::string(__FUNCTION__) << "(): " << e.what());
		throw;
	}
}

bool tf2_bot_detector::Processes::IsSteamRunning()
{
	const SafeHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));

	PROCESSENTRY32 entry;
	if (!Process32First(snapshot.get(), &entry))
	{
		auto error = GetLastErrorCode();
		LogError(std::string(__FUNCTION__) << "(): Failed to enumerate processes: " << error.message());
		return false;
	}

	do
	{
		if (!strcmp(entry.szExeFile, "Steam.exe"))
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
