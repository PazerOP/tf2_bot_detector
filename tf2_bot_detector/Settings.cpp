#include "Settings.h"
#include "PlayerListJSON.h"

#include <mh/text/string_insertion.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>

using namespace tf2_bot_detector;
using namespace std::string_literals;
using namespace std::string_view_literals;

static std::filesystem::path s_SettingsPath("cfg/settings.json");

namespace tf2_bot_detector
{
	void to_json(nlohmann::json& j, const TextMatchMode& d)
	{
		switch (d)
		{
		case TextMatchMode::Equal:       j = "equal"; break;
		case TextMatchMode::Contains:    j = "contains"; break;
		case TextMatchMode::StartsWith:  j = "starts_with"; break;
		case TextMatchMode::EndsWith:    j = "ends_with"; break;
		case TextMatchMode::Regex:       j = "regex"; break;
		case TextMatchMode::Word:        j = "word"; break;

		default:
			throw std::runtime_error("Unknown TextMatchMode value "s << +std::underlying_type_t<TextMatchMode>(d));
		}
	}

	void to_json(nlohmann::json& j, const TriggerMatchMode& d)
	{
		switch (d)
		{
		case TriggerMatchMode::MatchAll:  j = "match_all"; break;
		case TriggerMatchMode::MatchAny:  j = "match_any"; break;

		default:
			throw std::runtime_error("Unknown TriggerMatchMode value "s << +std::underlying_type_t<TriggerMatchMode>(d));
		}
	}

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

	void to_json(nlohmann::json& j, const Settings::TextMatch& d)
	{
		j =
		{
			{ "mode", d.m_Mode },
			{ "case_sensitive", d.m_CaseSensitive }
		};

		if (d.m_Patterns.size() == 1)
			j["pattern"] = d.m_Patterns.front();
		else
			j["pattern"] = d.m_Patterns;
	}

	void to_json(nlohmann::json& j, const Settings::Rule::Triggers& d)
	{
		size_t count = 0;

		if (d.m_ChatMsgTextMatch)
		{
			j["chatmsg_text_match"] = *d.m_ChatMsgTextMatch;
			count++;
		}
		if (d.m_UsernameTextMatch)
		{
			j["username_text_match"] = *d.m_UsernameTextMatch;
			count++;
		}

		if (count > 1)
			j["mode"] = d.m_Mode;
	}

	void to_json(nlohmann::json& j, const Settings::Rule::Actions& d)
	{
		if (!d.m_Mark.empty())
			j["mark"] = d.m_Mark;
		if (!d.m_Unmark.empty())
			j["unmark"] = d.m_Unmark;
	}

	void to_json(nlohmann::json& j, const Settings::Rule& d)
	{
		j =
		{
			{ "description", d.m_Description },
			{ "triggers", d.m_Triggers },
			{ "actions", d.m_Actions },
		};
	}

	void from_json(const nlohmann::json& j, TriggerMatchMode& d)
	{
		const std::string_view& str = j.get<std::string_view>();
		if (str == "match_all"sv)
			d = TriggerMatchMode::MatchAll;
		else if (str == "match_any"sv)
			d = TriggerMatchMode::MatchAny;
		else
			throw std::runtime_error("Invalid value for TriggerMatchMode "s << std::quoted(str));
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

	void from_json(const nlohmann::json& j, TextMatchMode& mode)
	{
		const std::string_view str = j;

		if (str == "equal"sv)
			mode = TextMatchMode::Equal;
		else if (str == "contains"sv)
			mode = TextMatchMode::Contains;
		else if (str == "starts_with"sv)
			mode = TextMatchMode::StartsWith;
		else if (str == "ends_with"sv)
			mode = TextMatchMode::EndsWith;
		else if (str == "regex"sv)
			mode = TextMatchMode::Regex;
		else if (str == "word"sv)
			mode = TextMatchMode::Word;
		else
			throw std::runtime_error("Unable to parse TextMatchMode from "s << std::quoted(str));
	}

	void from_json(const nlohmann::json& j, Settings::TextMatch& d)
	{
		d.m_Mode = j.at("mode");

		// Patterns, both arrays and single values
		{
			auto& pattern = j.at("pattern");
			if (pattern.is_array())
				d.m_Patterns = pattern.get<std::vector<std::string>>();
			else
				d.m_Patterns.push_back(pattern);
		}

		if (auto found = j.find("case_sensitive"); found != j.end())
			d.m_CaseSensitive = *found;
		else
			d.m_CaseSensitive = false;
	}

	void from_json(const nlohmann::json& j, Settings::Rule::Triggers& d)
	{
		if (auto found = j.find("mode"); found != j.end())
			d.m_Mode = *found;

		if (auto found = j.find("chatmsg_text_match"); found != j.end())
			d.m_ChatMsgTextMatch.emplace(Settings::TextMatch(*found));
		if (auto found = j.find("username_text_match"); found != j.end())
			d.m_UsernameTextMatch.emplace(Settings::TextMatch(*found));
	}

	void from_json(const nlohmann::json& j, Settings::Rule::Actions& d)
	{
		if (auto found = j.find("mark"); found != j.end())
			d.m_Mark = found->get<std::vector<PlayerAttributes>>();
		if (auto found = j.find("unmark"); found != j.end())
			d.m_Unmark = found->get<std::vector<PlayerAttributes>>();
	}

	void from_json(const nlohmann::json& j, Settings::Rule& d)
	{
		d.m_Description = j.at("description");
		d.m_Triggers = j.at("triggers");
		d.m_Actions = j.at("actions");
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
	m_Rules = json.at("rules").get<std::vector<Rule>>();
}

void Settings::SaveFile() const
{
	nlohmann::json json =
	{
		{ "$schema", "./schema/settings.schema.json" },
		{ "theme", m_Theme },
		{ "local_steamid", m_LocalSteamID },
		{ "sleep_when_unfocused", m_SleepWhenUnfocused },
		{ "rules", m_Rules },
	};;

	// Make sure we successfully serialize BEFORE we destroy our file
	auto jsonString = json.dump(1, '\t', true);
	{
		std::ofstream file(s_SettingsPath);
		file << jsonString << '\n';
	}
}
