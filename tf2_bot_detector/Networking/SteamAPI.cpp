#include "SteamAPI.h"
#include "Util/JSONUtils.h"
#include "Util/PathUtils.h"
#include "HTTPClient.h"
#include "HTTPHelpers.h"
#include "Log.h"

#include <mh/text/fmtstr.hpp>
#include <mh/text/format.hpp>
#include <mh/text/string_insertion.hpp>
#include <mh/future.hpp>
#include <nlohmann/json.hpp>
#include <stb_image.h>

#include <fstream>

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace tf2_bot_detector;
using namespace tf2_bot_detector::SteamAPI;

namespace
{
	class AvatarCacheManager final
	{
	public:
		AvatarCacheManager()
		{
			m_CacheDir = std::filesystem::temp_directory_path() / "TF2 Bot Detector/Steam Avatar Cache";
			std::filesystem::create_directories(m_CacheDir);
			DeleteOldFiles(m_CacheDir, 24h * 7);
		}

		std::shared_future<Bitmap> GetAvatarBitmap(const HTTPClient* client,
			const std::string& url, const std::string_view& hash) const
		{
			const std::filesystem::path cachedPath = m_CacheDir / mh::fmtstr<128>("{}.jpg", hash).view();

			// See if we're already stored in the cache
			{
				std::lock_guard lock(m_CacheMutex);
				if (std::filesystem::exists(cachedPath))
					return mh::make_ready_future(Bitmap(cachedPath));
			}

			if (client)
			{
				auto clientPtr = client->shared_from_this();
				// We're not stored in the cache, download now
				return std::async([this, cachedPath, clientPtr, url]() -> Bitmap
					{
						const auto data = clientPtr->GetString(url);

						std::lock_guard lock(m_CacheMutex);
						{
							std::ofstream file(cachedPath, std::ios::trunc | std::ios::binary);
							file << data;
						}
						return Bitmap(cachedPath);
					});
			}

			// No HTTPClient and we're not in the cache, so just give up
			return mh::make_ready_future(Bitmap{});
		}

	private:
		std::filesystem::path m_CacheDir;
		mutable std::mutex m_CacheMutex;

	} s_AvatarCacheManager;
}

std::optional<duration_t> PlayerSummary::GetAccountAge() const
{
	if (m_CreationTime)
		return clock_t::now() - *m_CreationTime;

	return std::nullopt;
}

std::string PlayerSummary::GetAvatarURL(AvatarQuality quality) const
{
	const char* qualityStr;
	switch (quality)
	{
	case AvatarQuality::Large:
		qualityStr = "_full";
		break;
	case AvatarQuality::Medium:
		qualityStr = "_medium";
		break;

	default:
		LogWarning(MH_SOURCE_LOCATION_CURRENT(),
			"Unknown AvatarQuality "s << +std::underlying_type_t<AvatarQuality>(quality));
		[[fallthrough]];
	case AvatarQuality::Small:
		qualityStr = "";
		break;
	}

	return mh::format("https://steamcdn-a.akamaihd.net/steamcommunity/public/images/avatars/{0:.2}/{0}{1}.jpg",
		m_AvatarHash, qualityStr);
}

std::shared_future<Bitmap> PlayerSummary::GetAvatarBitmap(const HTTPClient* client, AvatarQuality quality) const
{
	return s_AvatarCacheManager.GetAvatarBitmap(client, GetAvatarURL(quality), m_AvatarHash);
}

void tf2_bot_detector::SteamAPI::from_json(const nlohmann::json& j, PlayerSummary& d)
{
	d = {};

	d.m_SteamID = j.at("steamid");
	try_get_to_defaulted(j, d.m_RealName, "realname");
	d.m_Nickname = j.at("personaname");
	d.m_Status = j.at("personastate");
	d.m_Visibility = j.at("communityvisibilitystate");
	d.m_AvatarHash = j.at("avatarhash");

	if (auto found = j.find("lastlogoff"); found != j.end())
		d.m_LastLogOff = std::chrono::system_clock::time_point(std::chrono::seconds(found->get<uint64_t>()));

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
