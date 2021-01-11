#include "LogsTFAPI.h"
#include "HTTPClient.h"
#include "HTTPHelpers.h"

#include <nlohmann/json.hpp>

using namespace tf2_bot_detector;

mh::task<LogsTFAPI::PlayerLogsInfo> LogsTFAPI::GetPlayerLogsInfoAsync(std::shared_ptr<const IHTTPClient> client, SteamID id)
{
	const std::string string = co_await client->GetStringAsync(mh::format("https://logs.tf/api/v1/log?player={}&limit=0", id.ID64));

	const nlohmann::json json = nlohmann::json::parse(string);

	PlayerLogsInfo info{};
	info.m_ID = id;
	json.at("total").get_to(info.m_LogsCount);
	co_return info;
}
