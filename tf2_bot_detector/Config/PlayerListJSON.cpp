#include "PlayerListJSON.h"
#include "Networking/HTTPHelpers.h"
#include "Util/JSONUtils.h"
#include "ConfigHelpers.h"
#include "Log.h"
#include "Settings.h"

#include <mh/text/case_insensitive_string.hpp>
#include <mh/text/string_insertion.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
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
auto PlayerListData::LastSeen::Latest(const std::optional<LastSeen>& lhs, const std::optional<LastSeen>& rhs)
	-> std::optional<LastSeen>
{
	if (!lhs && !rhs)
		return std::nullopt;

	if (!lhs)
		return rhs;
	if (!rhs)
		return lhs;

	return lhs->m_Time < rhs->m_Time ? lhs : rhs;
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

PlayerListData& PlayerListJSON::PlayerListFile::GetOrAddPlayer(const SteamID& id)
{
	if (auto found = m_Players.find(id); found != m_Players.end())
		return found->second;
	else
		return m_Players.emplace(id, PlayerListData(id)).first->second;
}

bool PlayerListJSON::LoadFiles()
{
	m_CFGGroup.LoadFiles();

	if (m_CFGGroup.IsOfficial())
	{
		auto action = ModifyPlayerAction::NoChanges;
		for (auto& player : m_CFGGroup.GetDefaultMutableList().m_Players)
		{
			if (OnPlayerDataChanged(player.second) != ModifyPlayerAction::NoChanges)
				action = ModifyPlayerAction::Modified;
		}

		if (action != ModifyPlayerAction::NoChanges)
			m_CFGGroup.SaveFiles();
	}

	return true;
}

void PlayerListJSON::SaveFiles() const
{
	m_CFGGroup.SaveFiles();
}

auto PlayerListJSON::FindPlayerData(const SteamID& id) const ->
	mh::generator<std::pair<const ConfigFileName&, const PlayerListData&>>
{
	if (m_CFGGroup.m_UserList.has_value())
	{
		if (auto found = m_CFGGroup.m_UserList->m_Players.find(id);
			found != m_CFGGroup.m_UserList->m_Players.end())
		{
			co_yield { m_CFGGroup.m_UserList->GetName(), found->second };
		}
	}
	if (auto list = m_CFGGroup.m_ThirdPartyLists.try_get())
	{
		for (auto& file : *list)
		{
			if (auto found = file.second.find(id); found != file.second.end())
				co_yield { file.first, found->second };
		}
	}
	if (auto list = m_CFGGroup.m_OfficialList.try_get())
	{
		if (auto found = list->m_Players.find(id); found != list->m_Players.end())
		{
			co_yield { list->GetName(), found->second };
		}
	}
}

auto PlayerListJSON::FindPlayerAttributes(const SteamID& id) const ->
	mh::generator<std::pair<const ConfigFileName&, const PlayerAttributesList&>>
{
	for (auto& [fileName, found] : FindPlayerData(id))
		co_yield { fileName, found.m_Attributes };
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
	const std::function<ModifyPlayerAction(PlayerListData& data)>& func)
{
	if (id == m_Settings->GetLocalSteamID())
	{
		LogWarning("Attempted to modify player attributes for local SteamID "s << id);
		return ModifyPlayerResult::NoChanges;
	}

	PlayerListData& defaultMutableDataRef = m_CFGGroup.GetDefaultMutableList().GetOrAddPlayer(id);

	PlayerListData defaultMutableData = defaultMutableDataRef;
	if (m_CFGGroup.IsOfficial())
	{
		// Copy attributes into the official list (if we are modifying the official list)
		// Any attributes that shouldn't be in there (currently just PlayerAttribute::Suspicious)
		// will get moved back over to the local list in OnPlayerDataChanged
		const PlayerListData& localPlayer = m_CFGGroup.GetLocalList().GetOrAddPlayer(id);
		assert(&localPlayer != &defaultMutableDataRef);
		defaultMutableData.m_Attributes |= localPlayer.m_Attributes;
	}

	const auto action = func(defaultMutableData);
	if (action == ModifyPlayerAction::Modified)
	{
		OnPlayerDataChanged(defaultMutableData);
		defaultMutableDataRef = defaultMutableData;
		SaveFiles();
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

ModifyPlayerAction PlayerListJSON::OnPlayerDataChanged(PlayerListData& data)
{
	ModifyPlayerAction retVal = ModifyPlayerAction::NoChanges;

	if (m_CFGGroup.IsOfficial())
	{
		PlayerListData& localData = m_CFGGroup.GetLocalList().GetOrAddPlayer(data.GetSteamID());
		if (&data != &localData)
		{
			const auto oldSuspicious = localData.m_Attributes.HasAttribute(PlayerAttribute::Suspicious);
			const auto newSuspicious = data.m_Attributes.HasAttribute(PlayerAttribute::Suspicious);
			if (newSuspicious != oldSuspicious)
			{
				data.m_Attributes.SetAttribute(PlayerAttribute::Suspicious, false);
				localData.m_Attributes.SetAttribute(PlayerAttribute::Suspicious, newSuspicious);
				localData.m_LastSeen = PlayerListData::LastSeen::Latest(localData.m_LastSeen, data.m_LastSeen);
				retVal = ModifyPlayerAction::Modified;
			}
		}
	}

	return retVal;
}

PlayerListData::PlayerListData(const SteamID& id) :
	m_SteamID(id)
{
}

PlayerListData::~PlayerListData()
{
}

bool PlayerListData::operator==(const PlayerListData& other) const
#if _MSC_VER >= 1927
= default;
#else
{
	return
		m_SteamID == other.m_SteamID &&
		m_Attributes == other.m_Attributes &&
		m_LastSeen == other.m_LastSeen &&
		m_Proof == other.m_Proof
		;
}
#endif

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
	map.push_back({ file.GetName(), file.m_Players });
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
