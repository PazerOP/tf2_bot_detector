#include "SteamAPI.h"
#include "Config/Settings.h"
#include "Util/JSONUtils.h"
#include "Util/PathUtils.h"
#include "HTTPClient.h"
#include "HTTPHelpers.h"
#include "Log.h"
#include "Filesystem.h"

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

	class AvatarCacheManager final
	{
	public:
		AvatarCacheManager()
		{
			m_CacheDir = IFilesystem::Get().GetTempDir() / "Steam Avatar Cache";
			std::filesystem::create_directories(m_CacheDir);
			DeleteOldFiles(m_CacheDir, 24h * 7);
		}

		mh::task<Bitmap> GetAvatarBitmap(const HTTPClient* client,
			const std::string url, const std::string hash) const
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
				auto clientPtr = client->shared_from_this();

				// We're not stored in the cache, download now
				std::string data = co_await clientPtr->GetStringAsync(url);

				std::lock_guard lock(m_CacheMutex);
				{
					std::ofstream file(cachedPath, std::ios::trunc | std::ios::binary);
					file << data;
				}
				co_return Bitmap(cachedPath);
			}

			// No HTTPClient and we're not in the cache, so just give up
			co_return Bitmap{};
		}

	private:
		std::filesystem::path m_CacheDir;
		mutable std::mutex m_CacheMutex;

	};

	static AvatarCacheManager& GetAvatarCacheManager()
	{
		static AvatarCacheManager s_AvatarCacheManager;
		return s_AvatarCacheManager;
	}
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

