#include "GithubAPI.h"
#include "Config/JSONHelpers.h"
#include "Log.h"
#include "Version.h"
#include "HTTPHelpers.h"

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

static bool GetLatestVersion(Version& version, std::string& url)
{
#if 0
	auto res = client.Get("/repos/PazerOP/tf2_bot_detector/releases", headers);
	if (!res)
	{
		LogWarning("Autoupdate: Failed to contact GitHub when checking for a newer version.");
		return false;
	}

	auto body = res->body;
#endif

	try
	{
		auto j = HTTP::GetJSON("https://api.github.com/repos/PazerOP/tf2_bot_detector/releases");
		if (j.empty())
		{
			LogError("Autoupdate: Response was empty");
			return false;
		}

		if (!j.is_array())
		{
			LogError("Autoupdate: Response json was not an array");
			return false;
		}

		auto& first = j.front();
		auto tag = first["tag_name"].get<std::string>();

		if (!try_get_to(first, "html_url", url))
		{
			LogError("Autoupdate: Failed to get 'html_url' property for a release");
			return false;
		}

		std::string versionStr;
		if (!try_get_to(first, "tag_name", versionStr))
		{
			LogError("Autoupdate: Failed to get 'tag_name' property for a release");
			return false;
		}

		auto parsedVersion = ParseSemVer(tag.c_str());
		if (!parsedVersion)
		{
			LogError("Autoupdate: Failed to parse semantic version from "s << std::quoted(tag));
			return false;
		}

		version = *parsedVersion;
		return true;
	}
	catch (const nlohmann::json::parse_error& e)
	{
		LogError("Autoupdate: Failed to parse json: "s << e.what());
		return false;
	}
	catch (const std::exception& e)
	{
		LogError("Autoupdate: Unknown error of type "s << typeid(e).name() << ": " << e.what());
		return false;
	}
}

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

auto GithubAPI::CheckForNewVersion() -> NewVersionResult
{
	using Status = NewVersionResult::Status;

	if (!GetVersionCheckFuture().valid())
		return Status::Error;

	switch (GetVersionCheckFuture().wait_for(0s))
	{
	case std::future_status::deferred:
		DebugLog("s_IsNewerVersionAvailable wait returned deferred???");
	case std::future_status::timeout:
		return Status::Loading;

	case std::future_status::ready:
		break;
	}

	return GetVersionCheckFuture().get();
}
