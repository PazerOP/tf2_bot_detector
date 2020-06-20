#pragma once

#include "SteamID.h"

#include <filesystem>

namespace tf2_bot_detector
{
	std::filesystem::path GetCurrentSteamDir();
	SteamID GetCurrentActiveSteamID();
}
