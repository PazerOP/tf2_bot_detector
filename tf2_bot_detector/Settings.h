#pragma once

#include "SteamID.h"

namespace tf2_bot_detector
{
	class Settings final
	{
	public:
		Settings();

		void LoadFile();
		void SaveFile() const;

		SteamID m_LocalSteamID;
		bool m_SleepWhenUnfocused = true;

		struct Theme
		{
			struct Colors
			{
				float m_ScoreboardCheater[4] = { 1, 0, 1, 1 };
				float m_ScoreboardSuspicious[4] = { 1, 1, 0, 1 };
				float m_ScoreboardExploiter[4] = { 0, 1, 1, 1 };
				float m_ScoreboardRacist[4] = { 1, 1, 1, 1 };
			} m_Colors;

		} m_Theme;
	};
}
