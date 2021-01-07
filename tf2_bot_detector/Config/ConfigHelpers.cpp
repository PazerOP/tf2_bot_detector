#include "ConfigHelpers.h"
#include "Networking/HTTPClient.h"
#include "Networking/HTTPHelpers.h"
#include "Platform/Platform.h"
#include "Util/JSONUtils.h"
#include "Util/RegexUtils.h"
#include "Filesystem.h"
#include "Log.h"
#include "Version.h"
#include "Settings.h"

#include <mh/text/formatters/error_code.hpp>
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

	if (auto path = mh::format("cfg/{}.official.json", basename); IFilesystem::Get().Exists(path))
		retVal.m_Official = path;
	if (auto path = mh::format("cfg/{}.json", basename); IFilesystem::Get().Exists(path))
		retVal.m_User = path;

	if (const auto cfg = IFilesystem::Get().GetMutableDataDir() / std::filesystem::path("cfg");
		std::filesystem::is_directory(cfg))
	{
		try
		{
			const std::regex filenameRegex(mh::format("{}{}", basename, R"regex(\.(?!official).*\.json)regex"),
				std::regex::optimize | std::regex::icase);

			for (const auto& file : std::filesystem::directory_iterator(cfg,
				std::filesystem::directory_options::follow_directory_symlink | std::filesystem::directory_options::skip_permission_denied))
			{
				const auto path = file.path();
				const auto filename = path.filename().string();
				if (std::regex_match(filename.begin(), filename.end(), filenameRegex))
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
	IFilesystem::Get().WriteFile(filename, json.dump(1, '\t', true, nlohmann::detail::error_handler_t::ignore) << '\n');
}

static ConfigSchemaInfo LoadAndValidateSchema(const ConfigFileBase& config, const nlohmann::json& json)
{
	ConfigSchemaInfo schema(nullptr);
	if (!try_get_to_noinit(json, schema, "$schema"))
		throw std::runtime_error("JSON missing $schema property");

	config.ValidateSchema(schema);

	return schema;
}

static mh::task<bool> TryAutoUpdate(std::filesystem::path filename, const nlohmann::json& existingJson,
	SharedConfigFileBase& config, const HTTPClient& client)
{
	auto fileInfoJson = existingJson.find("file_info");
	if (fileInfoJson == existingJson.end())
	{
		DebugLog("Skipping auto-update of {}: file_info object missing", filename);
		co_return false;
	}

	const ConfigFileInfo info(*fileInfoJson);
	if (info.m_UpdateURL.empty())
	{
		DebugLog("Skipping auto-update of {}: update_url was empty", filename);
		co_return false;
	}

	nlohmann::json newJson;
	try
	{
		newJson = nlohmann::json::parse(co_await client.GetStringAsync(info.m_UpdateURL));
	}
	catch (...)
	{
		LogException(MH_SOURCE_LOCATION_CURRENT(),
			"Failed to auto-update {}: failed to parse new json from {}", filename, info.m_UpdateURL);
		co_return false;
	}

	try
	{
		LoadAndValidateSchema(config, newJson);
	}
	catch (...)
	{
		LogException(MH_SOURCE_LOCATION_CURRENT(),
			"Failed to auto-update {} from {}: new json failed schema validation", filename, info.m_UpdateURL);
		co_return false;
	}

	ConfigFileInfo fileInfo;

	try
	{
		try_get_to_defaulted(newJson, fileInfo, "file_info");
	}
	catch (...)
	{
		LogException(MH_SOURCE_LOCATION_CURRENT(),
			"Failed to auto-update {} from {}: failed to parse file info from new json", filename, info.m_UpdateURL);
		co_return false;
	}

	if (fileInfo.m_Title.empty())
		fileInfo.m_Title = filename.string();

	try
	{
		config.Deserialize(newJson);
	}
	catch (...)
	{
		LogException(MH_SOURCE_LOCATION_CURRENT(),
			"Failed to auto-update {}: failed to deserialize response from {}", filename, info.m_UpdateURL);
		co_return false;
	}

	if (config.SaveFile(filename))
	{
		LogError(MH_SOURCE_LOCATION_CURRENT(), "Successfully downloaded and deserialized new version of {} from {}, but couldn't write it back to disk.",
			filename, info.m_UpdateURL);
	}
	else
	{
		DebugLog(MH_SOURCE_LOCATION_CURRENT(), "Wrote auto-updated config file from {} to {}", info.m_UpdateURL, filename);
	}

	co_return true;
}

void tf2_bot_detector::to_json(nlohmann::json& j, const ConfigSchemaInfo& d)
{
	j = mh::format("{}", d);
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

mh::task<std::error_condition> tf2_bot_detector::detail::LoadConfigFileAsync(ConfigFileBase& file, std::filesystem::path filename,
	bool allowAutoUpdate, const Settings& settings)
{
	std::shared_ptr<const HTTPClient> client;
	if (dynamic_cast<SharedConfigFileBase*>(&file) && allowAutoUpdate)
	{
		client = settings.GetHTTPClient();
		if (!client)
			Log("Disallowing auto-update of {} because internet connectivity is disabled or unset in settings", filename);
	}

	co_return co_await file.LoadFileAsync(filename, client);
}

static void SaveConfigFileBackup(const std::filesystem::path& filename) noexcept try
{
	auto& fs = IFilesystem::Get();
	const auto readPath = fs.ResolvePath(filename, PathUsage::Read);

	const auto extension = filename.extension();
	const auto baseFilename = filename.filename().replace_extension();

	std::filesystem::path backupPath;
	for (size_t i = 1; ; i++)
	{
		backupPath = fs.ResolvePath(filename, PathUsage::Write).remove_filename();
		backupPath /= mh::format("{}_BACKUP_{}{}", baseFilename.string(), i, extension.string());

		if (!std::filesystem::exists(backupPath))
		{
			Log(MH_SOURCE_LOCATION_CURRENT(), "Attempting to make backup of {} to {}...", filename, backupPath);
			std::filesystem::copy_file(filename, backupPath);
			break;
		}
	}

	LogWarning(MH_SOURCE_LOCATION_CURRENT(), "A backup of {0} was saved to {1} before {0} was overwritten.", filename, backupPath);
}
catch (...)
{
	LogFatalException(MH_SOURCE_LOCATION_CURRENT(), "Loading config file {} failed, and TF2 Bot Detector was unable to make a backup before overwriting it.",
		filename);
}

mh::task<std::error_condition> ConfigFileBase::LoadFileAsync(const std::filesystem::path& filename, std::shared_ptr<const HTTPClient> client)
{
	const auto loadResult = co_await LoadFileInternalAsync(filename, client);

	if (loadResult && loadResult != std::errc::no_such_file_or_directory)
		SaveConfigFileBackup(filename);

	if (auto saveResult = SaveFile(filename))
	{
		if (loadResult)
		{
			LogFatalError(MH_SOURCE_LOCATION_CURRENT(), "Failed to load and resave {}. TF2 Bot Detector may not have permission to write to where it is installed.\n\nLoad error: {}\nSave error: {}", filename, loadResult, saveResult);
		}
		else
		{
			LogWarning(MH_SOURCE_LOCATION_CURRENT(), "Failed to resave {}", filename);
		}
	}

	co_return loadResult;
}

mh::task<std::error_condition> ConfigFileBase::LoadFileInternalAsync(std::filesystem::path filename, std::shared_ptr<const HTTPClient> client)
{
	try
	{
		if (!IFilesystem::Get().Exists(filename))
		{
			DebugLog("{} does not exist, not loading.", filename);
			co_return std::errc::no_such_file_or_directory;
		}
	}
	catch (...)
	{
		LogException("Failed to check if {} exists", filename);
		co_return ConfigErrorType::ReadFileFailed;
	}

	const auto startTime = clock_t::now();

	nlohmann::json json;
	{
		Log("Loading {}...", filename);

		std::string file;
		try
		{
			file = IFilesystem::Get().ReadFile(filename);
		}
		catch (...)
		{
			LogException(MH_SOURCE_LOCATION_CURRENT(), "Failed to load {}", filename);
			co_return ConfigErrorType::ReadFileFailed;
		}

		try
		{
			json = nlohmann::json::parse(file);
		}
		catch (...)
		{
			LogException(MH_SOURCE_LOCATION_CURRENT(), "Failed to parse JSON from {}", filename);
			co_return ConfigErrorType::JSONParseFailed;
		}
	}

	try
	{
		LoadAndValidateSchema(*this, json);
	}
	catch (...)
	{
		LogException(MH_SOURCE_LOCATION_CURRENT(),
			"Failed to load {}, existing json failed schema validation", filename);
		co_return ConfigErrorType::SchemaValidationFailed;
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
		catch (...)
		{
			LogException(MH_SOURCE_LOCATION_CURRENT(),
				"Skipping auto-update for {}, failed to parse file_info", filename);
		}
	}

	if (client)
	{
		if (auto shared = dynamic_cast<SharedConfigFileBase*>(this))
		{
			if (fileInfoParsed && co_await TryAutoUpdate(filename, json, *shared, *client))
				co_return ConfigErrorType::Success;
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
	catch (...)
	{
		LogException(MH_SOURCE_LOCATION_CURRENT(),
			"Failed to load {}, existing file failed to deserialize, and auto-update did not occur", filename);
		co_return ConfigErrorType::DeserializeFailed;
	}

	DebugLog("Loaded {} in {} seconds", filename, to_seconds(clock_t::now() - startTime));
	co_return ConfigErrorType::Success;
}

std::error_condition tf2_bot_detector::ConfigFileBase::SaveFile(const std::filesystem::path& filename) const
{
	nlohmann::json json;

	// If we already have a schema loaded, put it in the $schema property
	try
	{
		if (m_Schema)
			json["$schema"] = *m_Schema;
	}
	catch (...)
	{
		LogException(MH_SOURCE_LOCATION_CURRENT(),
			"Failed to serialize {}, schema failed to validate", filename);
		return ConfigErrorType::SchemaValidationFailed;
	}

	try
	{
		Serialize(json);
	}
	catch (...)
	{
		LogException(MH_SOURCE_LOCATION_CURRENT(), "Failed to serialize {}", filename);
		return ConfigErrorType::SerializeFailed;
	}

	try
	{
		LoadAndValidateSchema(*this, json);
	}
	catch (...)
	{
		LogException(MH_SOURCE_LOCATION_CURRENT(), "Failed to serialize {}", filename);
		return ConfigErrorType::SerializedSchemaValidationFailed;
	}

	try
	{
		SaveJSONToFile(filename, json);
	}
	catch (...)
	{
		LogException(MH_SOURCE_LOCATION_CURRENT(), "Failed to write {}", filename);
		return ConfigErrorType::WriteFileFailed;
	}

	return ConfigErrorType::Success;
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

const std::error_category& tf2_bot_detector::ConfigErrorCategory()
{
	struct ConfigErrorCategory_t final : std::error_category
	{
		const char* name() const noexcept override { return "tf2_bot_detector::ConfigErrorType"; }

		std::string message(int condition) const override
		{
			switch ((ConfigErrorType)condition)
			{
			case ConfigErrorType::Success:
				return "Success";

			case ConfigErrorType::ReadFileFailed:
				return "Failed to read file";
			case ConfigErrorType::JSONParseFailed:
				return "Failed to parse JSON";
			case ConfigErrorType::SchemaValidationFailed:
				return "Failed to validate $schema";
			case ConfigErrorType::DeserializeFailed:
				return "Failed to deserialize data from json";
			case ConfigErrorType::SerializeFailed:
				return "Failed to serialize data to json";
			case ConfigErrorType::SerializedSchemaValidationFailed:
				return "Failed to validate $schema that was generated by the Serialize() function";
			case ConfigErrorType::WriteFileFailed:
				return "Failed to write file";

			default:
				assert(false);
				return "<UNKNOWN>";
			}
		}

	} static const s_Category;

	return s_Category;
}

std::error_condition tf2_bot_detector::make_error_condition(tf2_bot_detector::ConfigErrorType e)
{
	return std::error_condition(static_cast<int>(e), ConfigErrorCategory());
}
