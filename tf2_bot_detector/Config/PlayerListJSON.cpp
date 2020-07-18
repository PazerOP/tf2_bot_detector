#include "PlayerListJSON.h"
#include "ConfigHelpers.h"
#include "Networking/HTTPHelpers.h"
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
	void to_json(nlohmann::json& j, const PlayerAttribute& d)
	{
		switch (d)
		{
		case PlayerAttribute::Cheater:     j = "cheater"; break;
		case PlayerAttribute::Suspicious:  j = "suspicious"; break;
		case PlayerAttribute::Exploiter:   j = "exploiter"; break;
		case PlayerAttribute::Racist:      j = "racist"; break;

		default:
			throw std::runtime_error("Unknown PlayerAttribute value "s << +std::underlying_type_t<PlayerAttribute>(d));
		}
	}

	void to_json(nlohmann::json& j, const PlayerAttributesList& d)
	{
		if (!j.is_array())
			j = nlohmann::json::array();
		else
			j.clear();

		using ut = std::underlying_type_t<PlayerAttribute>;
		for (ut i = 0; i < ut(PlayerAttribute::COUNT); i++)
		{
			if (d.HasAttribute(PlayerAttribute(i)))
				j.push_back(PlayerAttribute(i));
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

		if (!d.m_Proof.empty())
			j["proof"] = d.m_Proof;
	}

	void from_json(const nlohmann::json& j, PlayerAttribute& d)
	{
		const auto& str = j.get<std::string_view>();
		if (str == "suspicious"sv)
			d = PlayerAttribute::Suspicious;
		else if (str == "cheater"sv)
			d = PlayerAttribute::Cheater;
		else if (str == "exploiter"sv)
			d = PlayerAttribute::Exploiter;
		else if (str == "racist"sv)
			d = PlayerAttribute::Racist;
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

		try_get_to_defaulted(j, d.m_Proof, "proof");
	}
}

PlayerListJSON::PlayerListJSON(const Settings& settings) :
	m_Settings(&settings),
	m_CFGGroup(settings)
{
	// Immediately load and resave to normalize any formatting
	LoadFiles();
}

void PlayerListJSON::PlayerListFile::ValidateSchema(const ConfigSchemaInfo& schema) const
{
	if (schema.m_Type != "playerlist")
		throw std::runtime_error("Schema "s << std::quoted(schema.m_Type) << " is not a playerlist");
	if (schema.m_Version != 3)
		throw std::runtime_error("Schema must be version 3 (current version "s << schema.m_Version << ')');
}

void PlayerListJSON::PlayerListFile::Deserialize(const nlohmann::json& json)
{
	SharedConfigFileBase::Deserialize(json);

	PlayerMap_t& map = m_Players;
	for (const auto& player : json.at("players"))
	{
		const SteamID steamID = player.at("steamid");
		PlayerListData parsed(steamID);
		player.get_to(parsed);
		map.emplace(steamID, std::move(parsed));
	}
}

void PlayerListJSON::PlayerListFile::Serialize(nlohmann::json& json) const
{
	SharedConfigFileBase::Serialize(json);

	if (!m_Schema || m_Schema->m_Version != PLAYERLIST_SCHEMA_VERSION)
		json["$schema"] = ConfigSchemaInfo("playerlist", PLAYERLIST_SCHEMA_VERSION);

	auto& players = json["players"];
	players = json.array();

	for (const auto& pair : m_Players)
	{
		if (pair.second.m_Attributes.empty())
			continue;

		players.push_back(pair.second);
	}
}

bool PlayerListJSON::LoadFiles()
{
	m_CFGGroup.LoadFiles();

	return true;
}

void PlayerListJSON::SaveFile() const
{
	m_CFGGroup.SaveFile();
}

