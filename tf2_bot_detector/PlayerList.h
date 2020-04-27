#pragma once

#include "SteamID.h"

#include <filesystem>
#include <unordered_set>

namespace tf2_bot_detector
{
	class PlayerList
	{
	public:
		PlayerList() = default;
		PlayerList(const std::filesystem::path& filename);

		void LoadFile(const std::filesystem::path& filename);
		void SaveFile(const std::filesystem::path& filename);

		void IncludePlayer(const SteamID& id, bool included = true);
		bool IsPlayerIncluded(const SteamID& id) const;

	private:
		std::unordered_set<SteamID> m_Players;
	};
}
