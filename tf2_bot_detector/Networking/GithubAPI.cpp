#include "GithubAPI.h"
#include "Config/JSONHelpers.h"
#include "Log.h"
#include "Version.h"
#include "HTTPClient.h"
#include "HTTPHelpers.h"

#include <cppcoro/generator.hpp>
#include <mh/text/string_insertion.hpp>
#include <nlohmann/json.hpp>

#include <chrono>
#include <future>
#include <iomanip>
#include <optional>
#include <stdio.h>
#include <string>

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace tf2_bot_detector;
using namespace tf2_bot_detector::GithubAPI;

static std::optional<Version> ParseSemVer(const char* version)
{
	Version v{};
	auto parsed = sscanf_s(version, "%i.%i.%i.%i", &v.m_Major, &v.m_Minor, &v.m_Patch, &v.m_Preview);
	if (parsed < 2)
		return std::nullopt;

	return v;
}

namespace
{
	struct InternalRelease : NewVersionResult::Release
	{
		bool m_IsPrerelease;
	};
}

static cppcoro::generator<InternalRelease> GetAllReleases(const HTTPClient& client)
{
	auto str = client.GetString("https://api.github.com/repos/PazerOP/tf2_bot_detector/releases");
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
		if (auto version = ParseSemVer(versionTag.c_str()))
			retVal.m_Version = *version;
		else
		{
			DebugLogWarning("Release id "s << releases.at("id") << " has invalid tag_name version " << versionTag);
			continue;
		}

		co_yield retVal;
	}
}

static NewVersionResult GetLatestVersion(const HTTPClient& client)
{
	DebugLog("GetLatestVersion()");
	try
	{
		NewVersionResult retVal;

		for (auto release : GetAllReleases(client))
		{
			DebugLog("GetLatestVersion(): version = "s << release.m_Version << ", url = " << release.m_URL);

			if (release.m_Version <= VERSION)
			{
				DebugLog("GetLatestVersion(): break");
				break;
			}

			if (release.m_Version.m_Preview != 0)
				retVal.m_Preview = release;
			else
				retVal.m_Stable = release;

			if (retVal.m_Stable.has_value())
				break;
		}

		return retVal;
	}
	catch (const nlohmann::json::parse_error& e)
	{
		LogError("Autoupdate: Failed to parse json: "s << e.what());
		return NewVersionResult{ .m_Error = true };
	}
	catch (const std::exception& e)
	{
		LogError("Autoupdate: Unknown error of type "s << typeid(e).name() << ": " << e.what());
		return NewVersionResult{ .m_Error = true };
	}
}

#if 0
static const std::shared_future<GithubAPI::NewVersionResult>& GetVersionCheckFuture()
{
	static std::shared_future<GithubAPI::NewVersionResult> s_IsNewerVersionAvailable = std::async([]() -> NewVersionResult
		{
			using Status = NewVersionResult::Status;
			Version latestVersion;
			std::string url;
			if (!GetLatestVersion(latestVersion, url))
				return Status::Error;

			if (VERSION.m_Major == latestVersion.m_Major &&
				VERSION.m_Minor == latestVersion.m_Minor &&
				VERSION.m_Patch == latestVersion.m_Patch &&
				VERSION.m_Preview < latestVersion.m_Preview)
			{
				return { Status::PreviewAvailable, url };
			}

			return (latestVersion > VERSION) ? NewVersionResult{ Status::ReleaseAvailable, url } : Status::NoNewVersion;
		});

	return s_IsNewerVersionAvailable;
}
#endif

auto GithubAPI::CheckForNewVersion(const HTTPClient& client) -> std::future<NewVersionResult>
{
	return std::async([&] { return GetLatestVersion(client); });
}
