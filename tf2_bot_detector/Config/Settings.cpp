#include "Settings.h"
#include "Networking/HTTPClient.h"
#include "Networking/NetworkHelpers.h"
#include "Util/JSONUtils.h"
#include "Util/PathUtils.h"
#include "Filesystem.h"
#include "IPlayer.h"
#include "Log.h"
#include "PlayerListJSON.h"
#include "Platform/Platform.h"
#include "ReleaseChannel.h"

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

namespace tf2_bot_detector
{
	void to_json(nlohmann::json& j, const Settings::Theme::Colors& d)
	{
		j =
		{
			{ "scoreboard_marked_cheater.bg", d.m_ScoreboardCheaterBG },
			{ "scoreboard_marked_suspicious.bg", d.m_ScoreboardSuspiciousBG },
			{ "scoreboard_marked_exploiter.bg", d.m_ScoreboardExploiterBG },
			{ "scoreboard_marked_racism.bg", d.m_ScoreboardRacistBG },
			{ "scoreboard_you.fg", d.m_ScoreboardYouFG },
			{ "scoreboard_connecting.fg", d.m_ScoreboardConnectingFG },
			{ "scoreboard_team_friendly.bg", d.m_ScoreboardFriendlyTeamBG },
			{ "scoreboard_team_enemy.bg", d.m_ScoreboardEnemyTeamBG },

			{ "chat_log_you.fg", d.m_ChatLogYouFG },
			{ "chat_log_team_friendly.fg", d.m_ChatLogFriendlyTeamFG },
			{ "chat_log_team_enemy.fg", d.m_ChatLogEnemyTeamFG },
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
		static constexpr Settings::Theme::Colors DEFAULTS;

		try_get_to_defaulted(j, d.m_ScoreboardCheaterBG, { "scoreboard_marked_cheater.bg", "scoreboard_cheater" },
			DEFAULTS.m_ScoreboardCheaterBG);
		try_get_to_defaulted(j, d.m_ScoreboardSuspiciousBG, { "scoreboard_marked_suspicious.bg", "scoreboard_suspicious" },
			DEFAULTS.m_ScoreboardSuspiciousBG);
		try_get_to_defaulted(j, d.m_ScoreboardExploiterBG, { "scoreboard_marked_exploiter.bg", "scoreboard_exploiter" },
			DEFAULTS.m_ScoreboardExploiterBG);
		try_get_to_defaulted(j, d.m_ScoreboardRacistBG, { "scoreboard_marked_racism.bg", "scoreboard_racism" },
			DEFAULTS.m_ScoreboardRacistBG);
		try_get_to_defaulted(j, d.m_ScoreboardYouFG, { "scoreboard_you.fg", "scoreboard_you" },
			DEFAULTS.m_ScoreboardYouFG);
		try_get_to_defaulted(j, d.m_ScoreboardConnectingFG, { "scoreboard_connecting.fg", "scoreboard_connecting" },
			DEFAULTS.m_ScoreboardConnectingFG);
		try_get_to_defaulted(j, d.m_ScoreboardFriendlyTeamBG, { "scoreboard_team_friendly.bg", "friendly_team" },
			DEFAULTS.m_ScoreboardFriendlyTeamBG);
		try_get_to_defaulted(j, d.m_ScoreboardEnemyTeamBG, { "scoreboard_team_enemy.bg", "enemy_team" },
			DEFAULTS.m_ScoreboardEnemyTeamBG);

		try_get_to_defaulted(j, d.m_ChatLogYouFG, "chat_log_you.fg", DEFAULTS.m_ChatLogYouFG);
		try_get_to_defaulted(j, d.m_ChatLogFriendlyTeamFG, "chat_log_team_friendly.fg",
			DEFAULTS.m_ChatLogFriendlyTeamFG);
		try_get_to_defaulted(j, d.m_ChatLogEnemyTeamFG, "chat_log_team_enemy.fg",
			DEFAULTS.m_ChatLogEnemyTeamFG);
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

	void to_json(nlohmann::json& j, const Settings::Discord& d)
	{
		j =
		{
			{ "rich_presence_enable", d.m_EnableRichPresence },
		};
	}
	void from_json(const nlohmann::json& j, Settings::Discord& d)
	{
		static constexpr Settings::Discord DEFAULTS;
		try_get_to_defaulted(j, d.m_EnableRichPresence, "rich_presence_enable", DEFAULTS.m_EnableRichPresence);
	}

	void to_json(nlohmann::json& j, const Settings::Logging& d)
	{
		j =
		{
			{ "rcon_packets", d.m_RCONPackets },
			{ "discord_rich_presence", d.m_DiscordRichPresence },
		};
	}
	void from_json(const nlohmann::json& j, Settings::Logging& d)
	{
		static constexpr Settings::Logging DEFAULTS;

		try_get_to_defaulted(j, d.m_RCONPackets, "rcon_packets", DEFAULTS.m_RCONPackets);
		try_get_to_defaulted(j, d.m_DiscordRichPresence, "discord_rich_presence", DEFAULTS.m_DiscordRichPresence);
	}
}

void GeneralSettings::SetSteamAPIKey(std::string key)
{
	ILogManager::GetInstance().AddSecret(key, mh::format("<STEAM_API_KEY:{}>", key.size()));
	m_SteamAPIKey = std::move(key);
}

void tf2_bot_detector::to_json(nlohmann::json& j, const ReleaseChannel& d)
{
	switch (d)
	{
	case ReleaseChannel::None:      j = "disabled"; return;
	case ReleaseChannel::Public:    j = "public"; return;
	case ReleaseChannel::Preview:   j = "preview"; return;
	case ReleaseChannel::Nightly:   j = "nightly"; return;
	}

	throw std::invalid_argument(mh::format("Unexpected value {}", mh::enum_fmt(d)));
}

void tf2_bot_detector::from_json(const nlohmann::json& j, ReleaseChannel& d)
{
	auto value = mh::tolower(j.get<std::string_view>());
	if (value == "releases"sv || value == "public"sv)
		d = ReleaseChannel::Public;
	else if (value == "previews"sv || value == "preview"sv)
		d = ReleaseChannel::Preview;
	else if (value == "nightly"sv)
		d = ReleaseChannel::Nightly;
	else if (value == "disabled"sv || value == "none"sv)
		d = ReleaseChannel::None;
	else
		throw std::invalid_argument(mh::format("Unknown ReleaseChannel {}", std::quoted(value)));
}

Settings::Settings() try
{
	LoadFile();
}
catch (...)
{
	LogException(MH_SOURCE_LOCATION_CURRENT());
	throw;
}

Settings::~Settings() = default;

void Settings::LoadFile() try
{
#if 0
	nlohmann::json json;
	{
		const auto settingsPath = GetSettingsPath(PathUsage::Read);
		std::ifstream file(settingsPath);
		if (!file.good())
		{
			LogError(MH_SOURCE_LOCATION_CURRENT(), "Failed to open {}", settingsPath);
		}
		else
		{
			try
			{
				file >> json;
			}
			catch (const nlohmann::json::exception& e)
			{
				const auto backupPath = std::filesystem::path(settingsPath).replace_filename("settings.backup.json");
				LogException(MH_SOURCE_LOCATION_CURRENT(), e,
					"Failed to parse JSON from {}. Writing backup to {}...", settingsPath, backupPath);

				try
				{
					std::filesystem::copy_file(settingsPath, backupPath);
				}
				catch (...)
				{
					LogException(MH_SOURCE_LOCATION_CURRENT(),
						"Failed to make backup of settings.json to {}", backupPath);
				}
			}
		}
	}
#endif
	ConfigFileBase::LoadFileAsync("cfg/settings.json").get();
}
catch (...)
{
	LogException(MH_SOURCE_LOCATION_CURRENT());
	throw;
}

bool Settings::SaveFile() const try
{
	return !ConfigFileBase::SaveFile("cfg/settings.json");
}
catch (...)
{
	LogException(MH_SOURCE_LOCATION_CURRENT(), "Failed to save settings");
	return false;
}

Settings::Unsaved::~Unsaved()
{
}

std::shared_ptr<const HTTPClient> tf2_bot_detector::Settings::GetHTTPClient() const
{
	if (!m_AllowInternetUsage.value_or(false))
		return nullptr;

	if (!m_HTTPClient)
		m_HTTPClient = std::make_shared<HTTPClient>();

	return m_HTTPClient;
}

void Settings::ValidateSchema(const ConfigSchemaInfo& schema) const
{
	if (schema.m_Type != "settings")
		throw std::runtime_error(mh::format("Schema {} is not a settings file", schema.m_Type));
	if (schema.m_Version != 3)
		throw std::runtime_error(mh::format("Schema must be version {} (current version {})", SETTINGS_SCHEMA_VERSION, schema.m_Version));
}

void Settings::Deserialize(const nlohmann::json& json)
{
	if (auto found = json.find("general"); found != json.end())
	{
		static const GeneralSettings DEFAULTS;

		try_get_to_defaulted(*found, m_LocalSteamIDOverride, "local_steamid_override");
		try_get_to_defaulted(*found, m_SleepWhenUnfocused, "sleep_when_unfocused");
		try_get_to_defaulted(*found, m_AutoTempMute, "auto_temp_mute", DEFAULTS.m_AutoTempMute);
		try_get_to_defaulted(*found, m_AllowInternetUsage, "allow_internet_usage");
		try_get_to_defaulted(*found, m_ReleaseChannel, "program_update_check_mode");
		try_get_to_defaulted(*found, m_AutoLaunchTF2, "auto_launch_tf2", DEFAULTS.m_AutoLaunchTF2);
		try_get_to_defaulted(*found, m_AutoChatWarnings, "auto_chat_warnings", DEFAULTS.m_AutoChatWarnings);
		try_get_to_defaulted(*found, m_AutoChatWarningsConnecting, "auto_chat_warnings_connecting", DEFAULTS.m_AutoChatWarningsConnecting);
		try_get_to_defaulted(*found, m_AutoVotekick, "auto_votekick", DEFAULTS.m_AutoVotekick);
		try_get_to_defaulted(*found, m_AutoVotekickDelay, "auto_votekick_delay", DEFAULTS.m_AutoVotekickDelay);
		try_get_to_defaulted(*found, m_AutoMark, "auto_mark", DEFAULTS.m_AutoMark);
		try_get_to_defaulted(*found, m_LazyLoadAPIData, "lazy_load_api_data", DEFAULTS.m_LazyLoadAPIData);

		{
			std::string apiKey;
			try_get_to_defaulted(*found, apiKey, "steam_api_key");
			SetSteamAPIKey(std::move(apiKey));
		}

		if (auto foundDir = found->find("steam_dir_override"); foundDir != found->end())
			m_SteamDirOverride = foundDir->get<std::string_view>();
		if (auto foundDir = found->find("tf_game_dir_override"); foundDir != found->end())
			m_TFDirOverride = foundDir->get<std::string_view>();
	}

	try_get_to_defaulted(json, m_Discord, "discord");
	try_get_to_defaulted(json, m_Theme, "theme");
	if (!try_get_to_defaulted(json, m_GotoProfileSites, "goto_profile_sites"))
	{
		// Some defaults
		m_GotoProfileSites.push_back({ "Steam Community", "https://steamcommunity.com/profiles/%SteamID64%" });
		m_GotoProfileSites.push_back({ "logs.tf", "https://logs.tf/profile/%SteamID64%" });
		m_GotoProfileSites.push_back({ "RGL", "https://rgl.gg/Public/PlayerProfile.aspx?p=%SteamID64%" });
		m_GotoProfileSites.push_back({ "SteamRep", "https://steamrep.com/profiles/%SteamID64%" });
		m_GotoProfileSites.push_back({ "UGC League", "https://www.ugcleague.com/players_page.cfm?player_id=%SteamID64%" });
	}
}

void Settings::Serialize(nlohmann::json& json) const
{
	json =
	{
		{ "$schema", "https://raw.githubusercontent.com/PazerOP/tf2_bot_detector/master/schemas/v3/settings.schema.json" },
		{ "theme", m_Theme },
		{ "general",
			{
				{ "sleep_when_unfocused", m_SleepWhenUnfocused },
				{ "auto_temp_mute", m_AutoTempMute },
				{ "program_update_check_mode", m_ReleaseChannel },
				{ "steam_api_key", GetSteamAPIKey() },
				{ "auto_launch_tf2", m_AutoLaunchTF2 },
				{ "auto_chat_warnings", m_AutoChatWarnings },
				{ "auto_chat_warnings_connecting", m_AutoChatWarningsConnecting },
				{ "auto_votekick", m_AutoVotekick },
				{ "auto_votekick_delay", m_AutoVotekickDelay },
				{ "auto_mark", m_AutoMark },
				{ "lazy_load_api_data", m_LazyLoadAPIData },
			}
		},
		{ "goto_profile_sites", m_GotoProfileSites },
		{ "discord", m_Discord },
	};

	if (!m_SteamDirOverride.empty())
		json["general"]["steam_dir_override"] = m_SteamDirOverride.string();
	if (!m_TFDirOverride.empty())
		json["general"]["tf_game_dir_override"] = m_TFDirOverride.string();
	if (m_LocalSteamIDOverride.IsValid())
		json["general"]["local_steamid_override"] = m_LocalSteamIDOverride;
	if (m_AllowInternetUsage)
		json["general"]["allow_internet_usage"] = *m_AllowInternetUsage;
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
