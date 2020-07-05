#include "Rules.h"
#include "IPlayer.h"
#include "JSONHelpers.h"
#include "Log.h"
#include "PlayerListJSON.h"
#include "Settings.h"

#include <mh/text/case_insensitive_string.hpp>
#include <mh/text/string_insertion.hpp>
#include <nlohmann/json.hpp>

#include <fstream>
#include <iomanip>
#include <regex>
#include <stdexcept>
#include <string>

using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace tf2_bot_detector;

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

		if (!try_get_to(j, "case_sensitive", d.m_CaseSensitive))
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
		try_get_to(j, "mark", d.m_Mark);
		try_get_to(j, "unmark", d.m_Unmark);
	}

	void from_json(const nlohmann::json& j, ModerationRule& d)
	{
		d.m_Description = j.at("description");
		d.m_Triggers = j.at("triggers");
		d.m_Actions = j.at("actions");
	}
}

ModerationRules::ModerationRules(const Settings& settings) :
	m_CFGGroup(settings)
{
	// Immediately load and resave to normalize any formatting
	LoadFiles();
}

bool ModerationRules::LoadFiles()
{
	m_CFGGroup.LoadFiles();
	return true;
}

bool ModerationRules::SaveFile() const
{
	m_CFGGroup.SaveFile();
	return true;
}

cppcoro::generator<const ModerationRule&> tf2_bot_detector::ModerationRules::GetRules() const
{
	if (mh::is_future_ready(m_CFGGroup.m_OfficialList))
	{
		for (const auto& rule : m_CFGGroup.m_OfficialList.get().m_Rules)
			co_yield rule;
	}

	if (m_CFGGroup.m_UserList)
	{
		for (const auto& rule : m_CFGGroup.m_UserList->m_Rules)
			co_yield rule;
	}

	if (mh::is_future_ready(m_CFGGroup.m_ThirdPartyLists))
	{
		for (const auto& rule : m_CFGGroup.m_ThirdPartyLists.get())
			co_yield rule;
	}
}

void ModerationRules::RuleFile::ValidateSchema(const ConfigSchemaInfo& schema) const
{
	if (schema.m_Type != "rules")
		throw std::runtime_error("Schema is not a rules list");
	if (schema.m_Version != RULES_SCHEMA_VERSION)
		throw std::runtime_error("Schema must be version 3");
}

void ModerationRules::RuleFile::Deserialize(const nlohmann::json& json)
{
	SharedConfigFileBase::Deserialize(json);

	m_Rules = json.at("rules").get<RuleList_t>();
}

void ModerationRules::RuleFile::Serialize(nlohmann::json& json) const
{
	SharedConfigFileBase::Serialize(json);

	if (!m_Schema || m_Schema->m_Type != "rules" || m_Schema->m_Version != RULES_SCHEMA_VERSION)
		json["$schema"] = ConfigSchemaInfo("rules", RULES_SCHEMA_VERSION);

	json["rules"] = m_Rules;
}

void ModerationRules::ConfigFileGroup::CombineEntries(RuleList_t& list, const RuleFile& file) const
{
	list.insert(list.end(), file.m_Rules.begin(), file.m_Rules.end());
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

		const auto name = player.GetNameUnsafe();
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
