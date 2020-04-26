#include "SteamID.h"
#include "RegexCharConv.h"

#include <mh/text/string_insertion.hpp>

#include <regex>
#include <stdexcept>

using namespace tf2_bot_detector;

static std::regex s_SteamID3Regex(R"regex(\[([a-zA-Z]):(\d):(\d+)\])regex", std::regex::optimize);
SteamID::SteamID(const std::string_view& str)
{
	ID64 = 0;

	std::match_results<std::string_view::const_iterator> result;
	if (std::regex_match(str.begin(), str.end(), result, s_SteamID3Regex))
	{
		const char firstChar = *result[1].first;
		switch (firstChar)
		{
		case 'U': Type = SteamAccountType::Individual; break;
		case 'M': Type = SteamAccountType::Multiseat; break;
		case 'G': Type = SteamAccountType::GameServer; break;
		case 'A': Type = SteamAccountType::AnonGameServer; break;
		case 'P': Type = SteamAccountType::Pending; break;
		case 'C': Type = SteamAccountType::ContentServer; break;
		case 'g': Type = SteamAccountType::Clan; break;
		case 'a': Type = SteamAccountType::AnonUser; break;

		case 'T':
		case 'L':
		case 'c':
			Type = SteamAccountType::Chat; break;

		case 'I':
		default:
			Type = SteamAccountType::Invalid; break;
		}

		{
			uint32_t universe;
			from_chars_throw(result[2], universe);
			Universe = static_cast<SteamAccountUniverse>(universe);
		}

		{
			uint32_t id;
			from_chars_throw(result[3], id);
			ID = id;
		}

		return;
	}

	throw std::invalid_argument("SteamID string does not match any known formats");
}

SteamID::SteamID(uint32_t id, uint32_t instance, SteamAccountType type, SteamAccountUniverse universe) :
	ID(id),
	Instance(instance),
	Type(type),
	Universe(universe)
{
}

std::string SteamID::str() const
{
	std::string retVal;
	retVal << *this;
	return retVal;
}
