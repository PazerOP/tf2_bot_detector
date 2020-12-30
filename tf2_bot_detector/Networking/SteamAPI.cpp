#include "SteamAPI.h"
#include "Util/JSONUtils.h"
#include "Util/PathUtils.h"
#include "HTTPClient.h"
#include "HTTPHelpers.h"
#include "Log.h"

#include <mh/concurrency/thread_pool.hpp>
#include <mh/coroutine/future.hpp>
#include <mh/text/fmtstr.hpp>
#include <mh/text/format.hpp>
#include <mh/text/string_insertion.hpp>
#include <mh/future.hpp>
#include <nlohmann/json.hpp>
#include <stb_image.h>

#include <fstream>
#include <regex>

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace tf2_bot_detector;
using namespace tf2_bot_detector::SteamAPI;

namespace
{
	struct SteamAPITask final
	{
		URL m_RequestURL;
		std::string m_Response;
	};

	static mh::thread_pool s_SteamAPIThreadPool(2);
	static mh::task<SteamAPITask> SteamAPIGET(const HTTPClient& client, URL url)
	{
		auto clientPtr = client.shared_from_this();
		co_await s_SteamAPIThreadPool.co_add_task();

		DebugLog("[SteamAPI] HTTP GET "s << url);

		SteamAPITask retVal;
		retVal.m_RequestURL = url;
		retVal.m_Response = clientPtr->GetString(url);
		co_return retVal;
	}

	class AvatarCacheManager final
	{
	public:
		AvatarCacheManager()
		{
			m_CacheDir = std::filesystem::temp_directory_path() / "TF2 Bot Detector/Steam Avatar Cache";
			std::filesystem::create_directories(m_CacheDir);
			DeleteOldFiles(m_CacheDir, 24h * 7);
		}

