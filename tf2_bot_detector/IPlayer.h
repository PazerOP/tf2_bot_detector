#pragma once

#include "SteamID.h"

namespace tf2_bot_detector
{
	class IPlayer
	{
	public:
		virtual ~IPlayer() = default;

		virtual SteamID GetSteamID() const = 0;
	};
}
