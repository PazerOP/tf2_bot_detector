#include "Settings.h"
#include "PlayerListJSON.h"
#include "IPlayer.h"

#include <mh/text/case_insensitive_string.hpp>
#include <mh/text/string_insertion.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <regex>

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
			{ "scoreboard_you", d.m_ScoreboardYou },
		};
	}

	void to_json(nlohmann::json& j, const Settings::Theme& d)
	{
		j =
		{
			{ "colors", d.m_Colors }
		};
	}

	void to_json(nlohmann::json& j, const TextMatch& d)
	{
		j =
		{
			{ "mode", d.m_Mode },
			{ "case_sensitive", d.m_CaseSensitive },
			{ "patterns", d.m_Patterns },
		};
	}

	void to_json(nlohmann::json& j, const ModerationRule::Triggers& d)
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

	void to_json(nlohmann::json& j, const ModerationRule::Actions& d)
	{
		if (!d.m_Mark.empty())
			j["mark"] = d.m_Mark;
		if (!d.m_Unmark.empty())
			j["unmark"] = d.m_Unmark;
	}

	void to_json(nlohmann::json& j, const ModerationRule& d)
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
		j.at("scoreboard_you").get_to(d.m_ScoreboardYou);
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

	void from_json(const nlohmann::json& j, TextMatch& d)
	{
		d.m_Mode = j.at("mode");
		d.m_Patterns = j.at("patterns").get<std::vector<std::string>>();

		if (auto found = j.find("case_sensitive"); found != j.end())
			d.m_CaseSensitive = *found;
		else
			d.m_CaseSensitive = false;
	}

	void from_json(const nlohmann::json& j, ModerationRule::Triggers& d)
	{
		if (auto found = j.find("mode"); found != j.end())
			d.m_Mode = *found;

		if (auto found = j.find("chatmsg_text_match"); found != j.end())
			d.m_ChatMsgTextMatch.emplace(TextMatch(*found));
		if (auto found = j.find("username_text_match"); found != j.end())
			d.m_UsernameTextMatch.emplace(TextMatch(*found));
	}

	void from_json(const nlohmann::json& j, ModerationRule::Actions& d)
	{
		if (auto found = j.find("mark"); found != j.end())
			d.m_Mark = found->get<std::vector<PlayerAttributes>>();
		if (auto found = j.find("unmark"); found != j.end())
			d.m_Unmark = found->get<std::vector<PlayerAttributes>>();
	}

	void from_json(const nlohmann::json& j, ModerationRule& d)
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

	if (auto found = json.find("local_steamid"); found != json.end())
		m_LocalSteamID = json.at("local_steamid");

	m_SleepWhenUnfocused = json.at("sleep_when_unfocused");
	m_TFDir = json.at("tf_game_dir").get<std::string>();
	m_Theme = json.at("theme");
	m_Rules = json.at("rules").get<std::vector<ModerationRule>>();
}

void Settings::SaveFile() const
{
	nlohmann::json json =
	{
		{ "$schema", "./schema/settings.schema.json" },
		{ "sleep_when_unfocused", m_SleepWhenUnfocused },
		{ "tf_game_dir", m_TFDir.string() },
		{ "theme", m_Theme },
		{ "rules", m_Rules },
	};

	if (m_LocalSteamID.IsValid())
		json["local_steamid"] = m_LocalSteamID;

	// Make sure we successfully serialize BEFORE we destroy our file
	auto jsonString = json.dump(1, '\t', true);
	{
		std::ofstream file(s_SettingsPath);
		file << jsonString << '\n';
	}
}

