#include "ConfigHelpers.h"
#include "Log.h"

#include <mh/text/case_insensitive_string.hpp>
#include <mh/text/string_insertion.hpp>

#include <regex>

using namespace std::string_view_literals;

auto tf2_bot_detector::GetConfigFilePaths(const std::string_view& basename) -> ConfigFilePaths
{
	ConfigFilePaths retVal;

	if (std::filesystem::is_directory("cfg"))
	{
		try
		{
			for (const auto& file : std::filesystem::directory_iterator("cfg",
				std::filesystem::directory_options::follow_directory_symlink | std::filesystem::directory_options::skip_permission_denied))
			{
				static const std::regex s_PlayerListRegex(std::string(basename) << R"regex(\.(.*\.)?json)regex", std::regex::optimize);
				const auto path = file.path();
				const auto filename = path.filename().string();
				if (mh::case_insensitive_compare(filename, std::string(basename) << ".json"sv))
					retVal.m_User = filename;
				else if (mh::case_insensitive_compare(filename, std::string(basename) << ".official.json"sv))
					retVal.m_Official = filename;
				else if (std::regex_match(filename.begin(), filename.end(), s_PlayerListRegex))
					retVal.m_Others.push_back(filename);
			}
		}
		catch (const std::filesystem::filesystem_error& e)
		{
			LogError(std::string(__FUNCTION__ ": Exception when loading playerlist.*.json files from ./cfg/: ") << e.what());
		}
	}

	return retVal;
}
