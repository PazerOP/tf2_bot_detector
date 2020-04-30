#pragma once

#include "SteamID.h"
#include "TFConstants.h"

#include <cstdint>
#include <string>

namespace tf2_bot_detector
{
	struct PlayerStatus
	{
		std::string m_Name;
		std::string m_State;
		std::string m_Address;
		SteamID m_SteamID;

		uint32_t m_ConnectedTime; // in seconds
		UserID_t m_UserID;
		uint16_t m_Ping;
		uint8_t m_Loss;
	};

	struct PlayerStatusShort
	{
		std::string m_Name;
		uint8_t m_ClientIndex;
	};
}
