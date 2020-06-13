#pragma once

#include <string>

namespace tf2_bot_detector::GithubAPI
{
	struct NewVersionResult
	{
		enum class Status
		{
			ReleaseAvailable,
			PreviewAvailable,
			Loading,
			NoNewVersion,
			Error,
		} m_Status = Status::Error;

		NewVersionResult() = default;
		NewVersionResult(Status status, std::string url = {}) : m_Status(status), m_URL(std::move(url)) {}

		std::string m_URL;
	};

	NewVersionResult CheckForNewVersion();
}
