#pragma once

#include "Clock.h"
#include "SteamID.h"
#include "TFConstants.h"

#include <cstdint>
#include <string>

namespace tf2_bot_detector
{
	enum class PlayerStatusState : uint8_t
	{
		Invalid,

		Challenging,
		Connecting,
		Spawning,
		Active,
	};

	struct PlayerStatus
	{
		std::string m_Name;
		std::string m_Address;
		SteamID m_SteamID;

		time_point_t m_ConnectionTime{};
		UserID_t m_UserID;
		uint16_t m_Ping;
		uint8_t m_Loss;
		PlayerStatusState m_State = PlayerStatusState::Invalid;
	};

	struct PlayerStatusShort
	{
		std::string m_Name;
		uint8_t m_ClientIndex;
	};
}
