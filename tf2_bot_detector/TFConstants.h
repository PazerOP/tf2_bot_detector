#pragma once

#include <cstdint>

namespace tf2_bot_detector
{
	enum class TFTeam : uint8_t
	{
		Unknown,

		Spectator,
		Red,
		Blue,
	};

	using UserID_t = uint16_t;
}
