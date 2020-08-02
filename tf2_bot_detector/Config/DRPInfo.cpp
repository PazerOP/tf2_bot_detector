#include "DRPInfo.h"
#include "JSONHelpers.h"

#include <mh/text/format.hpp>
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

void DRPInfo::DRPFile::Deserialize(const nlohmann::json& json)
{
	BaseClass::Deserialize(json);

	if (auto found = json.find("maps"); found != json.end())
		found->get_to(m_Maps);
}

void DRPInfo::DRPFile::Serialize(nlohmann::json& json) const
{
	BaseClass::Serialize(json);

	if (!m_Schema || m_Schema->m_Version != DRP_SCHEMA_VERSION)
		json["$schema"] = ConfigSchemaInfo("discord_rich_presence", DRP_SCHEMA_VERSION);

	__debugbreak();
	for (const auto& entry : json["maps"])
	{
		__debugbreak();
	}
	//json["maps"] = m_Maps;
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
}

void tf2_bot_detector::to_json(nlohmann::json& j, const DRPInfo::Map& d)
{
	if (!d.m_FriendlyNameOverride.empty())
		j["friendly_name_override"] = d.m_FriendlyNameOverride;
	if (!d.m_LargeImageKeyOverride.empty())
		j["large_image_key_override"] = d.m_LargeImageKeyOverride;
	if (d.m_MapNames.size() > 1)
	{
		auto& a = j["map_name_aliases"];
		a = nlohmann::json::array();

		for (size_t i = 1; i < d.m_MapNames.size(); i++)
			a.push_back(d.m_MapNames[i]);
	}
}

void tf2_bot_detector::from_json(const nlohmann::json& j, DRPInfo::Map& d)
{
	try_get_to_defaulted(j, d.m_FriendlyNameOverride, "friendly_name_override");
	try_get_to_defaulted(j, d.m_LargeImageKeyOverride, "large_image_key_override");

	if (auto found = j.find("map_name_aliases"); found != j.end())
		d.m_MapNames.insert(d.m_MapNames.end(), found->begin(), found->end());
}
