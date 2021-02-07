#pragma once

#include "Clock.h"
#include "SteamID.h"

#include <optional>

namespace tf2_bot_detector::DB
{
	struct AccountAgeInfo
	{
		constexpr AccountAgeInfo() = default;
		constexpr AccountAgeInfo(SteamID id, time_point_t creationTime) :
			m_ID(id), m_CreationTime(creationTime)
		{
		}

		SteamID m_ID;
		time_point_t m_CreationTime{};
	};

	struct LogsTFCacheInfo
	{
		constexpr LogsTFCacheInfo() = default;
		constexpr LogsTFCacheInfo(SteamID id, time_point_t lastUpdateTime, uint32_t logCount) :
			m_ID(id), m_LastUpdateTime(lastUpdateTime), m_LogCount(logCount)
		{
		}

		SteamID m_ID;
		time_point_t m_LastUpdateTime{};
		uint32_t m_LogCount{};
	};

	class ITempDB
	{
	public:
		virtual ~ITempDB() = default;

		static std::unique_ptr<ITempDB> Create();

		virtual void Store(const AccountAgeInfo& info) = 0;
		[[nodiscard]] virtual bool TryGet(AccountAgeInfo& info) const = 0;
		virtual void GetNearestAccountAgeInfos(SteamID id, std::optional<AccountAgeInfo>& lower, std::optional<AccountAgeInfo>& upper) const = 0;

		virtual void Store(const LogsTFCacheInfo& info) = 0;
		[[nodiscard]] virtual bool TryGet(LogsTFCacheInfo& info) const = 0;
	};
}
