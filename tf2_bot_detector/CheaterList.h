#pragma once

#include "SteamID.h"

#include <filesystem>
#include <unordered_set>

namespace tf2_bot_detector
{
	class CheaterList
	{
	public:
		CheaterList() = default;
		CheaterList(const std::filesystem::path& filename);

		void LoadFile(const std::filesystem::path& filename);
		void SaveFile(const std::filesystem::path& filename);

		void SetIsCheater(const SteamID& id, bool isCheater = true);
		bool IsCheater(const SteamID& id) const;

	private:
		std::unordered_set<SteamID> m_Cheaters;
	};
}
