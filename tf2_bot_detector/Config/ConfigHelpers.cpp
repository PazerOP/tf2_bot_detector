#include "ConfigHelpers.h"
#include "JSONHelpers.h"
#include "Log.h"
#include "RegexHelpers.h"
#include "Version.h"

#include <mh/text/case_insensitive_string.hpp>
#include <mh/text/string_insertion.hpp>
#include <nlohmann/json.hpp>

#include <regex>

using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace tf2_bot_detector;

auto tf2_bot_detector::GetConfigFilePaths(const std::string_view& basename) -> ConfigFilePaths
{
	ConfigFilePaths retVal;

	const std::filesystem::path cfg("cfg");
	if (std::filesystem::is_directory(cfg))
	{
		try
		{
			for (const auto& file : std::filesystem::directory_iterator(cfg,
				std::filesystem::directory_options::follow_directory_symlink | std::filesystem::directory_options::skip_permission_denied))
			{
				static const std::regex s_PlayerListRegex(std::string(basename) << R"regex(\.(.*\.)?json)regex", std::regex::optimize);
				const auto path = file.path();
				const auto filename = path.filename().string();
				if (mh::case_insensitive_compare(filename, std::string(basename) << ".json"sv))
					retVal.m_User = cfg / filename;
				else if (mh::case_insensitive_compare(filename, std::string(basename) << ".official.json"sv))
					retVal.m_Official = cfg / filename;
				else if (std::regex_match(filename.begin(), filename.end(), s_PlayerListRegex))
					retVal.m_Others.push_back(cfg / filename);
			}
		}
		catch (const std::filesystem::filesystem_error& e)
		{
			LogError(std::string(__FUNCTION__ ": Exception when loading playerlist.*.json files from ./cfg/: ") << e.what());
		}
	}

	return retVal;
}

ConfigFileInfo::ConfigFileInfo(const nlohmann::json& j)
{
	if (!try_get_to(j, "authors", m_Authors))
		throw std::runtime_error("Failed to parse list of authors");
	if (!try_get_to(j, "title", m_Title))
		throw std::runtime_error("Missing required property \"title\"");

	try_get_to(j, "description", m_Description);
	try_get_to(j, "update_url", m_UpdateURL);
}

ConfigSchemaInfo::ConfigSchemaInfo(const char* schema)
{
	std::cmatch match;
	const std::regex schemaRegex(
		R"regex(https:\/\/raw\.githubusercontent\.com\/PazerOP\/tf2_bot_detector\/(\w+)\/schemas\/v(\d+)\/(\w+)\.schema\.json)regex");
	if (!std::regex_match(schema, match, schemaRegex))
		throw std::runtime_error("Unknown schema "s << std::quoted(schema));

	from_chars_throw(match[2], m_Version);

	{
		const std::string_view branchsv(match[1].first, match[1].length());
		if (!mh::case_insensitive_compare(branchsv, "autoupdate"sv) &&
			!mh::case_insensitive_compare(branchsv, "master"sv))
		{
			throw std::runtime_error("Invalid branch "s << std::quoted(branchsv));
		}
	}

	{
		const std::string_view typesv(match[3].first, match[3].length());
		if (mh::case_insensitive_compare(typesv, "playerlist"sv))
			m_Type = Type::Playerlist;
		else if (mh::case_insensitive_compare(typesv, "settings"sv))
			m_Type = Type::Settings;
		else if (mh::case_insensitive_compare(typesv, "rules"sv))
			m_Type = Type::Rules;
		else if (mh::case_insensitive_compare(typesv, "whitelist"sv))
			m_Type = Type::Whitelist;
		else
			throw std::runtime_error("Unknown schema type "s << std::quoted(typesv));
	}
}

ConfigSchemaInfo::ConfigSchemaInfo(const nlohmann::json& j) :
	ConfigSchemaInfo(j.at("$schema").get<std::string>().c_str())
{
}
