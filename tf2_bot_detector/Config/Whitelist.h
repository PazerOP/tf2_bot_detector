#pragma once

#include "SteamID.h"

#include <filesystem>
#include <map>
#include <string>

namespace tf2_bot_detector
{
	class PlayerWhitelist final
	{
	public:
		void LoadFiles();

		struct Player
		{
			std::string m_Nickname;
		};

		auto begin() const { return m_Whitelist.begin(); }
		auto end() const { return m_Whitelist.end(); }

		bool HasPlayer(const SteamID& id) const;

	private:
		bool LoadFile(const std::filesystem::path& filename);

		std::map<SteamID, Player> m_Whitelist;
	};
}
