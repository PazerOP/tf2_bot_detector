#pragma once

#include <mh/source_location.hpp>

namespace tf2_bot_detector::Platform
{
	void* GetProcAddressHelper(const char* moduleName, const char* symbolName, bool isCritical = false, MH_SOURCE_LOCATION_AUTO(location));
}
