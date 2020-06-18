#include "SteamID.h"
#include "RegexHelpers.h"

#include <mh/text/string_insertion.hpp>
#include <nlohmann/json.hpp>

#include <regex>
#include <stdexcept>

using namespace std::string_literals;
using namespace tf2_bot_detector;

SteamID::SteamID(const std::string_view& str)
{
	ID64 = 0;

	// Steam3
	static std::regex s_SteamID3Regex(R"regex(\[([a-zA-Z]):(\d):(\d+)(:\d+)?\])regex", std::regex::optimize);
	if (std::match_results<std::string_view::const_iterator> result;
		std::regex_match(str.begin(), str.end(), result, s_SteamID3Regex))
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
			Type = SteamAccountType::Invalid; break;

		default:
			throw std::invalid_argument("Invalid SteamID3: Unknown SteamAccountType '"s << firstChar << '\'');
		}

		{
			uint32_t universe;
			if (auto parseResult = from_chars(result[2], universe); !parseResult)
				throw std::invalid_argument("Out-of-range value for SteamID3 universe: "s << result[2].str());

			Universe = static_cast<SteamAccountUniverse>(universe);
		}

		{
			uint32_t id;
			if (auto parseResult = from_chars(result[3], id); !parseResult)
				throw std::invalid_argument("Out-of-range value for SteamID3 ID: "s << result[3].str());

			ID = id;
		}

		if (result[4].matched)
		{
			uint32_t instance;
			if (auto parseResult = from_chars(result[4], instance); !parseResult)
				throw std::invalid_argument("Out-of-range value for SteamID3 account instance: "s << result[4].str());

			Instance = static_cast<SteamAccountInstance>(instance);
		}
		else
		{
			Instance = SteamAccountInstance::Desktop;
		}

		return;
	}

	// Steam64
	if (std::all_of(str.begin(), str.end(), [](char c) { return std::isdigit(c) || std::isspace(c); }))
	{
		uint64_t result;
		if (auto parseResult = mh::from_chars(str, result); !parseResult)
			throw std::invalid_argument("Out-of-range SteamID64: "s << str);

		ID64 = result;
		return;
	}

	throw std::invalid_argument("SteamID string does not match any known formats");
}

std::string SteamID::str() const
{
	std::string retVal;
	retVal << *this;
	return retVal;
}

void tf2_bot_detector::to_json(nlohmann::json& j, const SteamID& d)
{
	j = d.str();
}

void tf2_bot_detector::from_json(const nlohmann::json& j, SteamID& d)
{
	if (j.is_number_unsigned())
		d = SteamID(j.get<uint64_t>());
	else
		d = SteamID(j.get<std::string_view>());
}
