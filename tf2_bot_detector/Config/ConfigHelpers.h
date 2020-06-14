#pragma once

#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <optional>
#include <vector>

namespace tf2_bot_detector
{
	enum class ConfigFileType
	{
		User,
		Official,
		ThirdParty,

		COUNT,
	};

	struct ConfigFilePaths
	{
		std::filesystem::path m_User;
		std::filesystem::path m_Official;
		std::vector<std::filesystem::path> m_Others;
	};
	ConfigFilePaths GetConfigFilePaths(const std::string_view& basename);

	struct ConfigSchemaInfo
	{
		ConfigSchemaInfo() = default;
		ConfigSchemaInfo(const std::string_view& schema);

		enum class Type
		{
			Invalid,

			Settings,
			Playerlist,
			Rules,
			Whitelist,

		} m_Type{};

		unsigned m_Version{};
	};

	struct ConfigFileInfo
	{
		std::vector<std::string> m_Authors;
		std::string m_Title;
		std::string m_Description;
		std::string m_UpdateURL;
	};

	void to_json(nlohmann::json& j, const ConfigFileInfo& d);
	void from_json(const nlohmann::json& j, ConfigFileInfo& d);

	class IConfigFileHandler
	{
	public:
		virtual ~IConfigFileHandler() = default;

		virtual void ValidateSchema(const std::string_view& schema) const = 0;
		virtual bool Deserialize(const nlohmann::json& json) = 0;
		virtual void Serialize(nlohmann::json& json) const = 0;
	};
	bool LoadConfigFile(const std::filesystem::path& filename, IConfigFileHandler& handler, bool autoUpdate);
	bool SaveConfigFile(const std::filesystem::path& filename, IConfigFileHandler& handler);
}
