#include "Settings.h"
#include "IPlayer.h"
#include "JSONHelpers.h"
#include "Log.h"
#include "PathUtils.h"
#include "PlayerListJSON.h"
#include "Platform/Platform.h"
#include "Networking/NetworkHelpers.h"

#include <mh/text/case_insensitive_string.hpp>
#include <mh/text/string_insertion.hpp>
#include <mh/text/stringops.hpp>
#include <nlohmann/json.hpp>
#include <srcon/async_client.h>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <regex>

using namespace tf2_bot_detector;
using namespace std::string_literals;
using namespace std::string_view_literals;

static const std::filesystem::path s_SettingsPath("cfg/settings.json");

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
			{ "scoreboard_you", d.m_ScoreboardYou },
			{ "scoreboard_connecting", d.m_ScoreboardConnecting },
			{ "friendly_team", d.m_FriendlyTeam },
			{ "enemy_team", d.m_EnemyTeam },
		};
	}

	void to_json(nlohmann::json& j, const Settings::Theme& d)
	{
		j =
		{
			{ "colors", d.m_Colors }
		};
	}

	void to_json(nlohmann::json& j, const GotoProfileSite& d)
	{
		j =
		{
			{ "name", d.m_Name },
			{ "profile_url", d.m_ProfileURL },
		};
	}

	void from_json(const nlohmann::json& j, Settings::Theme::Colors& d)
	{
		try_get_to(j, "scoreboard_cheater", d.m_ScoreboardCheater);
		try_get_to(j, "scoreboard_suspicious", d.m_ScoreboardSuspicious);
		try_get_to(j, "scoreboard_exploiter", d.m_ScoreboardExploiter);
		try_get_to(j, "scoreboard_racism", d.m_ScoreboardRacist);
		try_get_to(j, "scoreboard_you", d.m_ScoreboardYou);
		try_get_to(j, "scoreboard_connecting", d.m_ScoreboardConnecting);
		try_get_to(j, "friendly_team", d.m_FriendlyTeam);
		try_get_to(j, "enemy_team", d.m_EnemyTeam);
	}

	void from_json(const nlohmann::json& j, Settings::Theme& d)
	{
		d.m_Colors = j.at("colors");
	}

	void from_json(const nlohmann::json& j, GotoProfileSite& d)
	{
		d.m_Name = j.at("name");
		d.m_ProfileURL = j.at("profile_url");
	}
}

void tf2_bot_detector::to_json(nlohmann::json& j, const ProgramUpdateCheckMode& d)
{
	switch (d)
	{
	case ProgramUpdateCheckMode::Unknown:   j = nullptr; return;
	case ProgramUpdateCheckMode::Releases:  j = "releases"; return;
	case ProgramUpdateCheckMode::Previews:  j = "previews"; return;
	case ProgramUpdateCheckMode::Disabled:  j = "disabled"; return;

	default:
		throw std::invalid_argument("Unknown ProgramUpdateCheckMode "s << +std::underlying_type_t<ProgramUpdateCheckMode>(d));
	}
}

void tf2_bot_detector::from_json(const nlohmann::json& j, ProgramUpdateCheckMode& d)
{
	if (j.is_null())
	{
		d = ProgramUpdateCheckMode::Unknown;
		return;
	}

	auto value = j.get<std::string_view>();
	if (value == "releases"sv)
		d = ProgramUpdateCheckMode::Releases;
	else if (value == "previews"sv)
		d = ProgramUpdateCheckMode::Previews;
	else if (value == "disabled"sv)
		d = ProgramUpdateCheckMode::Disabled;
	else
		throw std::invalid_argument("Unknown ProgramUpdateCheckMode "s << std::quoted(value));
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
		if (!file.good())
		{
			LogError(std::string(__FUNCTION__ ": Failed to open ") << s_SettingsPath);
		}
		else
		{
			try
			{
				file >> json;
			}
			catch (const nlohmann::json::exception& e)
			{
				auto backupPath = std::filesystem::path(s_SettingsPath).replace_filename("settings.backup.json");
				LogError(std::string(__FUNCTION__) << ": Failed to parse JSON from " << s_SettingsPath << ": " << e.what()
					<< ". Writing backup to ");

				std::error_code ec;
				std::filesystem::copy_file(s_SettingsPath, backupPath, ec);
				if (!ec)
					LogError(std::string(__FUNCTION__) << ": Failed to make backup of settings.json to " << backupPath);
			}
		}
	}

	if (auto found = json.find("general"); found != json.end())
	{
		try_get_to(*found, "local_steamid_override", m_LocalSteamIDOverride);
		try_get_to(*found, "sleep_when_unfocused", m_SleepWhenUnfocused);
		try_get_to(*found, "auto_temp_mute", m_AutoTempMute);
		try_get_to(*found, "allow_internet_usage", m_AllowInternetUsage);
		try_get_to(*found, "program_update_check_mode", m_ProgramUpdateCheckMode);
		try_get_to(*found, "steam_api_key", m_SteamAPIKey);
		try_get_to(*found, "auto_launch_tf2", m_AutoLaunchTF2);
		try_get_to(*found, "auto_chat_warnings", m_AutoChatWarnings);
		try_get_to(*found, "auto_chat_warnings_connecting", m_AutoChatWarningsConnecting);
		try_get_to(*found, "auto_votekick", m_AutoVotekick);
		try_get_to(*found, "auto_mark", m_AutoMark);

		if (auto foundDir = found->find("steam_dir_override"); foundDir != found->end())
			m_SteamDirOverride = foundDir->get<std::string_view>();
		if (auto foundDir = found->find("tf_game_dir_override"); foundDir != found->end())
			m_TFDirOverride = foundDir->get<std::string_view>();
	}

	try_get_to(json, "theme", m_Theme);
	if (!try_get_to(json, "goto_profile_sites", m_GotoProfileSites))
	{
		// Some defaults
		m_GotoProfileSites.push_back({ "Steam Community", "https://steamcommunity.com/profiles/%SteamID64%" });
		m_GotoProfileSites.push_back({ "logs.tf", "https://logs.tf/profile/%SteamID64%" });
		m_GotoProfileSites.push_back({ "RGL", "https://rgl.gg/Public/PlayerProfile.aspx?p=%SteamID64%" });
		m_GotoProfileSites.push_back({ "SteamRep", "https://steamrep.com/profiles/%SteamID64%" });
		m_GotoProfileSites.push_back({ "UGC League", "https://www.ugcleague.com/players_page.cfm?player_id=%SteamID64%" });
	}
}

