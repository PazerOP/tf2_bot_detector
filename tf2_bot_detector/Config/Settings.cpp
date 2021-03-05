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
#include <random>
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
			{ "colors", d.m_Colors },
			{ "global_scale", d.m_GlobalScale },
			{ "font_temp", d.m_Font },
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
		using Colors = Settings::Theme::Colors;

		try_get_to_defaulted(j, d, &Colors::m_ScoreboardCheaterBG, { "scoreboard_marked_cheater.bg", "scoreboard_cheater" });
		try_get_to_defaulted(j, d, &Colors::m_ScoreboardSuspiciousBG, { "scoreboard_marked_suspicious.bg", "scoreboard_suspicious" });
		try_get_to_defaulted(j, d, &Colors::m_ScoreboardExploiterBG, { "scoreboard_marked_exploiter.bg", "scoreboard_exploiter" });
		try_get_to_defaulted(j, d, &Colors::m_ScoreboardRacistBG, { "scoreboard_marked_racism.bg", "scoreboard_racism" });
		try_get_to_defaulted(j, d, &Colors::m_ScoreboardYouFG, { "scoreboard_you.fg", "scoreboard_you" });
		try_get_to_defaulted(j, d, &Colors::m_ScoreboardConnectingFG, { "scoreboard_connecting.fg", "scoreboard_connecting" });
		try_get_to_defaulted(j, d, &Colors::m_ScoreboardFriendlyTeamBG, { "scoreboard_team_friendly.bg", "friendly_team" });
		try_get_to_defaulted(j, d, &Colors::m_ScoreboardEnemyTeamBG, { "scoreboard_team_enemy.bg", "enemy_team" });

		try_get_to_defaulted(j, d, &Colors::m_ChatLogYouFG, "chat_log_you.fg");
		try_get_to_defaulted(j, d, &Colors::m_ChatLogFriendlyTeamFG, "chat_log_team_friendly.fg");
		try_get_to_defaulted(j, d, &Colors::m_ChatLogEnemyTeamFG, "chat_log_team_enemy.fg");
	}

	void from_json(const nlohmann::json& j, Settings::Theme& d)
	{
		d.m_Colors = j.at("colors");
		try_get_to_defaulted(j, d, &Settings::Theme::m_GlobalScale, "global_scale");
		try_get_to_defaulted(j, d, &Settings::Theme::m_Font, "font_temp");
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

	void to_json(nlohmann::json& j, const Settings::UIState::MainWindow& d)
	{
		j =
		{
			{ "app_log_enabled", d.m_AppLogEnabled },
			{ "chat_enabled", d.m_ChatEnabled },
			{ "scoreboard_enabled", d.m_ScoreboardEnabled },
			{ "team_stats_enabled", d.m_TeamStatsEnabled },
		};
	}
	void from_json(const nlohmann::json& j, Settings::UIState::MainWindow& d)
	{
		using MainWindow = Settings::UIState::MainWindow;
		try_get_to_defaulted(j, d, &MainWindow::m_AppLogEnabled, "app_log_enabled");
		try_get_to_defaulted(j, d, &MainWindow::m_ChatEnabled, "chat_enabled");
		try_get_to_defaulted(j, d, &MainWindow::m_ScoreboardEnabled, "scoreboard_enabled");
		try_get_to_defaulted(j, d, &MainWindow::m_TeamStatsEnabled, "team_stats_enabled");
	}

	void to_json(nlohmann::json& j, const Settings::UIState& d)
	{
		j =
		{
			{ "main_window", d.m_MainWindow },
		};
	}
	void from_json(const nlohmann::json& j, Settings::UIState& d)
	{
		try_get_to_defaulted(j, d, &Settings::UIState::m_MainWindow, "main_window");
	}

	void to_json(nlohmann::json& j, const SteamAPIMode& d)
	{
		switch (d)
		{
		case SteamAPIMode::Direct:    j = "direct";   break;
		case SteamAPIMode::Disabled:  j = "disabled"; break;

		default:
			LogError("Unknown SteamAPIMode {}, defaulting to proxy", +std::underlying_type_t<SteamAPIMode>(d));
			[[fallthrough]];
		case SteamAPIMode::Proxy:     j = "proxy";    break;
		}
	}
	void from_json(const nlohmann::json& j, SteamAPIMode& d)
	{
		auto sv = j.get<std::string_view>();
		if (sv == "direct")
			d = SteamAPIMode::Direct;
		else if (sv == "disabled")
			d = SteamAPIMode::Disabled;
		else
		{
			if (j != "proxy")
				LogError("Unknown SteamAPIMode {}, defaulting to proxy", std::quoted(sv));

			d = SteamAPIMode::Proxy;
		}
	}

	void to_json(nlohmann::json& j, const Settings::Mods& d)
	{
		j =
		{
			{ "disabled_list", d.m_DisabledList },
		};
	}
	void from_json(const nlohmann::json& j, Settings::Mods& d)
	{
		try_get_to_defaulted(j, d.m_DisabledList, "disabled_list");
	}
}

