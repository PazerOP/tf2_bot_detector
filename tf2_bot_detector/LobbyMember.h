#pragma once

#include "SteamID.h"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace tf2_bot_detector
{
	enum class LobbyMemberTeam : uint8_t
	{
		Invaders,
		Defenders,
	};

	inline constexpr LobbyMemberTeam OppositeTeam(LobbyMemberTeam team)
	{
		if (team == LobbyMemberTeam::Invaders)
			return LobbyMemberTeam::Defenders;
		else if (team == LobbyMemberTeam::Defenders)
			return LobbyMemberTeam::Invaders;
		else
			throw std::runtime_error(__FUNCTION__ ": Invalid LobbyMemberTeam");
	}

	enum class LobbyMemberType : uint8_t
	{
		Player,
		InvalidPlayer,
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
