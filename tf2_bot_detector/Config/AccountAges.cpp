#include "AccountAges.h"
#include "ConfigHelpers.h"
#include "SteamID.h"

#include <mh/concurrency/thread_sentinel.hpp>
#include <mh/error/not_implemented_error.hpp>
#include <mh/math/interpolation.hpp>

using namespace tf2_bot_detector;

namespace
{
	class AccountAges final : public IAccountAges
	{
	public:
		void OnDataReady(const SteamID& id, time_point_t creationTime) override;

		std::optional<time_point_t> EstimateAccountCreationTime(const SteamID& id) const override;

	private:
		[[nodiscard]] bool CheckSteamIDValid(const SteamID& id, MH_SOURCE_LOCATION_AUTO(location)) const;

		mh::thread_sentinel m_Sentinel;

		std::map<uint64_t, time_point_t> m_Ages;
	};
}

std::shared_ptr<IAccountAges> tf2_bot_detector::IAccountAges::Create()
{
	return std::make_shared<AccountAges>();
}

bool AccountAges::CheckSteamIDValid(const SteamID& id, const mh::source_location& location) const
{
	if (id.Type != SteamAccountType::Individual)
	{
		LogError(location, "Steam ID ({}) must be for an individual account", id);
		return false;
	}

	return true;
}

void AccountAges::OnDataReady(const SteamID& id, time_point_t creationTime)
{
	if (!CheckSteamIDValid(id))
		return;

	m_Sentinel.check();

	m_Ages.insert({ id.ID, creationTime });
}

std::optional<time_point_t> AccountAges::EstimateAccountCreationTime(const SteamID& id) const
{
	if (!CheckSteamIDValid(id))
		return std::nullopt;

	if (m_Ages.empty())
		return std::nullopt;

	m_Sentinel.check();

	auto lower = m_Ages.lower_bound(id.ID);
	if (lower->first == id.ID)
		return lower->second; // Exact match

	auto upper = m_Ages.upper_bound(id.ID);
	if (upper == m_Ages.end())
		return lower->second; // Nothing to interpolate to

	if (lower != m_Ages.begin())
		--lower;

	const auto interpValue = mh::remap(id.ID,
		lower->first, upper->first,
		lower->second.time_since_epoch().count(), upper->second.time_since_epoch().count());

	return time_point_t(time_point_t::duration(interpValue));
}