bool Settings::SaveFile() const
{
	nlohmann::json json =
	{
		{ "$schema", "https://raw.githubusercontent.com/PazerOP/tf2_bot_detector/master/schemas/v3/settings.schema.json" },
		{ "theme", m_Theme },
		{ "general",
			{
				{ "sleep_when_unfocused", m_SleepWhenUnfocused },
				{ "auto_temp_mute", m_AutoTempMute },
				{ "program_update_check_mode", m_ProgramUpdateCheckMode },
				{ "steam_api_key", m_SteamAPIKey },
				{ "auto_launch_tf2", m_AutoLaunchTF2 },
				{ "auto_chat_warnings", m_AutoChatWarnings },
				{ "auto_chat_warnings_connecting", m_AutoChatWarningsConnecting },
				{ "auto_votekick", m_AutoVotekick },
				{ "auto_mark", m_AutoMark },
			}
		},
		{ "goto_profile_sites", m_GotoProfileSites },
	};

	if (!m_SteamDirOverride.empty())
		json["general"]["steam_dir_override"] = m_SteamDirOverride.string();
	if (!m_TFDirOverride.empty())
		json["general"]["tf_game_dir_override"] = m_TFDirOverride.string();
	if (m_LocalSteamIDOverride.IsValid())
		json["general"]["local_steamid_override"] = m_LocalSteamIDOverride;
	if (m_AllowInternetUsage)
		json["general"]["allow_internet_usage"] = *m_AllowInternetUsage;

	// Make sure we successfully serialize BEFORE we destroy our file
	auto jsonString = json.dump(1, '\t', true);
	{
		std::filesystem::create_directories(std::filesystem::path(s_SettingsPath).remove_filename());
		std::ofstream file(s_SettingsPath, std::ios::binary);
		if (!file.good())
		{
			LogError(std::string(__FUNCTION__ ": Failed to open settings file for writing: ") << s_SettingsPath);
			return false;
		}

		file << jsonString << '\n';
		if (!file.good())
		{
			LogError(std::string(__FUNCTION__ ": Failed to write settings to ") << s_SettingsPath);
			return false;
		}
	}

	return true;
}

Settings::Unsaved::~Unsaved()
{
}

const HTTPClient* tf2_bot_detector::Settings::GetHTTPClient() const
{
	if (!m_AllowInternetUsage.value_or(false))
		return nullptr;

	if (!m_HTTPClient)
		m_HTTPClient.emplace();

	return &*m_HTTPClient;
}

SteamID AutoDetectedSettings::GetLocalSteamID() const
{
	if (m_LocalSteamIDOverride.IsValid())
		return m_LocalSteamIDOverride;

	if (auto sid = GetCurrentActiveSteamID(); sid.IsValid())
		return sid;

	return {};
}

std::filesystem::path AutoDetectedSettings::GetTFDir() const
{
	if (!m_TFDirOverride.empty())
		return m_TFDirOverride;

	return FindTFDir(GetSteamDir());
}

std::string tf2_bot_detector::AutoDetectedSettings::GetLocalIP() const
{
	if (!m_LocalIPOverride.empty())
		return m_LocalIPOverride;

	return tf2_bot_detector::Networking::GetLocalIP();
}

std::filesystem::path AutoDetectedSettings::GetSteamDir() const
{
	if (!m_SteamDirOverride.empty())
		return m_SteamDirOverride;

	return GetCurrentSteamDir();
}

std::string GotoProfileSite::CreateProfileURL(const SteamID& id) const
{
	auto str = mh::find_and_replace(m_ProfileURL, "%SteamID64%"sv, std::string_view(std::to_string(id.ID64)));
	return mh::find_and_replace(std::move(str), "%SteamID3%"sv, std::string_view(id.str()));
}
