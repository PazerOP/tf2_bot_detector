#pragma once

#include "Networking/LogsTFAPI.h"
#include "Networking/SteamAPI.h"
#include "Clock.h"
#include "SteamID.h"

#include <mh/coroutine/task.hpp>
#include <mh/memory/stack_info.hpp>

#include <cassert>
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
		};

		struct BaseCacheInfo_SteamID : virtual ICacheInfo
		{
			using ICacheInfo::GetSteamID;
			const SteamID& GetSteamID() const override final { return m_SteamID; }

			SteamID m_SteamID;
		};

		struct BaseCacheInfo_Expiration : virtual ICacheInfo
		{
			virtual duration_t GetCacheLiveTime() const = 0;
			time_point_t m_LastCacheUpdateTime;
		};
	}

	struct AccountAgeInfo final : detail::BaseCacheInfo_SteamID
	{
		time_point_t m_CreationTime{};
	};

	struct AccountInventorySizeInfo final : detail::BaseCacheInfo_SteamID, detail::BaseCacheInfo_Expiration, SteamAPI::PlayerInventoryInfo
	{
		AccountInventorySizeInfo() = default;
		using SteamAPI::PlayerInventoryInfo::PlayerInventoryInfo;
		using SteamAPI::PlayerInventoryInfo::operator=;

		duration_t GetCacheLiveTime() const override { return day_t(7); }
	};

	struct LogsTFCacheInfo final : detail::BaseCacheInfo_Expiration, LogsTFAPI::PlayerLogsInfo
	{
		LogsTFCacheInfo() = default;
		using LogsTFAPI::PlayerLogsInfo::PlayerLogsInfo;
		using LogsTFAPI::PlayerLogsInfo::operator=;

		using ICacheInfo::GetSteamID;
		const SteamID& GetSteamID() const override { return m_ID; }

		duration_t GetCacheLiveTime() const override final { return day_t(7); }
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

		virtual void Store(const AccountInventorySizeInfo& info) = 0;
		[[nodiscard]] virtual bool TryGet(AccountInventorySizeInfo& info) const = 0;

		template<typename TInfo, typename TUpdateFunc>
		mh::task<> GetOrUpdateAsync(TInfo& info, TUpdateFunc&& updateFunc)
		{
			assert(!mh::is_variable_on_current_stack(info));
			assert(!mh::is_variable_on_current_stack(updateFunc));

			constexpr bool HAS_EXPIRATION = std::is_base_of_v<detail::BaseCacheInfo_Expiration, TInfo>;

			bool wantsRefresh = true;
			if (TryGet(info))
			{
				if constexpr (HAS_EXPIRATION)
				{
					auto elapsed = tfbd_clock_t::now() - info.m_LastCacheUpdateTime;
					if (elapsed <= info.GetCacheLiveTime())
						wantsRefresh = false;
				}
				else
				{
					wantsRefresh = false;
				}
			}

			if (wantsRefresh)
			{
				co_await updateFunc(info);

				if constexpr (HAS_EXPIRATION)
					info.m_LastCacheUpdateTime = tfbd_clock_t::now();

				Store(info);
			}
		}
	};
}
