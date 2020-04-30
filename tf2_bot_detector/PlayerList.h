#pragma once

#include "SteamID.h"

#include <filesystem>
#include <set>

namespace tf2_bot_detector
{
	class PlayerList
	{
	public:
		PlayerList(std::filesystem::path filename);

		void LoadFile();
		void SaveFile();

		// Returns true if the include state was changed
		bool IncludePlayer(const SteamID& id, bool included = true, bool saveFile = true);
		bool IsPlayerIncluded(const SteamID& id) const;

	private:
		std::set<SteamID> m_Players;
		std::filesystem::path m_Filename;
	};
}
