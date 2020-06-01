#pragma once

#include "SteamID.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace tf2_bot_detector
{
	class Settings final
	{
	public:
		Settings();

		bool LoadFile();
		bool SaveFile() const;

		bool m_Muted = false;
		bool m_DebugShowCommands = false;

		SteamID m_LocalSteamID;
		bool m_SleepWhenUnfocused = true;
		std::filesystem::path m_TFDir;

		struct Theme
		{
			struct Colors
			{
				float m_ScoreboardCheater[4] = { 1, 0, 1, 1 };
				float m_ScoreboardSuspicious[4] = { 1, 1, 0, 1 };
				float m_ScoreboardExploiter[4] = { 0, 1, 1, 1 };
				float m_ScoreboardRacist[4] = { 1, 1, 1, 1 };
				float m_ScoreboardYou[4] = { 0, 1, 0, 1 };
				float m_ScoreboardConnecting[4] = { 1, 1, 0, 0.5f };

				float m_FriendlyTeam[4] = { 0.19704340398311615f, 0.5180000066757202f, 0.25745877623558044f, 0.5f };
				float m_EnemyTeam[4] = { 0.8270000219345093f, 0.42039787769317627f, 0.38951700925827026f, 0.5f };
			} m_Colors;

		} m_Theme;
	};
}
