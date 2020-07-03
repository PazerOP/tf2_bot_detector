#include "SponsorsList.h"
#include "JSONHelpers.h"

#include <mh/text/string_insertion.hpp>
#include <nlohmann/json.hpp>

using namespace std::string_literals;
using namespace tf2_bot_detector;

SponsorsList::SponsorsList(const Settings& settings) :
	m_Settings(&settings)
{
	LoadFile();
}

void SponsorsList::LoadFile()
{
	m_Sponsors = LoadConfigFileAsync<SponsorsListFile>("cfg/sponsors.json", true, *m_Settings);
}

auto SponsorsList::GetSponsors() const -> std::vector<Sponsor>
{
	if (!mh::is_future_ready(m_Sponsors))
		return {};

	return m_Sponsors.get().m_Sponsors;
}

void SponsorsList::SponsorsListFile::Deserialize(const nlohmann::json& json)
{
	BaseClass::Deserialize(json);

	if (auto found = json.find("sponsors"); found != json.end())
		m_Sponsors = found->get<std::vector<Sponsor>>();
}

void SponsorsList::SponsorsListFile::Serialize(nlohmann::json& json) const
{
	BaseClass::Serialize(json);

	if (!m_Schema || m_Schema->m_Version != SPONSORS_SCHEMA_VERSION)
		json["$schema"] = ConfigSchemaInfo("sponsors", SPONSORS_SCHEMA_VERSION);

	json["sponsors"] = m_Sponsors;
}

void SponsorsList::SponsorsListFile::ValidateSchema(const ConfigSchemaInfo& schema) const
{
	BaseClass::ValidateSchema(schema);

	if (schema.m_Type != "sponsors")
		throw std::runtime_error("Schema "s << std::quoted(schema.m_Type) << " is not a sponsors list");
	if (schema.m_Version != SPONSORS_SCHEMA_VERSION)
		throw std::runtime_error("Sponsors schema must be version "s << SPONSORS_SCHEMA_VERSION << ", but was " << schema.m_Version);
}

void tf2_bot_detector::to_json(nlohmann::json& j, const SponsorsList::Sponsor& d)
{
	j =
	{
		{ "name", d.m_Name },
		{ "message", d.m_Message },
	};
}

void tf2_bot_detector::from_json(const nlohmann::json& j, SponsorsList::Sponsor& d)
{
	try_get_to(j, "name", d.m_Name);
	try_get_to(j, "message", d.m_Message);
}