bool ISteamAPISettings::IsSteamAPIAvailable() const
{
	switch (GetSteamAPIMode())
	{
	case SteamAPIMode::Disabled:
		return false;
	case SteamAPIMode::Proxy:
		return true;
	case SteamAPIMode::Direct:
		return !GetSteamAPIKey().empty();
	}

	LogError("Unknown SteamAPIMode {}", +std::underlying_type_t<SteamAPIMode>(GetSteamAPIMode()));
	return false;
}

std::string GeneralSettings::GetSteamAPIKey() const
{
	return (m_SteamAPIMode == SteamAPIMode::Direct) ? m_SteamAPIKey : std::string();
}

void GeneralSettings::SetSteamAPIKey(std::string key)
{
	assert(key.empty() || key.size() == 32);
	ILogManager::GetInstance().AddSecret(key, mh::format("<STEAM_API_KEY:{}>", key.size()));
	m_SteamAPIKey = std::move(key);
}

void tf2_bot_detector::to_json(nlohmann::json& j, const Font& d)
{
	switch (d)
	{
	case Font::ProggyTiny_10px:   j = "proggy_tiny_10px"; break;
	case Font::ProggyTiny_20px:   j = "proggy_tiny_20px"; break;
	case Font::ProggyClean_13px:  j = "proggy_clean_13px"; break;
	case Font::ProggyClean_26px:  j = "proggy_clean_26px"; break;
	}
}

