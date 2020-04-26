#pragma once

#include <cstdint>
#include <string>

namespace tf2_bot_detector
{
	enum class LobbyMemberTeam : uint8_t
	{
		Invaders,
		Defenders,
	};

	enum class LobbyMemberType : uint8_t
	{
		Player
	};

	struct LobbyMember
	{
		std::string m_SteamID;
		std::string m_Name;
		unsigned m_Index;
		LobbyMemberTeam m_Team;
		LobbyMemberType m_Type;
	};
}
