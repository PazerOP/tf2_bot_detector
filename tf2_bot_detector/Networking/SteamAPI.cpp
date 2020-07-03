#include "SteamAPI.h"
#include "HTTPClient.h"
#include "HTTPHelpers.h"
#include "Config/JSONHelpers.h"
#include "Log.h"

#include <mh/text/string_insertion.hpp>
#include <nlohmann/json.hpp>

using namespace std::string_literals;
using namespace tf2_bot_detector;
using namespace tf2_bot_detector::SteamAPI;

duration_t PlayerSummary::GetAccountAge() const
{
	return clock_t::now() - m_CreationTime;
}

void tf2_bot_detector::SteamAPI::from_json(const nlohmann::json& j, PlayerSummary& d)
{
	d = {};

	d.m_SteamID = j.at("steamid");
	try_get_to(j, "realname", d.m_RealName);
	d.m_Nickname = j.at("personaname");
	d.m_Status = j.at("personastate");
	d.m_Visibility = j.at("communityvisibilitystate");

	if (auto found = j.find("profilestate"); found != j.end())
		d.m_ProfileConfigured = found->get<int>() != 0;

	if (auto found = j.find("timecreated"); found != j.end())
		d.m_CreationTime = std::chrono::system_clock::time_point(std::chrono::seconds(found->get<uint64_t>()));
}

std::future<std::vector<PlayerSummary>> tf2_bot_detector::SteamAPI::GetPlayerSummariesAsync(
	std::string apikey, std::vector<SteamID> steamIDs, const HTTPClient& client)
{
	if (steamIDs.empty())
		return {};

	if (apikey.empty())
	{
		LogError(std::string(__FUNCTION__) << ": apikey was empty");
		return {};
	}

	if (steamIDs.size() > 100)
		LogError(std::string(__FUNCTION__) << "Attempted to fetch " << steamIDs.size() << " steamIDs at once");

	return std::async([apikey, steamIDs{ std::move(steamIDs) }, &client]
		{
			std::string url = "https://api.steampowered.com/ISteamUser/GetPlayerSummaries/v0002/?key="s
				<< apikey << "&steamids=";

			for (size_t i = 0; i < steamIDs.size(); i++)
			{
				if (i != 0)
					url << ',';

				url << steamIDs[i].ID64;
			}

			auto json = nlohmann::json::parse(client.GetString(url));

			return json.at("response").at("players").get<std::vector<PlayerSummary>>();
		});
}
