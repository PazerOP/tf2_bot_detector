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
		ConfigSchemaInfo(std::string type, unsigned version, std::string branch = "master");

		std::string m_Branch;
		std::string m_Type;
		unsigned m_Version{};
	};

	void to_json(nlohmann::json& j, const ConfigSchemaInfo& d);
	void from_json(const nlohmann::json& j, ConfigSchemaInfo& d);

	struct ConfigFileInfo
	{
		std::vector<std::string> m_Authors;
		std::string m_Title;
		std::string m_Description;
		std::string m_UpdateURL;
	};

	void to_json(nlohmann::json& j, const ConfigFileInfo& d);
	void from_json(const nlohmann::json& j, ConfigFileInfo& d);

	class ConfigFileBase
	{
	public:
		virtual ~ConfigFileBase() = default;

		bool LoadFile(const std::filesystem::path& filename);
		bool SaveFile(const std::filesystem::path& filename) const;

		virtual void ValidateSchema(const ConfigSchemaInfo& schema) const = 0;
		virtual void Deserialize(const nlohmann::json& json) = 0 {}
		virtual void Serialize(nlohmann::json& json) const = 0;

		ConfigSchemaInfo m_Schema;
	};

	class SharedConfigFileBase : public ConfigFileBase
	{
	public:
		void Deserialize(const nlohmann::json& json) override = 0;
		void Serialize(nlohmann::json& json) const override = 0;

		std::optional<ConfigFileInfo> m_FileInfo;
	};
}

template<typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const tf2_bot_detector::ConfigSchemaInfo& info)
{
	return os << "https://raw.githubusercontent.com/PazerOP/tf2_bot_detector/"
		<< info.m_Branch << "/schemas/v"
		<< info.m_Version << '/'
		<< info.m_Type << ".schema.json";
}
