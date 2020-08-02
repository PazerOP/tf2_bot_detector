#pragma once

#include "SteamID.h"

namespace tf2_bot_detector
{
	enum class TFPartyID : uint64_t;
	struct TFParty final
	{
		TFPartyID m_PartyID{};
		SteamID m_LeaderID{};
		uint8_t m_MemberCount{};
	};
}
