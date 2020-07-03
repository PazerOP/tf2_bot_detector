#pragma once

#include "ConfigHelpers.h"

#include <mh/future.hpp>
#include <nlohmann/json_fwd.hpp>

#include <vector>

namespace tf2_bot_detector
{
	class SponsorsList final
	{
	public:
		SponsorsList(const Settings& settings);

		void LoadFile();

		bool IsLoading() const { return mh::is_future_ready(m_Sponsors); }

		struct Sponsor
		{
			std::string m_Name;
			std::string m_Message;
		};

		std::vector<Sponsor> GetSponsors() const;

	private:
		const Settings* m_Settings = nullptr;

		struct SponsorsListFile final : public SharedConfigFileBase
		{
			using BaseClass = SharedConfigFileBase;

			void Deserialize(const nlohmann::json& json) override;
			void Serialize(nlohmann::json& json) const override;
			void ValidateSchema(const ConfigSchemaInfo& schema) const override;

			static constexpr int SPONSORS_SCHEMA_VERSION = 3;

			std::vector<Sponsor> m_Sponsors;
		};

		std::shared_future<SponsorsListFile> m_Sponsors;
	};

	void to_json(nlohmann::json& j, const SponsorsList::Sponsor& d);
	void from_json(const nlohmann::json& j, SponsorsList::Sponsor& d);
}
