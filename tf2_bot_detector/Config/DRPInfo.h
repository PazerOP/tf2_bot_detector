#pragma once

#include "ConfigHelpers.h"

#include <nlohmann/json_fwd.hpp>

#include <regex>
#include <string>
#include <string_view>

namespace tf2_bot_detector
{
	class DRPInfo final
	{
	public:
		DRPInfo(const Settings& settings);

		void LoadFile();

		struct Map
		{
			std::string GetLargeImageKey() const;
			std::string GetFriendlyName() const;
			bool Matches(const std::string_view& mapName) const;

			std::vector<std::string> m_MapNames;
			std::string m_FriendlyNameOverride;
			std::string m_LargeImageKeyOverride;
		};

	private:
		const Settings* m_Settings = nullptr;

		struct DRPFile final : public SharedConfigFileBase
		{
			using BaseClass = SharedConfigFileBase;

			void Deserialize(const nlohmann::json& json) override;
			void Serialize(nlohmann::json& json) const override;
			void ValidateSchema(const ConfigSchemaInfo& schema) const override;

			static constexpr int DRP_SCHEMA_VERSION = 3;

			std::vector<Map> m_Maps;
		};

		std::shared_future<DRPFile> m_DRPInfo;
	};
}
