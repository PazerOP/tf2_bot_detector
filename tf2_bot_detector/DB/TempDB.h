#pragma once

#include "Clock.h"
#include "SteamID.h"

struct sqlite3;

namespace tf2_bot_detector::DB
{
	struct AccountAgeInfo
	{
		SteamID m_ID;
		time_point_t m_CreationTime{};
	};

	struct LogsTFCacheInfo
	{
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

		virtual void Store(const LogsTFCacheInfo& info) = 0;
		[[nodiscard]] virtual bool TryGet(LogsTFCacheInfo& info) const = 0;
	};
}