cppcoro::generator<std::pair<const ConfigFileInfo&, const PlayerListData&>>
PlayerListJSON::FindPlayerData(const SteamID& id) const
{
	if (m_CFGGroup.m_UserList.has_value())
	{
		if (auto found = m_CFGGroup.m_UserList->m_Players.find(id);
			found != m_CFGGroup.m_UserList->m_Players.end())
		{
			co_yield { m_CFGGroup.m_UserList->m_FileInfo, found->second };
		}
	}
	if (mh::is_future_ready(m_CFGGroup.m_ThirdPartyLists))
	{
		for (auto& file : m_CFGGroup.m_ThirdPartyLists.get())
		{
			if (auto found = file.second.find(id); found != file.second.end())
				co_yield { file.first, found->second };
		}

	}
	if (mh::is_future_ready(m_CFGGroup.m_OfficialList))
	{
		if (auto found = m_CFGGroup.m_OfficialList.get().m_Players.find(id);
			found != m_CFGGroup.m_OfficialList.get().m_Players.end())
		{
			co_yield { m_CFGGroup.m_OfficialList.get().m_FileInfo, found->second };
		}
	}
}

cppcoro::generator<std::pair<const ConfigFileInfo&, const PlayerAttributesList&>>
PlayerListJSON::FindPlayerAttributes(const SteamID& id) const
{
	for (auto& [file, found] : FindPlayerData(id))
		co_yield { file, found.m_Attributes };
}

PlayerMarks PlayerListJSON::GetPlayerAttributes(const SteamID& id) const
{
	if (id == m_Settings->GetLocalSteamID())
		return {};

	PlayerMarks marks;
	for (const auto& [file, found] : FindPlayerAttributes(id))
	{
		if (found)
			marks.m_Marks.push_back({ found, file });
	}

	return marks;
}

PlayerMarks PlayerListJSON::HasPlayerAttributes(const SteamID& id, const PlayerAttributesList& attributes) const
{
	if (id == m_Settings->GetLocalSteamID())
		return {};

	PlayerMarks marks;
	for (const auto& [file, found] : FindPlayerAttributes(id))
	{
		auto attr = found & attributes;
		if (attr)
			marks.m_Marks.push_back({ attr, file });
	}

	return marks;
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
	if (id == m_Settings->GetLocalSteamID())
	{
		LogWarning("Attempted to modify player attributes for local SteamID "s << id);
		return ModifyPlayerResult::NoChanges;
	}

	PlayerListData* data = nullptr;
	auto& mutableList = m_CFGGroup.GetMutableList();
	if (auto found = mutableList.m_Players.find(id); found != mutableList.m_Players.end())
		data = &found->second;
	else
		data = &mutableList.m_Players.emplace(id, PlayerListData(id)).first->second;

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

PlayerListData::~PlayerListData()
{
}

PlayerAttributesList::PlayerAttributesList(const std::initializer_list<PlayerAttribute>& attributes)
{
	for (const auto& attr : attributes)
		SetAttribute(attr);
}

PlayerAttributesList::PlayerAttributesList(PlayerAttribute attribute)
{
	SetAttribute(attribute);
}

bool PlayerAttributesList::SetAttribute(PlayerAttribute attribute, bool set)
{
#undef HELPER
#define HELPER(value) \
	do \
	{ \
		auto old = (value); \
		(value) = set; \
		return old != (value); \
	} while(false)

	const auto ApplyChange = [&]
	{
		auto old = HasAttribute(attribute);
		m_Bits.set(size_t(attribute), set);
		return old != set;
	};

	switch (attribute)
	{
	case PlayerAttribute::Cheater:
	{
		auto retVal = SetAttribute(PlayerAttribute::Suspicious, false);
		retVal |= ApplyChange();
		return retVal;
	}

	case PlayerAttribute::Suspicious:
		return !HasAttribute(PlayerAttribute::Cheater) ? ApplyChange() : false;

	default:
		return ApplyChange();
	}
}

void PlayerListJSON::ConfigFileGroup::CombineEntries(BaseClass::collection_type& map, const PlayerListFile& file) const
{
	map.push_back({ file.m_FileInfo, file.m_Players });
}

bool PlayerMarks::Has(const PlayerAttributesList& attr) const
{
	for (const auto& mark : m_Marks)
	{
		if (mark.m_Attributes & attr)
			return true;
	}

	return false;
}
