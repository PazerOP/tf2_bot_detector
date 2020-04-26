#pragma once

#include "SteamID.h"

#include <cstdint>
#include <string>

namespace tf2_bot_detector
{
	struct PlayerStatus
	{
		std::string m_Name;
		SteamID m_SteamID;

		uint16_t m_UserID;
		uint32_t m_ConnectedTime; // in seconds
		uint16_t m_Ping;
		uint8_t m_Loss;
	};
}
