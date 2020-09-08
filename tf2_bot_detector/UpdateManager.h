#pragma once

#include "Platform/Platform.h"
#include "ReleaseChannel.h"
#include "Version.h"

#include <mh/error/status.hpp>
#include <mh/reflection/enum.hpp>

#include <memory>

namespace tf2_bot_detector
{
	class Settings;

	struct BuildInfo
	{
		ReleaseChannel m_ReleaseChannel{};
		Version m_Version{};

		std::string m_GitHubURL;
		std::string m_MSIXBundleURL;

		struct BuildVariant
		{
			Platform::OS m_OS{};
			Platform::Arch m_Arch{};
			std::string m_DownloadURL;
		};

		std::vector<BuildVariant> m_Updater;
		std::vector<BuildVariant> m_Portable;
	};
}

namespace tf2_bot_detector
{
	class IAvailableUpdate
	{
	public:
		IAvailableUpdate(BuildInfo&& bi) : m_BuildInfo(std::move(bi)) {}
		virtual ~IAvailableUpdate() = default;

		[[nodiscard]] virtual bool CanSelfUpdate() const = 0;
		virtual bool BeginSelfUpdate() const = 0;

		BuildInfo m_BuildInfo;
	};

	enum class UpdateStatus
	{
		Unknown = 0,

		StateSwitchFailure,

		UpdateCheckDisabled,
		InternetAccessDisabled,

		CheckQueued,
		Checking,

		CheckFailed,
		UpToDate,
		UpdateAvailable,

		UpdateToolRequired,
		UpdateToolDownloading,
		UpdateToolDownloadFailed,
		UpdateToolDownloadSuccess,

		Downloading,
		DownloadFailed,
		DownloadSuccess,

		Updating,
		UpdateFailed,
		UpdateSuccess,
	};

	class IUpdateManager
	{
	public:
		virtual ~IUpdateManager() = default;

		static std::unique_ptr<IUpdateManager> Create(const Settings& settings);

		virtual void QueueUpdateCheck() = 0;

		virtual void Update() = 0;

		virtual mh::status_reader<UpdateStatus> GetUpdateStatus() const = 0;
		virtual const IAvailableUpdate* GetAvailableUpdate() const = 0;
	};
}

MH_ENUM_REFLECT_BEGIN(tf2_bot_detector::UpdateStatus)
	MH_ENUM_REFLECT_VALUE(Unknown)

	MH_ENUM_REFLECT_VALUE(StateSwitchFailure)

	MH_ENUM_REFLECT_VALUE(UpdateCheckDisabled)
	MH_ENUM_REFLECT_VALUE(InternetAccessDisabled)

	MH_ENUM_REFLECT_VALUE(Checking)

	MH_ENUM_REFLECT_VALUE(CheckFailed)
	MH_ENUM_REFLECT_VALUE(UpToDate)
	MH_ENUM_REFLECT_VALUE(UpdateAvailable)

	MH_ENUM_REFLECT_VALUE(UpdateToolRequired)
	MH_ENUM_REFLECT_VALUE(UpdateToolDownloading)
	MH_ENUM_REFLECT_VALUE(UpdateToolDownloadFailed)
	MH_ENUM_REFLECT_VALUE(UpdateToolDownloadSuccess)

	MH_ENUM_REFLECT_VALUE(Downloading)
	MH_ENUM_REFLECT_VALUE(DownloadFailed)
	MH_ENUM_REFLECT_VALUE(DownloadSuccess)

	MH_ENUM_REFLECT_VALUE(Updating)
	MH_ENUM_REFLECT_VALUE(UpdateFailed)
	MH_ENUM_REFLECT_VALUE(UpdateSuccess)
MH_ENUM_REFLECT_END()
