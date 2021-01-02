#include "AccountAges.h"
#include "ConfigHelpers.h"
#include "SteamID.h"
#include "Util/JSONUtils.h"

#include <mh/concurrency/thread_sentinel.hpp>
#include <mh/error/not_implemented_error.hpp>
#include <mh/math/interpolation.hpp>
#include <nlohmann/json.hpp>

#include <cassert>

using namespace tf2_bot_detector;

namespace
{
	class AccountAges final : public IAccountAges, ConfigFileBase
	{
	public:
		AccountAges();

		void OnDataReady(const SteamID& id, time_point_t creationTime) override;

		std::optional<time_point_t> EstimateAccountCreationTime(const SteamID& id) const override;

	private:
		[[nodiscard]] bool CheckSteamIDValid(const SteamID& id, MH_SOURCE_LOCATION_AUTO(location)) const;

		mh::thread_sentinel m_Sentinel;

		std::map<uint64_t, time_point_t> m_Ages;

		// Inherited via ConfigFileBase
		static constexpr int ACCOUNT_AGES_SCHEMA_VERSION = 3;
		static constexpr char ACCOUNT_AGES_SCHEMA_NAME[] = "account_ages";
		void ValidateSchema(const ConfigSchemaInfo& schema) const override;
		void Deserialize(const nlohmann::json& json) override;
		void Serialize(nlohmann::json& json) const override;
	};

	static const std::filesystem::path ACCOUNT_AGES_FILENAME = "cfg/account_ages.json";
}

std::shared_ptr<IAccountAges> tf2_bot_detector::IAccountAges::Create()
{
	return std::make_shared<AccountAges>();
}

AccountAges::AccountAges()
{
	LoadFileAsync(ACCOUNT_AGES_FILENAME).wait();
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

	if (!m_Ages.empty())
	{
		auto lower = m_Ages.lower_bound(id.ID);
		if (lower != m_Ages.begin())
			--lower;

		constexpr uint64_t MIN_ID_DIST = 10'000;

		if ((id.ID - lower->first) < MIN_ID_DIST)
			return;  // Too close to previous ID

		auto upper = m_Ages.upper_bound(id.ID);
		if (upper != m_Ages.end() && (upper->first - id.ID) < MIN_ID_DIST)
			return;  // Too close to next id
	}

	m_Ages.insert({ id.ID, creationTime });

	SaveFile(ACCOUNT_AGES_FILENAME);
}

std::optional<time_point_t> AccountAges::EstimateAccountCreationTime(const SteamID& id) const
{
	if (!CheckSteamIDValid(id))
		return std::nullopt;

	if (m_Ages.empty())
		return std::nullopt;

	m_Sentinel.check();

	auto lower = m_Ages.lower_bound(id.ID);
	if (lower == m_Ages.end())
		return std::nullopt;  // super new, we don't have any data for this

	if (lower->first == id.ID)
		return lower->second; // Exact match

	auto upper = m_Ages.upper_bound(id.ID);
	assert(upper == lower);
	if (lower != m_Ages.begin())
		--lower;

	if (upper == m_Ages.end())
		return lower->second; // Nothing to interpolate to, pick the lower value

	const auto interpValue = mh::remap(id.ID,
		lower->first, upper->first,
		lower->second.time_since_epoch().count(), upper->second.time_since_epoch().count());

	return time_point_t(time_point_t::duration(interpValue));
}

void AccountAges::ValidateSchema(const ConfigSchemaInfo& schema) const
{
	ConfigFileBase::ValidateSchema(schema);

	if (schema.m_Type != ACCOUNT_AGES_SCHEMA_NAME)
		throw std::runtime_error(mh::format("Schema {} is not an account ages file", schema.m_Type));
	if (schema.m_Version != 3)
		throw std::runtime_error(mh::format("Schema must be version {} (current version {})", ACCOUNT_AGES_SCHEMA_VERSION, schema.m_Version));
}

void AccountAges::Deserialize(const nlohmann::json& json)
{
	ConfigFileBase::Deserialize(json);

	m_Sentinel.check();

	auto& accounts = json.at("accounts");

	for (const nlohmann::json& account : accounts)
	{
		uint64_t id, creationTime;
		if (!try_get_to_defaulted(account, id, "id"))
			continue;
		if (!try_get_to_defaulted(account, creationTime, "creation_time"))
			continue;

		m_Ages.emplace(id, time_point_t(std::chrono::seconds(creationTime)));
	}
}

void AccountAges::Serialize(nlohmann::json& json) const
{
	ConfigFileBase::Serialize(json);

	json["$schema"] = ConfigSchemaInfo(ACCOUNT_AGES_SCHEMA_NAME, ACCOUNT_AGES_SCHEMA_VERSION);

	auto& accounts = json["accounts"] = json.array();

	for (const auto& [id, creationTime] : m_Ages)
	{
		auto& account = accounts.emplace_back(json.object());
		account["id"] = id;
		account["creation_time"] = std::chrono::duration_cast<std::chrono::seconds>(creationTime.time_since_epoch()).count();
	}
}
