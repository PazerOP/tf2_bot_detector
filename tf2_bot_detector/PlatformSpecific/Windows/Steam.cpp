#include "PlatformSpecific/Steam.h"
#include "Log.h"
#include "TextUtils.h"

#include <mh/text/string_insertion.hpp>

#include <Windows.h>

using namespace std::string_literals;
using namespace tf2_bot_detector;

SteamID tf2_bot_detector::GetCurrentActiveSteamID()
{
	constexpr char STEAM_ACTIVE_PROCESS_KEY[] = "Software\\Valve\\Steam\\ActiveProcess";

	DWORD type;
	DWORD data;
	DWORD dataSize = sizeof(data);
	auto result = std::error_code(RegGetValueA(
		HKEY_CURRENT_USER,
		STEAM_ACTIVE_PROCESS_KEY, "ActiveUser",
		RRF_RT_DWORD, &type, &data, &dataSize), std::system_category());

	if (result)
	{
		LogError(std::string(__FUNCTION__) << ": Failed to retrieve "s
			<< STEAM_ACTIVE_PROCESS_KEY << "\\ActiveUser: error " << result << ' ' << std::quoted(result.message()));
		return {};
	}

	char universeStr[256];
	dataSize = sizeof(universeStr);
	result = std::error_code(RegGetValueA(
		HKEY_CURRENT_USER,
		STEAM_ACTIVE_PROCESS_KEY, "Universe",
		RRF_RT_REG_SZ, &type, universeStr, &dataSize), std::system_category());

	if (result)
	{
		LogError(std::string(__FUNCTION__) << ": Failed to retrieve "s
			<< STEAM_ACTIVE_PROCESS_KEY << "\\Universe: error " << result << ' ' << std::quoted(result.message()));
		return {};
	}

	SteamAccountUniverse universe;
	if (_strnicmp(universeStr, "Public", dataSize) == 0)
		universe = SteamAccountUniverse::Public;
	else if (_strnicmp(universeStr, "Beta", dataSize) == 0)
		universe = SteamAccountUniverse::Beta;
	else if (_strnicmp(universeStr, "Internal", dataSize) == 0)
		universe = SteamAccountUniverse::Internal;
	else if (_strnicmp(universeStr, "Dev", dataSize) == 0)
		universe = SteamAccountUniverse::Dev;
	else
	{
		LogError(std::string(__FUNCTION__) << ": Unknown steam account universe "
			<< std::quoted(universeStr));
		return {};
	}

	return SteamID(data, SteamAccountType::Individual, universe);
}
