#include "Settings.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

using namespace tf2_bot_detector;

static std::filesystem::path s_SettingsPath("cfg/settings.json");

namespace tf2_bot_detector
{
	void to_json(nlohmann::json& j, const Settings::Theme::Colors& d)
	{
		j =
		{
			{ "scoreboard_cheater", d.m_ScoreboardCheater },
			{ "scoreboard_suspicious", d.m_ScoreboardSuspicious },
			{ "scoreboard_exploiter", d.m_ScoreboardExploiter },
			{ "scoreboard_racism", d.m_ScoreboardRacist },
		};
	}
	void to_json(nlohmann::json& j, const Settings::Theme& d)
	{
		j =
		{
			{ "colors", d.m_Colors }
		};
	}

	void from_json(const nlohmann::json& j, Settings::Theme::Colors& d)
	{
		j.at("scoreboard_cheater").get_to(d.m_ScoreboardCheater);
		j.at("scoreboard_suspicious").get_to(d.m_ScoreboardSuspicious);
		j.at("scoreboard_exploiter").get_to(d.m_ScoreboardExploiter);
		j.at("scoreboard_racism").get_to(d.m_ScoreboardRacist);
	}
	void from_json(const nlohmann::json& j, Settings::Theme& d)
	{
		d.m_Colors = j.at("colors");
	}
}

Settings::Settings()
{
	// Immediately load and resave to normalize any formatting
	LoadFile();
	SaveFile();
}

void Settings::LoadFile()
{
	nlohmann::json json;
	{
		std::ifstream file(s_SettingsPath);
		file >> json;
	}

	m_LocalSteamID = json.at("local_steamid");
	m_SleepWhenUnfocused = json.at("sleep_when_unfocused");
	m_Theme = json.at("theme");
}

void Settings::SaveFile() const
{
	nlohmann::json json =
	{
		{ "$schema", "./schema/settings.schema.json" },
		{ "theme", m_Theme },
		{ "local_steamid", m_LocalSteamID },
		{ "sleep_when_unfocused", m_SleepWhenUnfocused },
	};;

	// Make sure we successfully serialize BEFORE we destroy our file
	auto jsonString = json.dump(1, '\t', true);
	{
		std::ofstream file(s_SettingsPath);
		file << jsonString << '\n';
	}
}
