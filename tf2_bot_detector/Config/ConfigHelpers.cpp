#include "ConfigHelpers.h"
#include "HTTPHelpers.h"
#include "JSONHelpers.h"
#include "Log.h"
#include "RegexHelpers.h"
#include "Version.h"

#include <mh/text/case_insensitive_string.hpp>
#include <mh/text/string_insertion.hpp>
#include <nlohmann/json.hpp>

#include <fstream>
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

static void SaveJSONToFile(const std::filesystem::path& filename, const nlohmann::json& json)
{
	auto jsonString = json.dump(1, '\t', true);

	std::ofstream file(filename, std::ios::binary);
	if (!file.good())
		throw std::runtime_error("Failed to open file for writing");

	file << jsonString << '\n';
	if (!file.good())
		throw std::runtime_error("Failed to write json to file");
}

static bool TryAutoUpdate(const std::filesystem::path& filename, const nlohmann::json& existingJson, IConfigFileHandler& loader)
{
	auto fileInfoJson = existingJson.find("file_info");
	if (fileInfoJson == existingJson.end())
	{
		DebugLog("Skipping auto-update of "s << filename << ": file_info object missing");
		return false;
	}

	const ConfigFileInfo info(*fileInfoJson);
	if (info.m_UpdateURL.empty())
	{
		DebugLog("Skipping auto-update of "s << filename << ": update_url was empty");
		return false;
	}

	nlohmann::json newJson;
	try
	{
		newJson = HTTP::GetJSON(info.m_UpdateURL);
	}
	catch (const std::exception& e)
	{
		LogError("Failed to auto-update "s << filename << ": failed to parse new json from "
			<< info.m_UpdateURL << ": " << e.what());
		return false;
	}

	if (auto found = newJson.find("$schema"); found != newJson.end())
	{
		try
		{
			loader.ValidateSchema(found->get<std::string>());
		}
		catch (const std::exception& e)
		{
			LogError("Failed to auto-update "s << filename << " from " << info.m_UpdateURL
				<< ": new json failed schema validation: " << e.what());
			return false;
		}
	}
	else
	{
		LogError("Failed to auto-update "s << filename << ": new json missing $schema property");
		return false;
	}

	if (loader.Deserialize(newJson))
	{
		try
		{
			SaveJSONToFile(filename, newJson);
			DebugLog("Wrote auto-updated config file from "s << info.m_UpdateURL << " to " << filename);
		}
		catch (const std::exception& e)
		{
			LogError("Successfully downloaded and deserialized new version of "s
				<< filename << " from " << info.m_UpdateURL
				<< ", but couldn't write it back to disk. Reason: " << e.what());
		}

		return true;
	}
	else
	{
		LogError("Skipping auto-update of "s << filename << ": failed to deserialize response from " << info.m_UpdateURL);
		return false;
	}
}

void tf2_bot_detector::to_json(nlohmann::json& j, const ConfigFileInfo& d)
{
	if (d.m_Authors.empty())
		throw std::invalid_argument("Authors list cannot be empty");
	if (d.m_Title.empty())
		throw std::invalid_argument("Title cannot be empty");

	j =
	{
		{ "authors", d.m_Authors },
		{ "title", d.m_Title },
	};

	if (!d.m_Description.empty())
		j["description"] = d.m_Description;
	if (!d.m_UpdateURL.empty())
		j["update_url"] = d.m_UpdateURL;
}

void tf2_bot_detector::from_json(const nlohmann::json& j, ConfigFileInfo& d)
{
	if (!try_get_to(j, "authors", d.m_Authors))
		throw std::runtime_error("Failed to parse list of authors");
	if (!try_get_to(j, "title", d.m_Title))
		throw std::runtime_error("Missing required property \"title\"");

	if (!try_get_to(j, "description", d.m_Description))
		d.m_Description.clear();
	if (!try_get_to(j, "update_url", d.m_UpdateURL))
		d.m_UpdateURL.clear();
}

bool tf2_bot_detector::LoadConfigFile(const std::filesystem::path& filename, IConfigFileHandler& loader, bool autoUpdate)
{
	nlohmann::json json;
	{
		std::ifstream file(filename);
		if (!file.good())
		{
			DebugLog("Failed to open "s << filename);
			return false;
		}

		Log("Loading "s << filename << "...");

		try
		{
			file >> json;
		}
		catch (const std::exception& e)
		{
			LogError(std::string(__FUNCTION__ ": Exception when parsing JSON from ") << filename << ": " << e.what());
		}
	}

	if (auto found = json.find("$schema"); found != json.end())
	{
		try
		{
			loader.ValidateSchema(found->get<std::string>());
		}
		catch (const std::exception& e)
		{
			LogError("Failed to load "s << filename << ": existing file failed schema validation: " << e.what());
			return false;
		}
	}
	else
	{
		LogError("Failed to load "s << filename << ": existing json missing $schema property");
		return false;
	}

	if (autoUpdate && TryAutoUpdate(filename, json, loader))
		return true;

	if (!loader.Deserialize(json))
	{
		LogError("Failed to load "s << filename << ": existing file failed to deserialize, and auto-update did not occur");
		return false;
	}

	try
	{
		json.clear();
		loader.Serialize(json);
		SaveJSONToFile(filename, json);
	}
	catch (const std::exception& e)
	{
		LogWarning("Successfully deserialized "s << filename << ", but failed to resave: " << e.what());
	}

	return true;
}

bool tf2_bot_detector::SaveConfigFile(const std::filesystem::path& filename, IConfigFileHandler& handler)
{
	nlohmann::json json;

	try
	{
		handler.Serialize(json);
	}
	catch (const std::exception& e)
	{
		LogError("Failed to serialize "s << filename << ": " << e.what());
		return false;
	}

	try
	{
		SaveJSONToFile(filename, json);
	}
	catch (const std::exception& e)
	{
		LogError("Failed to write "s << filename << ": " << e.what());
		return false;
	}

	return true;
}

ConfigSchemaInfo::ConfigSchemaInfo(const std::string_view& schema)
{
	std::match_results<std::string_view::iterator> match;
	const std::regex schemaRegex(
		R"regex(https:\/\/raw\.githubusercontent\.com\/PazerOP\/tf2_bot_detector\/(\w+)\/schemas\/v(\d+)\/(\w+)\.schema\.json)regex");
	if (!std::regex_match(schema.begin(), schema.end(), match, schemaRegex))
		throw std::runtime_error("Unknown schema "s << std::quoted(schema));

	from_chars_throw(match[2], m_Version);

	{
		const std::string_view branchsv(&*match[1].first, match[1].length());
		if (!mh::case_insensitive_compare(branchsv, "autoupdate"sv) &&
			!mh::case_insensitive_compare(branchsv, "master"sv))
		{
			throw std::runtime_error("Invalid branch "s << std::quoted(branchsv));
		}
	}

	{
		const std::string_view typesv(&*match[3].first, match[3].length());
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
