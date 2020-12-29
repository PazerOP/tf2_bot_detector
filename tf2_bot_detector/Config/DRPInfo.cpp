#include "DRPInfo.h"
#include "Util/JSONUtils.h"
#include "Util/RegexUtils.h"

#include <mh/text/format.hpp>
#include <mh/text/fmtstr.hpp>
#include <mh/text/stringops.hpp>
#include <mh/text/string_insertion.hpp>
#include <nlohmann/json.hpp>

using namespace tf2_bot_detector;

DRPInfo::DRPInfo(const Settings& settings) :
	m_Settings(&settings)
{
	LoadFile();
}

void DRPInfo::LoadFile()
{
	m_DRPInfo = LoadConfigFileAsync<DRPFile>("cfg/discord_rich_presence.json", true, *m_Settings);
}

auto DRPInfo::FindMap(const std::string_view& name) const -> const Map*
{
	if (m_DRPInfo.is_ready())
	{
		try
		{
			auto& drpInfo = m_DRPInfo.get();
			for (const auto& map : drpInfo.m_Maps)
			{
				if (map.Matches(name))
					return &map;
			}
		}
		catch (...)
		{
			LogException(MH_SOURCE_LOCATION_CURRENT());
		}
	}

	return nullptr;
}

void DRPInfo::DRPFile::Deserialize(const nlohmann::json& json)
{
	BaseClass::Deserialize(json);

	auto& maps = json.at("maps");
	for (auto it = maps.begin(); it != maps.end(); ++it)
	{
		Map map;
		map.m_MapNames.push_back(it.key());

		if (auto found = it->find("map_name_aliases"); found != it->end())
		{
			for (const auto& alias : *found)
				map.m_MapNames.push_back(alias);
		}

		try_get_to_defaulted(it.value(), map.m_FriendlyNameOverride, "friendly_name_override");
		try_get_to_defaulted(it.value(), map.m_LargeImageKeyOverride, "large_image_key_override");
		m_Maps.push_back(std::move(map));
	}
}

void DRPInfo::DRPFile::Serialize(nlohmann::json& json) const
{
	BaseClass::Serialize(json);

	if (!m_Schema || m_Schema->m_Version != DRP_SCHEMA_VERSION)
		json["$schema"] = ConfigSchemaInfo("discord_rich_presence", DRP_SCHEMA_VERSION);

	for (const auto& map : m_Maps)
	{
		auto& entry = json["maps"][map.m_MapNames.at(0)] = nlohmann::json::object();
		for (size_t i = 1; i < map.m_MapNames.size(); i++)
			entry["map_name_aliases"].push_back(map.m_MapNames[i]);

		if (!map.m_FriendlyNameOverride.empty())
			entry["friendly_name_override"] = map.m_FriendlyNameOverride;
		if (!map.m_LargeImageKeyOverride.empty())
			entry["large_image_key_override"] = map.m_LargeImageKeyOverride;
	}
}

void DRPInfo::DRPFile::ValidateSchema(const ConfigSchemaInfo& schema) const
{
	BaseClass::ValidateSchema(schema);

	if (schema.m_Type != "discord_rich_presence")
		throw std::runtime_error(mh::format("Schema {} is not a sponsors list", std::quoted(schema.m_Type)));

	if (schema.m_Version != DRP_SCHEMA_VERSION)
	{
		throw std::runtime_error(mh::format("DRP schema must be version {}, but was {}",
			DRP_SCHEMA_VERSION, schema.m_Version));
	}
}

std::string DRPInfo::Map::GetLargeImageKey() const
{
	if (!m_LargeImageKeyOverride.empty())
		return m_LargeImageKeyOverride;

	return mh::format("map_{}", m_MapNames.at(0));
}

std::string DRPInfo::Map::GetFriendlyName() const
{
	if (!m_FriendlyNameOverride.empty())
		return m_FriendlyNameOverride;

#if 0
	std::string friendlyName;
	size_t i = size_t(-1);
	for (const auto& segment : mh::split_string(m_MapNames.at(0), "_"))
	{
		i++;

		if (i == 0)
			continue;

		const std::string segMod = mh::tolower(segment);
		friendlyName << ' ' << char(std::toupper(segMod[0])) << std::string_view(segMod).substr(1);
	}

	throw "Not implemented";
#else
	return m_MapNames.at(0);
#endif
}

bool DRPInfo::Map::Matches(const std::string_view& mapName) const
{
	mh::fmtstr<512> buf("{}{}", m_MapNames.at(0).c_str(), R"regex(?(?:_(?!.*_)(?:(?:rc)|(?:final)|(?:[abv]))\d*[a-zA-Z]?)?)regex");
	const std::regex s_MainRegex(buf.c_str(), std::regex::icase);

	if (std::regex_match(mapName.begin(), mapName.end(), s_MainRegex))
		return true;

	for (size_t i = 1; i < m_MapNames.size(); i++)
	{
		try
		{
			const std::regex regex(m_MapNames[i], std::regex::icase);
			if (std::regex_match(mapName.begin(), mapName.end(), regex))
				return true;
		}
		catch (const std::exception& e)
		{
			LogError(MH_SOURCE_LOCATION_CURRENT(), mh::format("{}: {}", typeid(e).name(), e.what()));
		}
	}

	return false;
}
