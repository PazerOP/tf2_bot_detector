#include "Platform/PlatformCommon.h"

#include <mh/text/format.hpp>

#include <stdexcept>

#include <Windows.h>

void* tf2_bot_detector::Platform::GetProcAddressHelper(const char* moduleName, const char* symbolName,
	bool isCritical, const mh::source_location& location)
{
	if (!moduleName)
		throw std::invalid_argument("moduleName was nullptr");
	if (!moduleName[0])
		throw std::invalid_argument("moduleName was empty");
	if (!symbolName)
		throw std::invalid_argument("symbolName was nullptr");
	if (!symbolName[0])
		throw std::invalid_argument("symbolName was empty");

	HMODULE moduleHandle = GetModuleHandleA(moduleName);
	if (!moduleHandle)
	{
		auto err = GetLastError();
		throw std::system_error(err, std::system_category(), mh::format(MH_FMT_STRING("Failed to GetModuleHandle({})"), moduleName));
	}

	auto address = GetProcAddress(moduleHandle, symbolName);
	if (!address)
	{
		auto err = GetLastError();
		auto ec = std::error_code(err, std::system_category());

		if (isCritical)
		{
			throw std::system_error(ec, mh::format(MH_FMT_STRING("{}: Failed to find function {} in {}"), location, symbolName, moduleName));
		}
	}

	return address;
}