void tf2_bot_detector::from_json(const nlohmann::json& j, Font& d)
{
	auto value = mh::tolower(j.get<std::string_view>());
	if (value == "proggy_tiny_10px")
		d = Font::ProggyTiny_10px;
	else if (value == "proggy_tiny_20px")
		d = Font::ProggyTiny_20px;
	else if (value == "proggy_clean_13px")
		d = Font::ProggyClean_13px;
	else if (value == "proggy_clean_26px")
		d = Font::ProggyClean_26px;
	else
		throw std::invalid_argument(mh::format("{}: Unknown font {}", mh::source_location::current(), std::quoted(value)));
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

uint16_t Settings::TF2Interface::GetRandomRCONPort() const
{
	std::mt19937 generator;
	{
		std::random_device randomSeed;
		generator.seed(randomSeed());
	}

	std::uniform_int_distribution<uint16_t> dist(m_RCONPortMin, m_RCONPortMax);
	return dist(generator);
}

void tf2_bot_detector::to_json(nlohmann::json& j, const Settings::TF2Interface& d)
{
	j =
	{
		{ "rcon_port_min", d.m_RCONPortMin },
		{ "rcon_port_max", d.m_RCONPortMax },
	};
}

void tf2_bot_detector::from_json(const nlohmann::json& j, Settings::TF2Interface& d)
{
	try_get_to_defaulted(j, d, &Settings::TF2Interface::m_RCONPortMin, "rcon_port_min");
	try_get_to_defaulted(j, d, &Settings::TF2Interface::m_RCONPortMax, "rcon_port_max");

	if (d.m_RCONPortMin > d.m_RCONPortMax)
		std::swap(d.m_RCONPortMin, d.m_RCONPortMax);
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
		m_HTTPClient = IHTTPClient::Create();

	return m_HTTPClient;
}

void Settings::ValidateSchema(const ConfigSchemaInfo& schema) const
{
	if (schema.m_Type != "settings")
		throw std::runtime_error(mh::format("Schema {} is not a settings file", schema.m_Type));
	if (schema.m_Version != 3)
		throw std::runtime_error(mh::format("Schema must be version {} (current version {})", SETTINGS_SCHEMA_VERSION, schema.m_Version));
}

void Settings::AddDefaultGotoProfileSites()
{
	m_GotoProfileSites.push_back({ "Steam Community", "https://steamcommunity.com/profiles/%SteamID64%" });
	m_GotoProfileSites.push_back({ "logs.tf", "https://logs.tf/profile/%SteamID64%" });
	m_GotoProfileSites.push_back({ "RGL", "https://rgl.gg/Public/PlayerProfile.aspx?p=%SteamID64%" });
	m_GotoProfileSites.push_back({ "SteamRep", "https://steamrep.com/profiles/%SteamID64%" });
	m_GotoProfileSites.push_back({ "UGC League", "https://www.ugcleague.com/players_page.cfm?player_id=%SteamID64%" });
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
		try_get_to_defaulted(*found, m_ConfigCompatibilityMode, "config_compatibility_mode", DEFAULTS.m_ConfigCompatibilityMode);

		{
			std::string apiKey;
			try_get_to_defaulted(*found, apiKey, "steam_api_key");
			SetSteamAPIKey(std::move(apiKey));
		}

		try_get_to_defaulted(*found, m_SteamAPIMode, "steam_api_mode",
			GetSteamAPIKeyDirect().size() == 32 ? SteamAPIMode::Direct : SteamAPIMode::Proxy);

		if (auto foundDir = found->find("steam_dir_override"); foundDir != found->end())
			m_SteamDirOverride = foundDir->get<std::string_view>();
		if (auto foundDir = found->find("tf_game_dir_override"); foundDir != found->end())
			m_TFDirOverride = foundDir->get<std::string_view>();
	}

	try_get_to_defaulted(json, m_Discord, "discord");
	try_get_to_defaulted(json, m_Theme, "theme");
	try_get_to_defaulted(json, m_TF2Interface, "tf2_interface");
	try_get_to_defaulted(json, m_UIState, "ui_state");
	try_get_to_defaulted(json, m_Mods, "mods");

	if (!try_get_to_defaulted(json, m_GotoProfileSites, "goto_profile_sites"))
		AddDefaultGotoProfileSites();
}

void Settings::PostLoad(bool deserialized)
{
	if (!deserialized)
		AddDefaultGotoProfileSites();
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
				{ "steam_api_key", GetSteamAPIKeyDirect() },
				{ "steam_api_mode", m_SteamAPIMode },
				{ "auto_launch_tf2", m_AutoLaunchTF2 },
				{ "auto_chat_warnings", m_AutoChatWarnings },
				{ "auto_chat_warnings_connecting", m_AutoChatWarningsConnecting },
				{ "auto_votekick", m_AutoVotekick },
				{ "auto_votekick_delay", m_AutoVotekickDelay },
				{ "auto_mark", m_AutoMark },
				{ "lazy_load_api_data", m_LazyLoadAPIData },
				{ "config_compatibility_mode", m_ConfigCompatibilityMode },
			}
		},
		{ "goto_profile_sites", m_GotoProfileSites },
		{ "discord", m_Discord },
		{ "tf2_interface", m_TF2Interface },
		{ "ui_state", m_UIState },
		{ "mods", m_Mods },
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