mh::task<Bitmap> PlayerSummary::GetAvatarBitmap(std::shared_ptr<const HTTPClient> client, AvatarQuality quality) const
{
	return GetAvatarCacheManager().GetAvatarBitmap(client.get(), GetAvatarURL(quality), m_AvatarHash);
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

static std::string GenerateSteamAPIURL(const ISteamAPISettings& apiSettings,
	const std::string_view& endpoint, std::string query = "") try
{
	assert(apiSettings.IsSteamAPIAvailable());
	assert(query.empty() || query.starts_with("?"));
	assert(endpoint.starts_with("/"));
	assert(!endpoint.ends_with("/"));

	if (apiSettings.GetSteamAPIMode() == SteamAPIMode::Direct)
	{
		if (!query.empty())
			query[0] = '&';

		return mh::format(MH_FMT_STRING("https://api.steampowered.com{}/?key={}{}"), endpoint, apiSettings.GetSteamAPIKey(), query);
	}
	else if (apiSettings.GetSteamAPIMode() == SteamAPIMode::Proxy)
	{
		return mh::format(MH_FMT_STRING("https://tf2bd-util.pazer.us/SteamAPIProxy{}{}"), endpoint, query);
	}
	else
	{
		throw std::runtime_error(mh::format(MH_FMT_STRING("Should never get here: SteamAPIMode was {}"), mh::enum_fmt(apiSettings.GetSteamAPIMode())));
	}
}
catch (...)
{
	LogException();
	throw;
}

static std::string GenerateSteamIDsQueryParam(const std::vector<SteamID>& steamIDs, size_t max, MH_SOURCE_LOCATION_AUTO(location))
{
	if (steamIDs.size() > max)
		LogError(location, "Attempted to fetch {} steamIDs at once (max {}). Clamping to {}.", steamIDs.size(), max, max);

	std::string steamIDsString = "?steamids=";
	for (size_t i = 0; i < std::min<size_t>(steamIDs.size(), max); i++)
	{
		if (i != 0)
			steamIDsString << ',';

		steamIDsString << steamIDs[i].ID64;
	}

	return steamIDsString;
}

mh::task<std::vector<PlayerSummary>> tf2_bot_detector::SteamAPI::GetPlayerSummariesAsync(
	const ISteamAPISettings& apiSettings, const std::vector<SteamID>& steamIDs, const HTTPClient& client)
{
	if (steamIDs.empty())
		co_return {};

	std::string url = GenerateSteamAPIURL(apiSettings, "/ISteamUser/GetPlayerSummaries/v0002",
		GenerateSteamIDsQueryParam(steamIDs, 100));

	auto clientPtr = client.shared_from_this();
	const std::string data = co_await clientPtr->GetStringAsync(url);

	nlohmann::json json;
	try
	{
		json = nlohmann::json::parse(data);
	}
	catch (...)
	{
		throw SteamAPIError(ErrorCode::JSONParseError);
	}

	try
	{
		co_return json.at("response").at("players").get<std::vector<PlayerSummary>>();
	}
	catch (...)
	{
		throw SteamAPIError(ErrorCode::JSONDeserializeError);
	}
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
	const ISteamAPISettings& apiSettings, const std::vector<SteamID>& steamIDs, const HTTPClient& client)
{
	if (steamIDs.empty())
		co_return std::vector<PlayerBans>();

	std::string url = GenerateSteamAPIURL(apiSettings, "/ISteamUser/GetPlayerBans/v0001",
		GenerateSteamIDsQueryParam(steamIDs, 100));

	auto clientPtr = client.shared_from_this();
	std::string response;
	try
	{
		response = co_await clientPtr->GetStringAsync(url);
	}
	catch (const std::exception&)
	{
		throw SteamAPIError(ErrorCode::GenericHttpError);
	}

	nlohmann::json json;
	try
	{
		json = nlohmann::json::parse(response);
	}
	catch (const std::exception&)
	{
		throw SteamAPIError(ErrorCode::JSONParseError);
	}

	try
	{
		co_return json.at("players").get<std::vector<PlayerBans>>();
	}
	catch (...)
	{
		throw SteamAPIError(ErrorCode::JSONDeserializeError);
	}
}

mh::task<duration_t> tf2_bot_detector::SteamAPI::GetTF2PlaytimeAsync(
	const ISteamAPISettings& apiSettings, const SteamID& steamID, const HTTPClient& client)
{
	if (!steamID.IsValid())
	{
		throw SteamAPIError(ErrorCode::InvalidSteamID,
			mh::format(MH_FMT_STRING("Invalid SteamID {}"), steamID));
	}

	std::string url = GenerateSteamAPIURL(apiSettings, "/IPlayerService/GetOwnedGames/v0001",
		mh::format(MH_FMT_STRING("?input_json=%7B%22appids_filter%22%3A%5B440%5D,%22include_played_free_games%22%3Atrue,%22steamid%22%3A{}%7D"),
			steamID.ID64));

	auto clientPtr = client.shared_from_this();
	std::string responseString;
	try
	{
		responseString = co_await clientPtr->GetStringAsync(url);
	}
	catch (...)
	{
		throw SteamAPIError(ErrorCode::GenericHttpError);
	}

	nlohmann::json json;
	try
	{
		json = nlohmann::json::parse(responseString);
	}
	catch (...)
	{
		throw SteamAPIError(ErrorCode::JSONParseError);
	}

	auto& response = json.at("response");
	if (!response.contains("game_count"))
	{
		// response is empty (as opposed to games being empty and game_count = 0) if games list is private
		throw SteamAPIError(ErrorCode::InfoPrivate, "Games list is private");
	}

	auto games = response.find("games");
	if (games == response.end())
		throw SteamAPIError(ErrorCode::GameNotOwned); // TF2 not on their owned games list

	if (games->size() != 1)
	{
		throw SteamAPIError(ErrorCode::UnexpectedDataFormat,
			mh::format(MH_FMT_STRING("Unexpected games array size {}"), games->size()));
	}

	auto& firstElem = games->at(0);
	if (uint32_t appid = firstElem.at("appid"); appid != 440)
	{
		throw SteamAPIError(ErrorCode::UnexpectedDataFormat,
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
			case ErrorCode::JSONDeserializeError:
				return "There was an error deserializing the response JSON.";
			case ErrorCode::InvalidSteamID:
				return "The SteamID was not valid for the given request.";
			case ErrorCode::EmptyAPIKey:
				return "The provided Steam Web API key was an empty string.";
			case ErrorCode::SteamAPIDisabled:
				return "Steam API support has been disabled via the tool settings.";
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

mh::task<std::unordered_set<SteamID>> tf2_bot_detector::SteamAPI::GetFriendList(const ISteamAPISettings& apiSettings,
	const SteamID& steamID, const HTTPClient& client)
{
	if (!steamID.IsValid())
	{
		LogError(MH_SOURCE_LOCATION_CURRENT(), "Invalid SteamID {}", steamID.ID64);
		co_return {};
	}

	auto url = GenerateSteamAPIURL(apiSettings, "/ISteamUser/GetFriendList/v0001", mh::format("?steamid={}", steamID.ID64));

	auto clientPtr = client.shared_from_this();
	std::string data = co_await clientPtr->GetStringAsync(url);

	const auto json = nlohmann::json::parse(data);

	std::unordered_set<SteamID> retVal;

	auto& friendsList = json.at("friendslist");
	auto& friends = friendsList.at("friends");

	for (const auto& friendEntry : friends)
		retVal.insert(SteamID(friendEntry.at("steamid").get<std::string_view>()));

	co_return retVal;
}

tf2_bot_detector::SteamAPI::SteamAPIError::SteamAPIError(
	std::error_condition code, const std::string_view& detail, const mh::source_location& location) :
	mh::error_condition_exception(code, mh::format(MH_FMT_STRING("{}: {}"), location, detail)),
	m_SourceLocation(location)
{
	if (code != ErrorCode::InfoPrivate && code != ErrorCode::GameNotOwned)
		LogException(m_SourceLocation, *this);
}

bool SteamAPI::PlayerBans::HasAnyBans() const
{
	if (m_CommunityBanned)
		return true;
	if (m_EconomyBan != PlayerEconomyBan::None)
		return true;
	if (m_VACBanCount > 0)
		return true;
	if (m_GameBanCount > 0)
		return true;

	return false;
}

mh::task<PlayerInventoryInfo> SteamAPI::GetTF2InventoryInfoAsync(const ISteamAPISettings& apiSettings,
	const SteamID& steamID, const IHTTPClient& client)
{
	if (!steamID.IsValid())
	{
		LogError("Invalid SteamID {}", steamID.ID64);
		co_return {};
	}

	auto clientPtr = client.shared_from_this();
	std::string data;

	auto url = GenerateSteamAPIURL(apiSettings, "/IEconItems_440/GetPlayerItems/v0001", mh::format("?steamid={}", steamID.ID64));

	try
	{
		data = co_await clientPtr->GetStringAsync(url);
	}
	catch (const http_error& error)
	{
		if (error.code() == HTTPResponseCode::Forbidden)
			throw SteamAPIError(ErrorCode::InfoPrivate);
		else
			throw; // rethrow generic http errors
	}

	nlohmann::json json;
	try
	{
		json = nlohmann::json::parse(data);
	}
	catch (...)
	{
		throw SteamAPIError(ErrorCode::JSONParseError);
	}

	PlayerInventoryInfo info{};

	try
	{
		const nlohmann::json& result = json.at("result");

		int status = result.at("status");
		if (status == 15)
			throw SteamAPIError(ErrorCode::InfoPrivate);

		info.m_Items = static_cast<uint32_t>(result.at("items").size());
		result.at("num_backpack_slots").get_to(info.m_Slots);
	}
	catch (const SteamAPIError&)
	{
		throw; // Don't touch these
	}
	catch (...)
	{
		throw SteamAPIError(ErrorCode::JSONDeserializeError);
	}

	co_return info;
}
