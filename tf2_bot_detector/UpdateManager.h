#pragma once

#include "Version.h"

#include <mh/reflection/enum.hpp>

#include <memory>

namespace tf2_bot_detector
{
	enum class ReleaseChannel;
	class Settings;

	class IAvailableUpdate
	{
	public:
		virtual ~IAvailableUpdate() = default;

		virtual ReleaseChannel GetReleaseChannel() const = 0;
		virtual Version GetVersion() const = 0;

		virtual bool CanSelfUpdate() const = 0;
		virtual void BeginSelfUpdate() const = 0;
	};

	enum class UpdateStatus
	{
		Unknown = 0,

		InternalError_UpdateCheckNull,
		InternalError_UnknownVariantState,

		UpdateCheckDisabled,
		InternetAccessDisabled,

		CheckQueued,
		Checking,

		CheckFailed,
		UpToDate,
		UpdateAvailable,

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
		virtual UpdateStatus GetUpdateStatus() const = 0;

		virtual const IAvailableUpdate* GetAvailableUpdate() const = 0;
	};
}

MH_ENUM_REFLECT_BEGIN(tf2_bot_detector::UpdateStatus)
	MH_ENUM_REFLECT_VALUE(Unknown)

	MH_ENUM_REFLECT_VALUE(InternalError_UpdateCheckNull)

	MH_ENUM_REFLECT_VALUE(UpdateCheckDisabled)
	MH_ENUM_REFLECT_VALUE(InternetAccessDisabled)

	MH_ENUM_REFLECT_VALUE(Checking)

	MH_ENUM_REFLECT_VALUE(CheckFailed)
	MH_ENUM_REFLECT_VALUE(UpToDate)
	MH_ENUM_REFLECT_VALUE(UpdateAvailable)

	MH_ENUM_REFLECT_VALUE(Updating)

	MH_ENUM_REFLECT_VALUE(UpdateFailed)
	MH_ENUM_REFLECT_VALUE(UpdateSuccess)
MH_ENUM_REFLECT_END()
