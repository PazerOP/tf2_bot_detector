#include "ConfigHelpers.h"
#include "Networking/HTTPHelpers.h"
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
			const std::regex s_PlayerListRegex(std::string(basename) << R"regex(\.(.*\.)?json)regex", std::regex::optimize);

			for (const auto& file : std::filesystem::directory_iterator(cfg,
				std::filesystem::directory_options::follow_directory_symlink | std::filesystem::directory_options::skip_permission_denied))
			{
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

static ConfigSchemaInfo LoadAndValidateSchema(const ConfigFileBase& config, const nlohmann::json& json)
{
	ConfigSchemaInfo schema(nullptr);
	if (!try_get_to(json, "$schema", schema))
		throw std::runtime_error("JSON missing $schema property");

	config.ValidateSchema(schema);

	return schema;
}

static bool TryAutoUpdate(const std::filesystem::path& filename, const nlohmann::json& existingJson,
	SharedConfigFileBase& config, const HTTPClient& client)
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
		newJson = nlohmann::json::parse(client.GetString(info.m_UpdateURL));
	}
	catch (const std::exception& e)
	{
		LogError("Failed to auto-update "s << filename << ": failed to parse new json from "
			<< info.m_UpdateURL << ": " << e.what());
		return false;
	}

	try
	{
		LoadAndValidateSchema(config, newJson);
	}
	catch (const std::exception& e)
	{
		LogError("Failed to auto-update "s << filename << " from " << info.m_UpdateURL
			<< ": new json failed schema validation: " << e.what());
		return false;
	}

	try
	{
		config.Deserialize(newJson);
	}
	catch (const std::exception& e)
	{
		LogError("Skipping auto-update of "s << filename << ": failed to deserialize response from "
			<< info.m_UpdateURL << ": " << e.what());
		return false;
	}

	if (!config.SaveFile(filename))
	{
		LogError("Successfully downloaded and deserialized new version of "s
			<< filename << " from " << info.m_UpdateURL
			<< ", but couldn't write it back to disk.");
	}
	else
	{
		DebugLog("Wrote auto-updated config file from "s << info.m_UpdateURL << " to " << filename);
	}

	return true;
}

void tf2_bot_detector::to_json(nlohmann::json& j, const ConfigSchemaInfo& d)
{
	j = ""s << d;
}

void tf2_bot_detector::from_json(const nlohmann::json& j, ConfigSchemaInfo& d)
{
	d = ConfigSchemaInfo(j.get<std::string_view>());
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

bool ConfigFileBase::LoadFile(const std::filesystem::path& filename, const HTTPClient* client)
{
	bool retVal = LoadFileInternal(filename, client);

	if (!SaveFile(filename))
		LogWarning("Failed to resave "s << filename);

	return retVal;
}

bool ConfigFileBase::LoadFileInternal(const std::filesystem::path& filename, const HTTPClient* client)
{
	const auto startTime = clock_t::now();

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
			return false;
		}
	}

	try
	{
		LoadAndValidateSchema(*this, json);
	}
	catch (const std::exception& e)
	{
		LogError("Failed to load "s << filename << ": existing json failed schema validation: " << e.what());
		return false;
	}

	if (client)
	{
		if (auto shared = dynamic_cast<SharedConfigFileBase*>(this))
		{
			bool fileInfoParsed = false;

			try
			{
				if (ConfigFileInfo info; try_get_to(json, "file_info", info))
				{
					shared->m_FileInfo = std::move(info);
					fileInfoParsed = true;
				}
				else
				{
					shared->m_FileInfo.reset();
				}
			}
			catch (const std::exception& e)
			{
				LogWarning("Skipping auto-update for "s << filename << ": failed to parse file_info: " << e.what());
			}

			if (fileInfoParsed && TryAutoUpdate(filename, json, *shared, *client))
				return true;
		}
	}
	else
	{
		DebugLog("Skipping auto-update for "s << filename << " because allowAutoupdate = false.");
	}

	try
	{
		Deserialize(json);
	}
	catch (const std::exception& e)
	{
		LogError("Failed to load "s << filename
			<< ": existing file failed to deserialize, and auto-update did not occur. Reason: " << e.what());
		return false;
	}

	DebugLog("Loaded "s << filename << " in " << to_seconds(clock_t::now() - startTime) << " seconds");
	return true;
}

bool tf2_bot_detector::ConfigFileBase::SaveFile(const std::filesystem::path& filename) const
{
	nlohmann::json json;

	// If we already have a schema loaded, put it in the $schema property
	try
	{
		if (m_Schema)
			json["$schema"] = *m_Schema;
	}
	catch (const std::exception& e)
	{
		LogError("Failed to serialize "s << filename << ": schema failed to validate: " << e.what());
		return false;
	}

	try
	{
		Serialize(json);
	}
	catch (const std::exception& e)
	{
		LogError("Failed to serialize "s << filename << ": " << e.what());
		return false;
	}

	try
	{
		LoadAndValidateSchema(*this, json);
	}
	catch (const std::exception& e)
	{
		LogError("Failed to serialize "s << filename << ": LoadAndValidateSchema threw " << std::quoted(e.what()));
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

void ConfigFileBase::Serialize(nlohmann::json& json) const
{
	json.clear();

	if (m_Schema)
		json["$schema"] = *m_Schema;
}

ConfigSchemaInfo::ConfigSchemaInfo(const std::string_view& schema)
{
	std::match_results<std::string_view::iterator> match;
	const std::regex schemaRegex(
		R"regex(https:\/\/raw\.githubusercontent\.com\/PazerOP\/tf2_bot_detector\/(\w+)\/schemas\/v(\d+)\/(\w+)\.schema\.json)regex");
	if (!std::regex_match(schema.begin(), schema.end(), match, schemaRegex))
		throw std::runtime_error("Unknown schema "s << std::quoted(schema));

	from_chars_throw(match[2], m_Version);

	m_Branch = match[1].str();
	m_Type = match[3].str();
}

ConfigSchemaInfo::ConfigSchemaInfo(std::string type, unsigned version, std::string branch) :
	m_Type(std::move(type)), m_Version(version), m_Branch(std::move(branch))
{
}

void SharedConfigFileBase::Deserialize(const nlohmann::json& json)
{
	ConfigFileBase::Deserialize(json);

	if (ConfigFileInfo info; try_get_to(json, "file_info", info))
		m_FileInfo = std::move(info);
	else
		m_FileInfo.reset();
}

void SharedConfigFileBase::Serialize(nlohmann::json& json) const
{
	ConfigFileBase::Serialize(json);
	if (m_FileInfo)
		json["file_info"] = m_FileInfo.value();
}
