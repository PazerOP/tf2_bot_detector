#include "ConfigHelpers.h"
#include "Networking/HTTPHelpers.h"
#include "Platform/Platform.h"
#include "Util/JSONUtils.h"
#include "Util/RegexUtils.h"
#include "Log.h"
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
			LogException(MH_SOURCE_LOCATION_CURRENT(), e,
				"Failed to gather names matching {}.*.json in {}", basename, cfg);
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
	if (!try_get_to_noinit(json, schema, "$schema"))
		throw std::runtime_error("JSON missing $schema property");

	config.ValidateSchema(schema);

	return schema;
}

static bool TryAutoUpdate(std::filesystem::path filename, const nlohmann::json& existingJson,
	SharedConfigFileBase& config, const HTTPClient& client)
{
	try
	{
		filename = std::filesystem::absolute(filename);
	}
	catch (const std::exception& e)
	{
		LogException(MH_SOURCE_LOCATION_CURRENT(), e, "Failed to convert {} to absolute path", filename);
		return false;
	}

	auto fileInfoJson = existingJson.find("file_info");
	if (fileInfoJson == existingJson.end())
	{
		DebugLog("Skipping auto-update of {}: file_info object missing", filename);
		return false;
	}

	const ConfigFileInfo info(*fileInfoJson);
	if (info.m_UpdateURL.empty())
	{
		DebugLog("Skipping auto-update of {}: update_url was empty", filename);
		return false;
	}

	nlohmann::json newJson;
	try
	{
		newJson = nlohmann::json::parse(client.GetString(info.m_UpdateURL));
	}
	catch (const std::exception& e)
	{
		LogException(MH_SOURCE_LOCATION_CURRENT(), e,
			"Failed to auto-update {}: failed to parse new json from {}", filename, info.m_UpdateURL);
		return false;
	}

	try
	{
		LoadAndValidateSchema(config, newJson);
	}
	catch (const std::exception& e)
	{
		LogException(MH_SOURCE_LOCATION_CURRENT(), e,
			"Failed to auto-update {} from {}: new json failed schema validation", filename, info.m_UpdateURL);
		return false;
	}

	ConfigFileInfo fileInfo;

	try
	{
		try_get_to_defaulted(newJson, fileInfo, "file_info");
	}
	catch (const std::exception& e)
	{
		LogException(MH_SOURCE_LOCATION_CURRENT(), e,
			"Failed to auto-update {} from {}: failed to parse file info from new json", filename, info.m_UpdateURL);
		return false;
	}

	if (fileInfo.m_Title.empty())
		fileInfo.m_Title = filename.string();

	try
	{
		config.Deserialize(newJson);
	}
	catch (const std::exception& e)
	{
		LogException(MH_SOURCE_LOCATION_CURRENT(), e,
			"Failed to auto-update {}: failed to deserialize response from {}", filename, info.m_UpdateURL);
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
	if (!try_get_to_noinit(j, d.m_Authors, "authors"))
		throw std::runtime_error("Failed to parse list of authors");
	if (!try_get_to_noinit(j, d.m_Title, "title"))
		throw std::runtime_error("Missing required property \"title\"");

	try_get_to_defaulted(j, d.m_Description, "description");
	try_get_to_defaulted(j, d.m_UpdateURL, "update_url");
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
			DebugLog("Failed to open {}", filename);
			return false;
		}

		Log("Loading {}...", filename);

		try
		{
			file >> json;
		}
		catch (const std::exception& e)
		{
			LogException(MH_SOURCE_LOCATION_CURRENT(), e, "Exception when parsing JSON from {}", filename);
			return false;
		}
	}

	try
	{
		LoadAndValidateSchema(*this, json);
	}
	catch (const std::exception& e)
	{
		LogException(MH_SOURCE_LOCATION_CURRENT(), e,
			"Failed to load {}, existing json failed schema validation", filename);
		return false;
	}

	m_FileName = filename.string();

	bool fileInfoParsed = false;
	if (auto shared = dynamic_cast<SharedConfigFileBase*>(this))
	{
		try
		{
			if (try_get_to_defaulted(json, shared->m_FileInfo, "file_info"))
				fileInfoParsed = true;
		}
		catch (const std::exception& e)
		{
			LogException(MH_SOURCE_LOCATION_CURRENT(), e,
				"Skipping auto-update for {}, failed to parse file_info", filename);
		}
	}

	if (client)
	{
		if (auto shared = dynamic_cast<SharedConfigFileBase*>(this))
		{
			if (fileInfoParsed && TryAutoUpdate(filename, json, *shared, *client))
				return true;
		}
	}
	else
	{
		DebugLog("Skipping auto-update for {} because allowAutoupdate = false.", filename);
	}

	try
	{
		Deserialize(json);
	}
	catch (const std::exception& e)
	{
		LogException(MH_SOURCE_LOCATION_CURRENT(), e,
			"Failed to load {}, existing file failed to deserialize, and auto-update did not occur", filename);
		return false;
	}

	DebugLog("Loaded {} in {} seconds", filename, to_seconds(clock_t::now() - startTime));
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
		LogException(MH_SOURCE_LOCATION_CURRENT(), e,
			"Failed to serialize {}, schema failed to validate", filename);
		return false;
	}

	try
	{
		Serialize(json);
	}
	catch (const std::exception& e)
	{
		LogException(MH_SOURCE_LOCATION_CURRENT(), e, "Failed to serialize {}", filename);
		return false;
	}

	try
	{
		LoadAndValidateSchema(*this, json);
	}
	catch (const std::exception& e)
	{
		LogException(MH_SOURCE_LOCATION_CURRENT(), e, "Failed to serialize {}", filename);
		return false;
	}

	try
	{
		SaveJSONToFile(filename, json);
	}
	catch (const std::exception& e)
	{
		LogException(MH_SOURCE_LOCATION_CURRENT(), e, "Failed to write {}", filename);
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
}

void SharedConfigFileBase::Serialize(nlohmann::json& json) const
{
	ConfigFileBase::Serialize(json);

	if (m_FileInfo)
		json["file_info"] = *m_FileInfo;
}

const std::string& SharedConfigFileBase::GetName() const
{
	if (m_FileInfo && !m_FileInfo->m_Title.empty())
		return m_FileInfo->m_Title;
	else
		return m_FileName;
}

ConfigFileInfo SharedConfigFileBase::GetFileInfo() const
{
	if (m_FileInfo)
		return *m_FileInfo;

	ConfigFileInfo retVal;
	retVal.m_Title = m_FileName;
	return retVal;
}
