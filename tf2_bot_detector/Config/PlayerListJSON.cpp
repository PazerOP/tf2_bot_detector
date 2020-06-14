#include "PlayerListJSON.h"
#include "ConfigHelpers.h"
#include "HTTPHelpers.h"
#include "Log.h"
#include "Settings.h"
#include "JSONHelpers.h"

#include <mh/text/case_insensitive_string.hpp>
#include <mh/text/string_insertion.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <regex>
#include <string>

using namespace tf2_bot_detector;
using namespace std::string_literals;
using namespace std::string_view_literals;

static std::filesystem::path s_PlayerListPath("cfg/playerlist.json");

namespace tf2_bot_detector
{
	void to_json(nlohmann::json& j, const PlayerAttributes& d)
	{
		switch (d)
		{
		case PlayerAttributes::Cheater:     j = "cheater"; break;
		case PlayerAttributes::Suspicious:  j = "suspicious"; break;
		case PlayerAttributes::Exploiter:   j = "exploiter"; break;
		case PlayerAttributes::Racist:      j = "racist"; break;

		default:
			throw std::runtime_error("Unknown PlayerAttributes value "s << +std::underlying_type_t<PlayerAttributes>(d));
		}
	}

	void to_json(nlohmann::json& j, const PlayerAttributesList& d)
	{
		if (!j.is_array())
			j = nlohmann::json::array();
		else
			j.clear();

		using ut = std::underlying_type_t<PlayerAttributes>;
		for (ut i = 0; i < ut(PlayerAttributes::COUNT); i++)
		{
			if (d.HasAttribute(PlayerAttributes(i)))
				j.push_back(PlayerAttributes(i));
		}
	}
	void to_json(nlohmann::json& j, const PlayerListData::LastSeen& d)
	{
		if (!d.m_PlayerName.empty())
			j["player_name"] = d.m_PlayerName;

		j["time"] = std::chrono::duration_cast<std::chrono::seconds>(d.m_Time.time_since_epoch()).count();
	}
	void to_json(nlohmann::json& j, const PlayerListData& d)
	{
		j = nlohmann::json
		{
			{ "steamid", d.GetSteamID() },
			{ "attributes", d.m_Attributes }
		};

		if (d.m_LastSeen)
			j["last_seen"] = *d.m_LastSeen;
	}

	void from_json(const nlohmann::json& j, PlayerAttributes& d)
	{
		const auto& str = j.get<std::string_view>();
		if (str == "suspicious"sv)
			d = PlayerAttributes::Suspicious;
		else if (str == "cheater"sv)
			d = PlayerAttributes::Cheater;
		else if (str == "exploiter"sv)
			d = PlayerAttributes::Exploiter;
		else if (str == "racist"sv)
			d = PlayerAttributes::Racist;
		else
			throw std::runtime_error("Unknown player attribute type "s << std::quoted(str));
	}
	void from_json(const nlohmann::json& j, PlayerAttributesList& d)
	{
		d = {};

		if (!j.is_array())
			throw std::invalid_argument("json must be an array");

		for (const auto& attribute : j)
			d.SetAttribute(attribute);
	}
	void from_json(const nlohmann::json& j, PlayerListData::LastSeen& d)
	{
		using clock = std::chrono::system_clock;
		using time_point = clock::time_point;
		using duration = clock::duration;
		using seconds = std::chrono::seconds;

		d.m_Time = clock::time_point(seconds(j.at("time").get<seconds::rep>()));
		d.m_PlayerName = j.value("player_name", "");
	}
	void from_json(const nlohmann::json& j, PlayerListData& d)
	{
		if (SteamID sid = j.at("steamid"); d.GetSteamID() != sid)
		{
			throw std::runtime_error("Mismatch between target PlayerListData SteamID ("s
				<< d.GetSteamID() << ") and json SteamID (" << sid << ')');
		}

		d.m_Attributes = j.at("attributes").get<PlayerAttributesList>();

		if (auto lastSeen = j.find("last_seen"); lastSeen != j.end())
			lastSeen->get_to(d.m_LastSeen.emplace());
	}
}

PlayerListJSON::PlayerListJSON(const Settings& settings) :
	m_Settings(&settings)
{
	// Immediately load and resave to normalize any formatting
	LoadFiles();
}

//void PlayerListJSON::ValidateSchema(const nlohmann::json& json)
//{
//	throw std::exception("Not implemented");
//
//	//const ConfigSchemaInfo schemaInfo(json);
//	//if (schemaInfo.m_Type != ConfigSchemaInfo::Type::Playerlist)
//	//	throw std::runtime_error("Schema says this is not a playerlist");
//}