		mh::task<Bitmap> GetAvatarBitmap(const HTTPClient* client,
			const std::string& url, const std::string_view& hash) const
		{
			const std::filesystem::path cachedPath = m_CacheDir / mh::fmtstr<128>("{}.jpg", hash).view();

			// See if we're already stored in the cache
			{
				std::lock_guard lock(m_CacheMutex);
				try
				{
					if (std::filesystem::exists(cachedPath))
						co_return Bitmap(cachedPath);
				}
				catch (const std::exception& e)
				{
					LogException(MH_SOURCE_LOCATION_CURRENT(), e, "Failed to load cached avatar from {}, re-fetching...", cachedPath);
				}
			}

			if (client)
			{
				// We're not stored in the cache, download now
				const SteamAPITask& data = co_await SteamAPIGET(*client, url);

				std::lock_guard lock(m_CacheMutex);
				{
					std::ofstream file(cachedPath, std::ios::trunc | std::ios::binary);
					file << data.m_Response;
				}
				co_return Bitmap(cachedPath);
			}

			// No HTTPClient and we're not in the cache, so just give up
			co_return Bitmap{};
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

mh::task<Bitmap> PlayerSummary::GetAvatarBitmap(const HTTPClient* client, AvatarQuality quality) const
{
	return s_AvatarCacheManager.GetAvatarBitmap(client, GetAvatarURL(quality), m_AvatarHash);
}

std::string_view PlayerSummary::GetVanityURL() const
{
	constexpr std::string_view vanityBase = "https://steamcommunity.com/id/";
	if (m_ProfileURL.starts_with(vanityBase))
	{
		auto retVal = std::string_view(m_ProfileURL).substr(vanityBase.size());
		if (retVal.ends_with('/'))
			retVal = retVal.substr(0, retVal.size() - 1);

		return retVal;
	}

	return "";
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
	d.m_ProfileURL = j.at("profileurl");

	if (auto found = j.find("lastlogoff"); found != j.end())
		d.m_LastLogOff = std::chrono::system_clock::time_point(std::chrono::seconds(found->get<uint64_t>()));

	if (auto found = j.find("profilestate"); found != j.end())
		d.m_ProfileConfigured = found->get<int>() != 0;
	if (auto found = j.find("commentpermission"); found != j.end())
		d.m_CommentPermissions = found->get<int>() != 0;

	if (auto found = j.find("timecreated"); found != j.end())
		d.m_CreationTime = std::chrono::system_clock::time_point(std::chrono::seconds(found->get<uint64_t>()));
}

mh::task<std::vector<PlayerSummary>> tf2_bot_detector::SteamAPI::GetPlayerSummariesAsync(
	const std::string_view& apikey, const std::vector<SteamID>& steamIDs, const HTTPClient& client)
{
	if (steamIDs.empty())
		co_return {};

	if (apikey.empty())
	{
		LogError(MH_SOURCE_LOCATION_CURRENT(), "apikey was empty");
		co_return {};
	}

	if (steamIDs.size() > 100)
	{
		LogError(MH_SOURCE_LOCATION_CURRENT(), "Attempted to fetch "s << steamIDs.size()
			<< " steamIDs at once (max 100)");
	}

	std::string url = "https://api.steampowered.com/ISteamUser/GetPlayerSummaries/v0002/?key="s
		<< apikey << "&steamids=";

	for (size_t i = 0; i < std::min<size_t>(steamIDs.size(), 100); i++)
	{
		if (i != 0)
			url << ',';

		url << steamIDs[i].ID64;
	}

	const auto& data = co_await SteamAPIGET(client, url);

	auto json = nlohmann::json::parse(data.m_Response);
	co_return json.at("response").at("players").get<std::vector<PlayerSummary>>();
}

void tf2_bot_detector::SteamAPI::from_json(const nlohmann::json& j, PlayerBans& d)
{
	d = {};
	d.m_SteamID = j.at("SteamId");
	d.m_CommunityBanned = j.at("CommunityBanned");
	d.m_VACBanCount = j.at("NumberOfVACBans");
	d.m_GameBanCount = j.at("NumberOfGameBans");
	d.m_TimeSinceLastBan = 24h * j.at("DaysSinceLastBan").get<uint32_t>();

	const std::string_view economyBan = j.at("EconomyBan");
	if (economyBan == "none"sv)
		d.m_EconomyBan = PlayerEconomyBan::None;
	else if (economyBan == "banned"sv)
		d.m_EconomyBan = PlayerEconomyBan::Banned;
	else if (economyBan == "probation"sv)
		d.m_EconomyBan = PlayerEconomyBan::Probation;
	else
	{
		LogError(MH_SOURCE_LOCATION_CURRENT(), "Unknown EconomyBan value "s << std::quoted(economyBan));
		d.m_EconomyBan = PlayerEconomyBan::Unknown;
	}
}

mh::task<std::vector<PlayerBans>> tf2_bot_detector::SteamAPI::GetPlayerBansAsync(
	const std::string_view& apikey, const std::vector<SteamID>& steamIDs, const HTTPClient& client)
{
	if (steamIDs.empty())
		co_return std::vector<PlayerBans>();

	if (apikey.empty())
		throw SteamAPIError(MH_SOURCE_LOCATION_CURRENT(), ErrorCode::EmptyAPIKey);

	if (steamIDs.size() > 100)
		LogError(MH_SOURCE_LOCATION_CURRENT(), "Attempted to fetch {} steamIDs at once (max 100)", steamIDs.size());

	std::string url = mh::format(
		MH_FMT_STRING("https://api.steampowered.com/ISteamUser/GetPlayerBans/v0001/?key={}&steamids="), apikey);

	for (size_t i = 0; i < std::min<size_t>(steamIDs.size(), 100); i++)
	{
		if (i != 0)
			url += ',';

		url << steamIDs[i].ID64;
	}

	auto data = SteamAPIGET(client, url);

	std::string response;
	try
	{
		response = (co_await data).m_Response;
	}
	catch (const std::exception&)
	{
		throw SteamAPIError(MH_SOURCE_LOCATION_CURRENT(), ErrorCode::GenericHttpError);
	}

	nlohmann::json json;
	try
	{
		json = nlohmann::json::parse(response);
	}
	catch (const std::exception&)
	{
		throw SteamAPIError(MH_SOURCE_LOCATION_CURRENT(), ErrorCode::JSONParseError);
	}

	co_return json.at("players").get<std::vector<PlayerBans>>();
}

mh::task<duration_t> tf2_bot_detector::SteamAPI::GetTF2PlaytimeAsync(
	const std::string_view& apikey, const SteamID& steamID, const HTTPClient& client)
{
	if (!steamID.IsValid())
	{
		throw SteamAPIError(MH_SOURCE_LOCATION_CURRENT(), ErrorCode::InvalidSteamID,
			mh::format(MH_FMT_STRING("Invalid SteamID {}"), steamID));
	}

	if (apikey.empty())
		throw SteamAPIError(MH_SOURCE_LOCATION_CURRENT(), ErrorCode::EmptyAPIKey);

	auto url = mh::format(MH_FMT_STRING("https://api.steampowered.com/IPlayerService/GetOwnedGames/v0001/?key={}&input_json=%7B%22appids_filter%22%3A%5B440%5D,%22include_played_free_games%22%3Atrue,%22steamid%22%3A{}%7D"), apikey, steamID.ID64);

	auto data = co_await SteamAPIGET(client, url);

	std::string responseString;
	try
	{
		responseString = data.m_Response;
	}
	catch (...)
	{
		throw SteamAPIError(MH_SOURCE_LOCATION_CURRENT(), ErrorCode::GenericHttpError);
	}

	nlohmann::json json;
	try
	{
		json = nlohmann::json::parse(responseString);
	}
	catch (...)
	{
		throw SteamAPIError(MH_SOURCE_LOCATION_CURRENT(), ErrorCode::JSONParseError);
	}

	auto& response = json.at("response");
	if (!response.contains("game_count"))
	{
		// response is empty (as opposed to games being empty and game_count = 0) if games list is private
		throw SteamAPIError(MH_SOURCE_LOCATION_CURRENT(), ErrorCode::InfoPrivate, "Games list is private");
	}

	auto games = response.find("games");
	if (games == response.end())
		throw SteamAPIError(MH_SOURCE_LOCATION_CURRENT(), ErrorCode::GameNotOwned); // TF2 not on their owned games list

	if (games->size() != 1)
	{
		throw SteamAPIError(MH_SOURCE_LOCATION_CURRENT(), ErrorCode::UnexpectedDataFormat,
			mh::format(MH_FMT_STRING("Unexpected games array size {}"), games->size()));
	}

	auto& firstElem = games->at(0);
	if (uint32_t appid = firstElem.at("appid"); appid != 440)
	{
		throw SteamAPIError(MH_SOURCE_LOCATION_CURRENT(), ErrorCode::UnexpectedDataFormat,
			mh::format(MH_FMT_STRING("Unexpected appid {} at response.games[0].appid"), appid));
	}

	co_return std::chrono::minutes(firstElem.at("playtime_forever"));
}

namespace tf2_bot_detector::SteamAPI
{
	class ErrorCategoryType final : public std::error_category
	{
	public:
		const char* name() const noexcept override { return "TF2 Bot Detector (SteamAPI)"; }
		std::string message(int condition) const override
		{
			switch (ErrorCode(condition))
			{
			case ErrorCode::Success:
				return "Success";
			case ErrorCode::EmptyState:
				return "The state does not contain any value.";
			case ErrorCode::InfoPrivate:
				return "Your Steam account does not have permission to access this information.";
			case ErrorCode::GameNotOwned:
				return "The specified AppID is not owned by the specified account.";
			case ErrorCode::UnexpectedDataFormat:
				return "The response from the Steam API was formatted in an unexpected or unsupported way.";
			case ErrorCode::GenericHttpError:
				return "There was an HTTP error encountered when getting the result.";
			case ErrorCode::JSONParseError:
				return "There was an error parsing the response JSON.";
			case ErrorCode::InvalidSteamID:
				return "The SteamID was not valid for the given request.";
			case ErrorCode::EmptyAPIKey:
				return "The provided Steam Web API key was an empty string.";
			}

			return mh::format("Unknown SteamAPI error ({})", condition);
		}
	};

	static const ErrorCategoryType& ErrorCategory()
	{
		static const ErrorCategoryType s_Value;
		return s_Value;
	}
}

std::error_condition tf2_bot_detector::SteamAPI::make_error_condition(tf2_bot_detector::SteamAPI::ErrorCode e)
{
	return std::error_condition(int(e), tf2_bot_detector::SteamAPI::ErrorCategory());
}

mh::task<std::unordered_set<SteamID>> tf2_bot_detector::SteamAPI::GetFriendList(const std::string_view& apikey,
	const SteamID& steamID, const HTTPClient& client)
{
	if (!steamID.IsValid())
	{
		LogError(MH_SOURCE_LOCATION_CURRENT(), "Invalid SteamID {}", steamID.ID64);
		co_return {};
	}

	if (apikey.empty())
	{
		LogError(MH_SOURCE_LOCATION_CURRENT(), "apikey was empty");
		co_return {};
	}

	auto url = mh::format(
		MH_FMT_STRING("https://api.steampowered.com/ISteamUser/GetFriendList/v0001/?key={}&steamid={}"),
		apikey, steamID.ID64);

	auto data = co_await SteamAPIGET(client, url);

	const auto json = nlohmann::json::parse(data.m_Response);

	std::unordered_set<SteamID> retVal;

	auto& friendsList = json.at("friendslist");
	auto& friends = friendsList.at("friends");

	for (const auto& friendEntry : friends)
		retVal.insert(SteamID(friendEntry.at("steamid").get<std::string_view>()));

	co_return retVal;
}

tf2_bot_detector::SteamAPI::SteamAPIError::SteamAPIError(const mh::source_location& location,
	std::error_condition code, const std::string_view& detail) :
	mh::error_condition_exception(code, mh::format(MH_FMT_STRING("{}: {}"), location, detail)),
	m_SourceLocation(location)
{
	if (code != ErrorCode::InfoPrivate && code != ErrorCode::GameNotOwned)
		LogException(m_SourceLocation, *this);
}
