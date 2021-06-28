#include "Platform/Platform.h"
#include "Log.h"
#include "WindowsHelpers.h"

#include <mh/memory/unique_object.hpp>
#include <mh/text/fmtstr.hpp>

#include <Windows.h>
#include <winsock2.h>
#include <WS2tcpip.h>

using namespace tf2_bot_detector;

static std::error_code WSAGetLastErrorCode()
{
	return std::error_code(WSAGetLastError(), std::system_category());
}

static void EnsureWSAInit()
{
	struct WSAInit final
	{
		WSAInit()
		{
			if (auto result = WSAStartup(MAKEWORD(2, 2), &m_WSAData))
			{
				const auto error = WSAGetLastErrorCode();
				LogFatalError("Failed to initialize WSA: returned {}, error code {}", result, error);
			}
		}
		~WSAInit()
		{
			// No real point in calling WSACleanup, we're closing anyway, and don't want to potentially
			// cause problems for other libraries later on if we switch to static linking
		}

		const WSAData& GetWSAData() const { return m_WSAData; }

	private:
		WSADATA m_WSAData{};

	} static const s_WSAInit;
}

const std::error_code tf2_bot_detector::Platform::ErrorCodes::PRIVILEGE_NOT_HELD(ERROR_PRIVILEGE_NOT_HELD, std::system_category());

tf2_bot_detector::Platform::OS tf2_bot_detector::Platform::GetOS()
{
	return OS::Windows;
}

tf2_bot_detector::Platform::Arch tf2_bot_detector::Platform::GetArch()
{
	static const Platform::Arch s_Arch = []
	{
		SYSTEM_INFO sysInfo;
		GetNativeSystemInfo(&sysInfo);
		switch (sysInfo.wProcessorArchitecture)
		{
		default:
			LogError(MH_SOURCE_LOCATION_CURRENT(),
				"Failed to identify processor architecture: wProcessorArchitecture = {}",
				sysInfo.wProcessorArchitecture);

			[[fallthrough]];
		case PROCESSOR_ARCHITECTURE_INTEL:
			return Arch::x86;
		case PROCESSOR_ARCHITECTURE_AMD64:
			return Arch::x64;
		}
	}();

	return s_Arch;
}

bool tf2_bot_detector::Platform::IsDebuggerAttached()
{
	return ::IsDebuggerPresent();
}

bool tf2_bot_detector::Platform::IsPortAvailable(uint16_t port)
{
	EnsureWSAInit();

	const mh::fmtstr<32> portStr("{}", port);
	addrinfo hints{};
	hints.ai_family = AF_INET;
	//hints.ai_socktype = SOCK_STREAM
	hints.ai_protocol = IPPROTO_UDP;

	struct FreeAddrInfoHelper
	{
		void operator()(addrinfo* info) const
		{
			if (info)
				freeaddrinfo(info);
		}
	};

	std::unique_ptr<addrinfo, FreeAddrInfoHelper> addrResult;
	{
		addrinfo* addrResultRaw = nullptr;

		if (auto result = getaddrinfo("localhost", portStr.c_str(), &hints, &addrResultRaw); result != 0)
		{
			LogError("Failed to determine if port {} was available: getaddrinfo returned {}", port, result);
			return false;
		}

		addrResult.reset(addrResultRaw);
	}

	struct FreeSocketHelper
	{
		bool is_obj_valid(SOCKET s) { return !!s; }
		SOCKET release_obj(SOCKET& s) { return std::exchange(s, SOCKET{}); }
		void delete_obj(SOCKET s)
		{
			if (s)
			{
				if (auto result = closesocket(s))
				{
					const auto error = WSAGetLastErrorCode();
					LogError("Failed to close socket {}: closesocket() returned {}, error {}", s, result, error);
				}
			}
		}
	};
	mh::unique_object<SOCKET, FreeSocketHelper> listenSocket;
	listenSocket.reset(socket(addrResult->ai_family, addrResult->ai_socktype, addrResult->ai_protocol));

	if (auto result = bind(listenSocket.value(), addrResult->ai_addr, (int)addrResult->ai_addrlen); result != 0)
	{
		LogError("Failed to bind socket {}", listenSocket.value());
		return false;
	}

	return true;
}
