#include "GithubAPI.h"
#include "Util/JSONUtils.h"
#include "Log.h"
#include "Version.h"
#include "HTTPClient.h"
#include "HTTPHelpers.h"

#include <mh/concurrency/async.hpp>
#include <mh/coroutine/generator.hpp>
#include <mh/text/string_insertion.hpp>
#include <nlohmann/json.hpp>

#include <chrono>
#include <iomanip>
#include <optional>
#include <stdio.h>
#include <string>

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace tf2_bot_detector;
using namespace tf2_bot_detector::GithubAPI;

namespace
{
	struct InternalRelease : NewVersionResult::Release
	{
		bool m_IsPrerelease;
	};
}

static mh::generator<InternalRelease> GetAllReleases(const HTTPClient& client)
{
	auto str = co_await client.GetStringAsync("https://api.github.com/repos/PazerOP/tf2_bot_detector/releases");
	if (str.empty())
		throw std::runtime_error("Autoupdate: response string was empty");

	auto j = nlohmann::json::parse(str);
	if (j.empty())
		throw std::runtime_error("Autoupdate: Response json was empty");

	if (!j.is_array())
		throw std::runtime_error("Autoupdate: Response json was not an array");

	for (auto& releases : j)
	{
		InternalRelease retVal;
		retVal.m_IsPrerelease = releases.at("prerelease");
		retVal.m_URL = releases.at("html_url");

		std::string versionTag = releases.at("tag_name");
		if (auto version = Version::Parse(versionTag.c_str()))
			retVal.m_Version = *version;
		else
		{
			DebugLogWarning("Release id "s << releases.at("id") << " has invalid tag_name version " << versionTag);
			continue;
		}

		co_yield retVal;
	}
}

static mh::task<NewVersionResult> GetLatestVersion(const HTTPClient& client)
{
	DebugLog("GetLatestVersion()");
	try
	{
		NewVersionResult retVal;

		for (auto release : GetAllReleases(client))
		{
			DebugLog("GetLatestVersion(): version = {}, url = {}", release.m_Version, release.m_URL);

			if (release.m_Version <= VERSION)
			{
				DebugLog("GetLatestVersion(): break");
				break;
			}

			if (release.m_IsPrerelease)
				retVal.m_Preview = release;
			else
				retVal.m_Stable = release;

			if (retVal.m_Stable.has_value())
				break;
		}

		co_return retVal;
	}
	catch (const nlohmann::json::parse_error& e)
	{
		LogError("Autoupdate: Failed to parse json: "s << e.what());
		co_return NewVersionResult{ .m_Error = true };
	}
	catch (const std::exception& e)
	{
		LogError("Autoupdate: Unknown error of type "s << typeid(e).name() << ": " << e.what());
		co_return NewVersionResult{ .m_Error = true };
	}
}

auto GithubAPI::CheckForNewVersion(const HTTPClient& client) -> mh::task<NewVersionResult>
{
	return GetLatestVersion(client);
}
