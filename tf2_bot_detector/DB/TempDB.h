#pragma once

#include "Networking/LogsTFAPI.h"
#include "Clock.h"
#include "SteamID.h"

#include <optional>

namespace tf2_bot_detector::DB
{
	namespace detail
	{
		struct ICacheInfo
		{
			virtual ~ICacheInfo() = default;

			SteamID& GetSteamID() { return const_cast<SteamID&>(const_cast<const ICacheInfo&>(*this).GetSteamID()); }
			virtual const SteamID& GetSteamID() const = 0;

			virtual std::optional<time_point_t> GetCacheCreationTime() const;
			virtual bool SetCacheCreationTime(time_point_t cacheCreationTime);
		};

		struct BaseCacheInfo_SteamID : virtual ICacheInfo
		{
			using ICacheInfo::GetSteamID;
			const SteamID& GetSteamID() const override final { return m_SteamID; }

			SteamID m_SteamID;
		};

		struct BaseCacheInfo_Expiration : virtual ICacheInfo
		{
			std::optional<time_point_t> GetCacheCreationTime() const override final;
			bool SetCacheCreationTime(time_point_t cacheCreationTime) override final;
			time_point_t m_LastCacheUpdateTime;
		};
	}

	struct AccountAgeInfo final : detail::BaseCacheInfo_SteamID
	{
		time_point_t m_CreationTime{};
	};

	struct LogsTFCacheInfo final : detail::BaseCacheInfo_Expiration, LogsTFAPI::PlayerLogsInfo
	{
		LogsTFCacheInfo() = default;
		using LogsTFAPI::PlayerLogsInfo::PlayerLogsInfo; 
		using LogsTFAPI::PlayerLogsInfo::operator=;

		using ICacheInfo::GetSteamID;
		const SteamID& GetSteamID() const override { return m_ID; }
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
