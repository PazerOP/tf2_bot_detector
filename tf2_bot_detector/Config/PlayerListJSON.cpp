#include "PlayerListJSON.h"
#include "Log.h"
#include "Settings.h"

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
	SaveFile();
}

bool PlayerListJSON::LoadFile(const std::filesystem::path& filename, PlayerMap_t& map) const
{
	Log("Loading player list from "s << filename);

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

	for (const auto& player : json.at("players"))
	{
		const SteamID steamID(player.at("steamid").get<std::string>());
		PlayerListData parsed(steamID);
		player.get_to(parsed);
		map.emplace(steamID, std::move(parsed));
	}

	return true;
}

bool PlayerListJSON::IsOfficial() const
{
	// I am magic. I get to mess with playerlist.official.json, while
	// mere mortals have to use playerlist.json.
	return m_Settings->m_LocalSteamID.IsPazer();
}

auto PlayerListJSON::GetMutableList() -> PlayerMap_t&
{
	if (IsOfficial())
		return m_OfficialPlayerList;

	if (!m_UserPlayerList)
		m_UserPlayerList.emplace();

	return m_UserPlayerList.value();
}

auto PlayerListJSON::GetMutableList() const -> const PlayerMap_t*
{
	if (IsOfficial())
		return &m_OfficialPlayerList;

	if (!m_UserPlayerList)
		return nullptr;

	return &m_UserPlayerList.value();
}

bool PlayerListJSON::LoadFiles()
{
	if (!IsOfficial())
	{
		if (!m_UserPlayerList.has_value())
			m_UserPlayerList.emplace();

		LoadFile("cfg/playerlist.json", m_UserPlayerList.value());
	}

	m_OfficialPlayerList.clear();
	LoadFile("cfg/playerlist.official.json", m_OfficialPlayerList);

	m_OtherPlayerLists.clear();
	if (std::filesystem::is_directory("cfg"))
	{
		try
		{
			for (const auto& file : std::filesystem::directory_iterator("cfg",
				std::filesystem::directory_options::follow_directory_symlink | std::filesystem::directory_options::skip_permission_denied))
			{
				static const std::regex s_PlayerListRegex(R"regex(playerlist\.(.*\.)?json)regex", std::regex::optimize);
				const auto path = file.path();
				const auto filename = path.filename().string();
				if (mh::case_insensitive_compare(filename, "playerlist.json"sv) || mh::case_insensitive_compare(filename, "playerlist.official.json"sv))
					continue;

				if (std::regex_match(filename.begin(), filename.end(), s_PlayerListRegex))
				{
					LoadFile(path, m_OtherPlayerLists);
				}
			}
		}
		catch (const std::filesystem::filesystem_error& e)
		{
			LogError(std::string(__FUNCTION__ ": Exception when loading playerlist.*.json files from ./cfg/: ") << e.what());
		}
	}

	return true;
}

void PlayerListJSON::SaveFile() const
{
	nlohmann::json json =
	{
		{ "$schema", "./schema/playerlist.schema.json" }
	};

	auto& players = json["players"];
	players = json.array();

	if (auto mutableList = GetMutableList())
	{
		for (const auto& pair : *mutableList)
			players.push_back(pair.second);
	}

	// Make sure we successfully serialize BEFORE we destroy our file
	auto jsonString = json.dump(1, '\t', true);
	{
		std::ofstream file(IsOfficial() ? "cfg/playerlist.official.json" : "cfg/playerlist.json");
		file << jsonString << '\n';
	}
}

const PlayerListData* PlayerListJSON::FindPlayerData(const SteamID& id) const
{
	if (m_UserPlayerList.has_value())
	{
		if (auto found = m_UserPlayerList->find(id); found != m_UserPlayerList->end())
			return &found->second;
	}
	if (auto found = m_OtherPlayerLists.find(id); found != m_OtherPlayerLists.end())
		return &found->second;
	if (auto found = m_OfficialPlayerList.find(id); found != m_OfficialPlayerList.end())
		return &found->second;

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
	if (auto found = FindPlayerAttributes(id))
		return found->HasAttribute(attribute);

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
	if (auto found = GetMutableList().find(id); found != GetMutableList().end())
		data = &found->second;
	else
	{
		auto existing = FindPlayerData(id);
		data = &GetMutableList().emplace(id, existing ? *existing : PlayerListData(id)).first->second;
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
