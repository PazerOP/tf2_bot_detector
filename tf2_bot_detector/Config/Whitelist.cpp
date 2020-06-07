#include "Whitelist.h"
#include "ConfigHelpers.h"
#include "Log.h"
#include "JSONHelpers.h"

#include <mh/text/string_insertion.hpp>
#include <nlohmann/json.hpp>

#include <fstream>

using namespace std::string_literals;
using namespace tf2_bot_detector;

void PlayerWhitelist::LoadFiles()
{
	const auto files = GetConfigFilePaths("whitelist");

	if (!files.m_Official.empty())
		LoadFile(files.m_Official);
	if (!files.m_User.empty())
		LoadFile(files.m_User);
	for (const auto& file : files.m_Others)
		LoadFile(file);
}

bool PlayerWhitelist::HasPlayer(const SteamID& id) const
{
	return m_Whitelist.contains(id);
}

bool PlayerWhitelist::LoadFile(const std::filesystem::path& filename)
{
	nlohmann::json json;
	{
		std::ifstream file(filename);
		if (!file.good())
		{
			LogWarning(std::string(__FUNCTION__ ": Failed to open ") << filename);
			return false;
		}

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

	if (auto found = json.find("players"); found != json.end())
	{
		for (const auto& player : *found)
		{
			SteamID id;
			if (!try_get_to(player, "steamid", id))
			{
				LogWarning("Failed to parse SteamID from a player in "s << filename);
				continue;
			}

			Player metadata;
			if (!try_get_to(player, "nickname", metadata.m_Nickname))
			{
				LogWarning("Failed to get nickname from a player in "s << filename);
				continue;
			}

			m_Whitelist.emplace(id, std::move(metadata));
		}
	}

	return true;
}
