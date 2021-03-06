#pragma once

#include "ConfigHelpers.h"

#include <mh/coroutine/task.hpp>
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

		const Map* FindMap(const std::string_view& name) const;

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

		mh::task<DRPFile> m_DRPInfo;
	};
}
