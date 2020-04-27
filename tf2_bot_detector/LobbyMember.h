#pragma once

#include "SteamID.h"

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

	/// <summary>
	/// Represents a player managed by the GC, as reported by tf_lobby_debug.
	/// </summary>
	struct LobbyMember
	{
		SteamID m_SteamID;
		unsigned m_Index;
		LobbyMemberTeam m_Team;
		LobbyMemberType m_Type;
		bool m_Pending;
	};
}