auto PlayerListJSON::ParsePlayerlist(const nlohmann::json& json) -> PlayerListFile
{
	//ValidateSchema(json);

	PlayerListFile retVal;
	retVal.m_FileInfo = json;

	PlayerMap_t& map = retVal.m_Players;
	for (const auto& player : json.at("players"))
	{
		const SteamID steamID(player.at("steamid").get<std::string>());
		PlayerListData parsed(steamID);
		player.get_to(parsed);
		map.emplace(steamID, std::move(parsed));
	}

	return retVal;
}

struct PlayerListJSON::FileHandler : IConfigFileHandler
{
	FileHandler(PlayerListFile& file) : m_File(&file) {}
	PlayerListFile* m_File = nullptr;

	void ValidateSchema(const std::string_view& schema) const override
	{
		const ConfigSchemaInfo schemaInfo(schema);
		if (schemaInfo.m_Type != ConfigSchemaInfo::Type::Playerlist)
			throw std::runtime_error("Schema is not a playerlist");
	}

	bool Deserialize(const nlohmann::json& json) override
	{
		*m_File = {};

		if (auto found = json.find("file_info"); found != json.end())
		{
			m_File->m_FileInfo = *found;
		}
		else
		{
			m_File->m_FileInfo.m_Authors.push_back("Me!");
			m_File->m_FileInfo.m_Title = "My List of Morons";
		}

		PlayerMap_t& map = m_File->m_Players;
		for (const auto& player : json.at("players"))
		{
			const SteamID steamID(player.at("steamid").get<std::string>());
			PlayerListData parsed(steamID);
			player.get_to(parsed);
			map.emplace(steamID, std::move(parsed));
		}

		return true;
	}

	void Serialize(nlohmann::json& json) const override
	{
		json =
		{
			{ "$schema", "https://raw.githubusercontent.com/PazerOP/tf2_bot_detector/master/schemas/v3/playerlist.schema.json" },
			{ "file_info", m_File->m_FileInfo },
		};

		auto& players = json["players"];
		players = json.array();

		for (const auto& pair : m_File->m_Players)
		{
			if (pair.second.m_Attributes.empty())
				continue;

			players.push_back(pair.second);
		}
	}
};

auto PlayerListJSON::LoadFile(const std::filesystem::path& filename, bool autoUpdate) -> AsyncObject<PlayerListFile>
{
	auto filenameCopy = filename;
	return std::async([filenameCopy, autoUpdate]
		{
			PlayerListFile file;
			FileHandler handler(file);
			if (LoadConfigFile(filenameCopy, handler, autoUpdate))
				return file;

			return PlayerListFile{};
		});
}

bool PlayerListJSON::IsOfficial() const
{
	// I am magic. I get to mess with playerlist.official.json, while
	// mere mortals have to use playerlist.json.
	return m_Settings->m_LocalSteamID.IsPazer();
}

auto PlayerListJSON::GetMutableList() -> PlayerListFile&
{
	if (IsOfficial())
		return *m_OfficialPlayerList;

	if (!m_UserPlayerList)
		m_UserPlayerList.emplace();

	return *m_UserPlayerList;
}

auto PlayerListJSON::GetMutableList() const -> const PlayerListFile*
{
	if (IsOfficial())
		return &m_OfficialPlayerList.get();

	if (!m_UserPlayerList)
		return nullptr;

	return &m_UserPlayerList.value();
}

bool PlayerListJSON::LoadFiles()
{
	const auto paths = GetConfigFilePaths("playerlist");

	if (!IsOfficial() && !paths.m_User.empty())
		m_UserPlayerList = *LoadFile(paths.m_User, false);

	if (!paths.m_Official.empty())
		m_OfficialPlayerList = LoadFile(paths.m_Official, true);
	else
		m_OfficialPlayerList = {};

	m_OtherPlayerLists = std::async([paths]
		{
			PlayerMap_t map;

			for (const auto& file : paths.m_Others)
			{
				try
				{
					for (auto& otherEntry : LoadFile(file, true)->m_Players)
					{
						auto& newEntry = map.emplace(otherEntry.first, otherEntry.first).first->second;
						newEntry.m_Attributes |= otherEntry.second.m_Attributes;
						if (otherEntry.second.m_LastSeen > newEntry.m_LastSeen)
							newEntry.m_LastSeen = otherEntry.second.m_LastSeen;
					}
				}
				catch (const std::exception& e)
				{
					LogError("Exception when loading "s << file << ": " << e.what());
				}
			}

			return map;
		});

	return true;
}

void PlayerListJSON::SaveFile() const
{
	auto mutableList = GetMutableList();
	if (!mutableList)
		return; // Nothing to save

	// FIXME this is stupid
	auto mutableListCopyFIXME = *mutableList;
	FileHandler handler(mutableListCopyFIXME);
	SaveConfigFile(IsOfficial() ? "cfg/playerlist.official.json" : "cfg/playerlist.json", handler);
}

