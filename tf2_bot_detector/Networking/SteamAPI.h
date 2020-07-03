#pragma once

#include "Clock.h"
#include "SteamID.h"

#include <future>
#include <string>
#include <vector>

namespace tf2_bot_detector
{
	class HTTPClient;
}

namespace tf2_bot_detector::SteamAPI
{
	enum class CommunityVisibilityState
	{
		Hidden = 1,
		Visible = 3,
	};

	enum class PersonaState
	{
		Offline = 0,
		Online = 1,
		Busy = 2,
		Away = 3,
		Snooze = 4,
		LookingToTrade = 5,
		LookingToPlay = 6,
	};

	struct PlayerSummary
	{
		tf2_bot_detector::SteamID m_SteamID;
		std::string m_RealName;
		std::string m_Nickname;
		PersonaState m_Status;
		CommunityVisibilityState m_Visibility;
		bool m_ProfileConfigured;
		tf2_bot_detector::time_point_t m_CreationTime;

		tf2_bot_detector::duration_t GetAccountAge() const;
	};

	//void to_json(nlohmann::json& j, const SteamID& d);
	void from_json(const nlohmann::json& j, PlayerSummary& d);

	std::future<std::vector<PlayerSummary>> GetPlayerSummariesAsync(
		std::string apikey, std::vector<SteamID> steamIDs, const HTTPClient& client);
}
