#pragma once

namespace tf2_bot_detector
{
	class SteamID;

	class CheaterList
	{
	public:
		void Add(const SteamID& id);
		bool IsCheater(const SteamID& id) const;
	};
}
