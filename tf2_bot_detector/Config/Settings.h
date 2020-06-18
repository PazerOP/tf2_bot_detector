#pragma once

#include "SteamID.h"

#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <optional>
#include <vector>

namespace tf2_bot_detector
{
	enum class ProgramUpdateCheckMode
	{
		Unknown = -1,

		Releases,
		Previews,
		Disabled,
	};

	void to_json(nlohmann::json& j, const ProgramUpdateCheckMode& d);
	void from_json(const nlohmann::json& j, ProgramUpdateCheckMode& d);

	class Settings final
	{
	public:
		Settings();

		bool LoadFile();
		bool SaveFile() const;

		// Settings that are not saved in the config file because we want them to
		// reset to these defaults when the tool is reopened
		struct
		{
			bool m_Muted = false;
			bool m_EnableVotekick = true;
			bool m_EnableAutoMark = true;
			bool m_DebugShowCommands = false;
		} m_Unsaved;

		SteamID m_LocalSteamID;
		bool m_SleepWhenUnfocused = true;
		bool m_AutoTempMute = true;
		std::filesystem::path m_TFDir;

		float m_CommandTimeoutSeconds = 5;

		std::optional<bool> m_AllowInternetUsage;
		ProgramUpdateCheckMode m_ProgramUpdateCheckMode = ProgramUpdateCheckMode::Unknown;

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

template<typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, tf2_bot_detector::ProgramUpdateCheckMode val)
{
	using ProgramUpdateCheckMode = tf2_bot_detector::ProgramUpdateCheckMode;

#undef OS_CASE
#define OS_CASE(v) case v : return os << #v
	switch (val)
	{
		OS_CASE(ProgramUpdateCheckMode::Disabled);
		OS_CASE(ProgramUpdateCheckMode::Previews);
		OS_CASE(ProgramUpdateCheckMode::Releases);
		OS_CASE(ProgramUpdateCheckMode::Unknown);

	default:
		return os << "ProgramUpdateCheckMode(" << +std::underlying_type_t<ProgramUpdateCheckMode>(val) << ')';
	}
#undef OS_CASE
}