bool TextMatch::Match(const std::string_view& text) const
{
	switch (m_Mode)
	{
	case TextMatchMode::Equal:
	{
		return std::any_of(m_Patterns.begin(), m_Patterns.end(), [&](const std::string_view& pattern)
			{
				if (m_CaseSensitive)
					return text == pattern;
				else
					return mh::case_insensitive_compare(text, pattern);
			});
	}
	case TextMatchMode::Contains:
	{
		return std::any_of(m_Patterns.begin(), m_Patterns.end(), [&](const std::string_view& pattern)
			{
				if (m_CaseSensitive)
					return text.find(pattern) != text.npos;
				else
					return mh::case_insensitive_view(text).find(mh::case_insensitive_view(pattern)) != text.npos;
			});
		break;
	}
	case TextMatchMode::StartsWith:
	{
		return std::any_of(m_Patterns.begin(), m_Patterns.end(), [&](const std::string_view& pattern)
			{
				if (m_CaseSensitive)
					return text.starts_with(pattern);
				else
					return mh::case_insensitive_view(text).starts_with(mh::case_insensitive_view(pattern));
			});
	}
	case TextMatchMode::EndsWith:
	{
		return std::any_of(m_Patterns.begin(), m_Patterns.end(), [&](const std::string_view& pattern)
			{
				if (m_CaseSensitive)
					return text.ends_with(pattern);
				else
					return mh::case_insensitive_view(text).ends_with(mh::case_insensitive_view(pattern));
			});
	}
	case TextMatchMode::Regex:
	{
		return std::any_of(m_Patterns.begin(), m_Patterns.end(), [&](const std::string_view& pattern)
			{
				std::regex_constants::syntax_option_type options{};
				if (!m_CaseSensitive)
					options = std::regex_constants::icase;

				std::regex r(pattern.begin(), pattern.end(), options);
				return std::regex_match(text.begin(), text.end(), r);
			});
	}
	case TextMatchMode::Word:
	{
		static const std::regex s_WordRegex(R"regex((\w+))regex", std::regex::optimize);

		const auto end = std::regex_iterator<std::string_view::const_iterator>{};
		for (auto it = std::regex_iterator<std::string_view::const_iterator>(text.begin(), text.end(), s_WordRegex);
			it != end; ++it)
		{
			const std::string_view itStr(&*it->operator[](0).first, it->operator[](0).length());
			const auto anyMatches = std::any_of(m_Patterns.begin(), m_Patterns.end(), [&](const std::string_view& pattern)
				{
					if (m_CaseSensitive)
						return itStr == pattern;
					else
						return mh::case_insensitive_compare(itStr, pattern);
				});

			if (anyMatches)
				return true;
		}

		return false;
	}

	default:
		throw std::runtime_error("Unknown TextMatchMode "s << +std::underlying_type_t<TextMatchMode>(m_Mode));
	}
}

bool ModerationRule::Match(const IPlayer& player) const
{
	return Match(player, std::string_view{});
}

bool ModerationRule::Match(const IPlayer& player, const std::string_view& chatMsg) const
{
	const bool usernameMatch = [&]()
	{
		if (!m_Triggers.m_UsernameTextMatch)
			return false;

		const auto name = player.GetName();
		if (name.empty())
			return false;

		return m_Triggers.m_UsernameTextMatch->Match(name);
	}();
	if (m_Triggers.m_Mode == TriggerMatchMode::MatchAny && usernameMatch)
		return true;

	const bool chatMsgMatch = [&]()
	{
		if (!m_Triggers.m_ChatMsgTextMatch || chatMsg.empty())
			return false;

		return m_Triggers.m_ChatMsgTextMatch->Match(chatMsg);
	}();
	if (m_Triggers.m_Mode == TriggerMatchMode::MatchAny && chatMsgMatch)
		return true;

	if (m_Triggers.m_Mode == TriggerMatchMode::MatchAll)
	{
		if (m_Triggers.m_UsernameTextMatch && m_Triggers.m_ChatMsgTextMatch)
			return usernameMatch && chatMsgMatch;
		else if (m_Triggers.m_UsernameTextMatch)
			return usernameMatch;
		else if (m_Triggers.m_ChatMsgTextMatch)
			return chatMsgMatch;
	}

	return false;
}
