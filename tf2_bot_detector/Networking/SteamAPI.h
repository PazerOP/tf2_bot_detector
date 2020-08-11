#pragma once

#include "Bitmap.h"
#include "Clock.h"
#include "SteamID.h"

#include <future>
#include <optional>
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

	enum class AvatarQuality
	{
		Small,
		Medium,
		Large,
	};

	struct PlayerSummary
	{
		SteamID m_SteamID;
		std::string m_RealName;
		std::string m_Nickname;
		std::string m_AvatarHash;
		std::string m_ProfileURL;
		PersonaState m_Status;
		CommunityVisibilityState m_Visibility;
		bool m_ProfileConfigured = false;
		bool m_CommentPermissions = false;
		std::optional<time_point_t> m_CreationTime;
		std::optional<time_point_t> m_LastLogOff;

		std::optional<duration_t> GetAccountAge() const;

		std::string GetAvatarURL(AvatarQuality quality = AvatarQuality::Large) const;
		std::shared_future<Bitmap> GetAvatarBitmap(const HTTPClient* client,
			AvatarQuality quality = AvatarQuality::Large) const;

		std::string_view GetVanityURL() const;
	};

	//void to_json(nlohmann::json& j, const SteamID& d);
	void from_json(const nlohmann::json& j, PlayerSummary& d);

	std::future<std::vector<PlayerSummary>> GetPlayerSummariesAsync(
		std::string apikey, std::vector<SteamID> steamIDs, const HTTPClient& client);
}
