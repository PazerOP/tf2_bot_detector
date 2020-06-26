#include "NetworkHelpers.h"
#include "Log.h"

#include <mh/text/string_insertion.hpp>

#include <stdexcept>

#ifdef _WIN32
#include <WinSock2.h>
#else
#include <unistd.h>
#endif

std::string tf2_bot_detector::Networking::GetLocalIP()
{
	// Same basic logic that the source engine uses to figure out what ip to bind to
	// (attempting to connect to rcon at 127.0.0.1 doesn't work)
	char buf[512];
	if (auto result = gethostname(buf, sizeof(buf)); result < 0)
		throw std::runtime_error(std::string(__FUNCTION__) << "(): gethostname() returned " << result);

	buf[sizeof(buf) - 1] = 0;

	const char* tempBuf = "localhost";
	auto host = gethostbyname(tempBuf);// buf);
	if (!host)
		throw std::runtime_error(std::string(__FUNCTION__) << "(): gethostbyname returned nullptr");

	in_addr addr{};
	addr.s_addr = *(u_long*)host->h_addr_list[0];

	std::string retVal;
	if (auto addrStr = inet_ntoa(addr); addrStr && addrStr[0])
		retVal = addrStr;
	else
		throw std::runtime_error(std::string(__FUNCTION__) << "(): inet_ntoa returned null or empty string");

	Log(std::string(__FUNCTION__) << "(): ip = " << retVal);
	return retVal;
}
