#pragma once

#include "Clock.h"
#include "SteamID.h"

#include <optional>

namespace tf2_bot_detector
{
	class SteamID;

	class IAccountAges
	{
	public:
		virtual ~IAccountAges() = default;

		static std::shared_ptr<IAccountAges> Create();

		virtual void OnDataReady(const SteamID& id, time_point_t creationTime) = 0;

		virtual std::optional<time_point_t> EstimateAccountCreationTime(const SteamID& id) const = 0;
	};
}
