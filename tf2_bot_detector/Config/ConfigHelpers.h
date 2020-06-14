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
		ConfigSchemaInfo(const char* schema);
		ConfigSchemaInfo(const nlohmann::json& j);

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
		ConfigFileInfo() = default;
		ConfigFileInfo(const nlohmann::json& j);

		std::vector<std::string> m_Authors;
		std::string m_Title;
		std::string m_Description;
		std::string m_UpdateURL;
	};
}