const PlayerListData* PlayerListJSON::FindPlayerData(const SteamID& id) const
{
	if (m_UserPlayerList.has_value())
	{
		if (auto found = m_UserPlayerList->m_Players.find(id); found != m_UserPlayerList->m_Players.end())
			return &found->second;
	}

	if (m_OtherPlayerLists.is_ready())
	{
		if (auto found = m_OtherPlayerLists->find(id); found != m_OtherPlayerLists->end())
			return &found->second;
	}
	if (m_OfficialPlayerList.is_ready())
	{
		if (auto found = m_OfficialPlayerList->m_Players.find(id); found != m_OfficialPlayerList->m_Players.end())
			return &found->second;
	}

	return nullptr;
}

const PlayerAttributesList* PlayerListJSON::FindPlayerAttributes(const SteamID& id) const
{
	if (auto found = FindPlayerData(id))
		return &found->m_Attributes;

	return nullptr;
}

bool PlayerListJSON::HasPlayerAttribute(const SteamID& id, PlayerAttributes attribute) const
{
	return HasPlayerAttribute(id, { attribute });
}

bool PlayerListJSON::HasPlayerAttribute(const SteamID& id, const std::initializer_list<PlayerAttributes>& attributes) const
{
	if (auto found = FindPlayerAttributes(id))
	{
		for (auto attr : attributes)
		{
			if (found->HasAttribute(attr))
				return true;
		}
	}

	return false;
}

ModifyPlayerResult PlayerListJSON::ModifyPlayer(const SteamID& id,
	ModifyPlayerAction(*func)(PlayerListData& data, void* userData), void* userData)
{
	return ModifyPlayer(id, reinterpret_cast<ModifyPlayerAction(*)(PlayerListData&, const void*)>(func),
		static_cast<const void*>(userData));
}

ModifyPlayerResult PlayerListJSON::ModifyPlayer(const SteamID& id,
	ModifyPlayerAction(*func)(PlayerListData& data, const void* userData), const void* userData)
{
	PlayerListData* data = nullptr;
	if (auto found = GetMutableList().m_Players.find(id); found != GetMutableList().m_Players.end())
	{
		data = &found->second;
	}
	else
	{
		auto existing = FindPlayerData(id);
		data = &GetMutableList().m_Players.emplace(id, existing ? *existing : PlayerListData(id)).first->second;
	}

	const auto action = func(*data, userData);
	if (action == ModifyPlayerAction::Modified)
	{
		SaveFile();
		return ModifyPlayerResult::FileSaved;
	}
	else if (action == ModifyPlayerAction::NoChanges)
	{
		return ModifyPlayerResult::NoChanges;
	}
	else
	{
		throw std::runtime_error("Unexpected ModifyPlayerAction "s << +std::underlying_type_t<ModifyPlayerAction>(action));
	}
}

PlayerListData::PlayerListData(const SteamID& id) :
	m_SteamID(id)
{
}

bool PlayerAttributesList::HasAttribute(PlayerAttributes attribute) const
{
	switch (attribute)
	{
	case PlayerAttributes::Cheater:     return m_Cheater;
	case PlayerAttributes::Suspicious:  return m_Suspicious;
	case PlayerAttributes::Exploiter:   return m_Exploiter;
	case PlayerAttributes::Racist:      return m_Racist;
	}

	throw std::runtime_error("Unknown PlayerAttributes value "s << +std::underlying_type_t<PlayerAttributes>(attribute));
}

bool PlayerAttributesList::SetAttribute(PlayerAttributes attribute, bool set)
{
#undef HELPER
#define HELPER(value) \
	{ \
		auto old = (value); \
		(value) = set; \
		return old != (value); \
	}

	switch (attribute)
	{
	case PlayerAttributes::Cheater:     HELPER(m_Cheater);
	case PlayerAttributes::Suspicious:  HELPER(m_Suspicious);
	case PlayerAttributes::Exploiter:   HELPER(m_Exploiter);
	case PlayerAttributes::Racist:      HELPER(m_Racist);

	default:
		throw std::runtime_error("Unknown PlayerAttributes value "s << +std::underlying_type_t<PlayerAttributes>(attribute));
	}
}

bool PlayerAttributesList::empty() const
{
	return !m_Cheater && !m_Suspicious && !m_Exploiter && !m_Racist;
}

PlayerAttributesList& PlayerAttributesList::operator|=(const PlayerAttributesList& other)
{
	m_Cheater = m_Cheater || other.m_Cheater;
	m_Suspicious = m_Suspicious || other.m_Suspicious;
	m_Exploiter = m_Exploiter || other.m_Exploiter;
	m_Racist = m_Racist || other.m_Racist;

	return *this;
}
