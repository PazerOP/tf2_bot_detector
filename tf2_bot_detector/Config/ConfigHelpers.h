#pragma once
#include "Log.h"
#include "Settings.h"

#include <mh/future.hpp>
#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <future>
#include <optional>
#include <vector>

namespace tf2_bot_detector
{
	enum class ConfigFileType
	{
		User,
		Official,
		ThirdParty,

		COUNT,
	};

	struct ConfigFilePaths
	{
		std::filesystem::path m_User;
		std::filesystem::path m_Official;
		std::vector<std::filesystem::path> m_Others;
	};
	ConfigFilePaths GetConfigFilePaths(const std::string_view& basename);

	struct ConfigSchemaInfo
	{
		explicit ConfigSchemaInfo(std::nullptr_t) {}
		ConfigSchemaInfo(const std::string_view& schema);
		ConfigSchemaInfo(std::string type, unsigned version, std::string branch = "master");

		std::string m_Branch;
		std::string m_Type;
		unsigned m_Version{};
	};

	void to_json(nlohmann::json& j, const ConfigSchemaInfo& d);
	void from_json(const nlohmann::json& j, ConfigSchemaInfo& d);

	struct ConfigFileInfo
	{
		std::vector<std::string> m_Authors;
		std::string m_Title;
		std::string m_Description;
		std::string m_UpdateURL;
	};

	void to_json(nlohmann::json& j, const ConfigFileInfo& d);
	void from_json(const nlohmann::json& j, ConfigFileInfo& d);

	class ConfigFileBase
	{
	public:
		virtual ~ConfigFileBase() = default;

		bool LoadFile(const std::filesystem::path& filename, const HTTPClient* client = nullptr);
		bool SaveFile(const std::filesystem::path& filename) const;

		virtual void ValidateSchema(const ConfigSchemaInfo& schema) const = 0 {}
		virtual void Deserialize(const nlohmann::json& json) = 0 {}
		virtual void Serialize(nlohmann::json& json) const = 0;

		std::optional<ConfigSchemaInfo> m_Schema;

	private:
		bool LoadFileInternal(const std::filesystem::path& filename, const HTTPClient* client);
	};

	class SharedConfigFileBase : public ConfigFileBase
	{
	public:
		void Deserialize(const nlohmann::json& json) override = 0;
		void Serialize(nlohmann::json& json) const override = 0;

		std::optional<ConfigFileInfo> m_FileInfo;
	};

	template<typename T, typename = std::enable_if_t<std::is_base_of_v<ConfigFileBase, T>>>
	T LoadConfigFile(const std::filesystem::path& filename, bool allowAutoupdate, const Settings& settings)
	{
		const HTTPClient* client = allowAutoupdate ? settings.GetHTTPClient() : nullptr;
		if (allowAutoupdate && !client)
			Log(std::string("Disallowing auto-update of ") << filename << " because internet connectivity is disabled or unset in settings");

		// Not going to be doing any async loading
		if (T file; file.LoadFile(filename, client))
			return file;

		return T{};
	}

	template<typename T, typename = std::enable_if_t<std::is_base_of_v<ConfigFileBase, T>>>
	auto LoadConfigFileAsync(std::filesystem::path filename, bool allowAutoupdate, const Settings& settings)
	{
		if constexpr (std::is_base_of_v<SharedConfigFileBase, T>)
		{
			if (allowAutoupdate)
			{
				return std::async([filename, &settings]
					{
						return LoadConfigFile<T>(filename, true, settings);
					});
			}
			else
			{
				return mh::make_ready_future(LoadConfigFile<T>(filename, false, settings));
			}
		}
		else
		{
			assert(!allowAutoupdate);
			return LoadConfigFile<T>(filename, allowAutoupdate, settings);
		}
	}

	template<typename T, typename TOthers = typename T::collection_type>
	class ConfigFileGroupBase
	{
	public:
		using collection_type = TOthers;

		ConfigFileGroupBase(const Settings& settings) : m_Settings(&settings) {}
		virtual ~ConfigFileGroupBase() = default;

		virtual void CombineEntries(collection_type& collection, const T& file) const = 0;
		virtual std::string GetBaseFileName() const = 0;

		void LoadFiles()
		{
			using namespace std::string_literals;

			const auto paths = GetConfigFilePaths(GetBaseFileName());

			if (!IsOfficial() && !paths.m_User.empty())
				m_UserList = LoadConfigFile<T>(paths.m_User, false, *m_Settings);

			if (!paths.m_Official.empty())
				m_OfficialList = LoadConfigFileAsync<T>(paths.m_Official, !IsOfficial(), *m_Settings);
			else
				m_OfficialList = {};

			m_ThirdPartyLists = std::async([this, paths]
				{
					collection_type collection;

					for (const auto& file : paths.m_Others)
					{
						try
						{
							auto parsedFile = LoadConfigFile<T>(file, true, *m_Settings);
							CombineEntries(collection, parsedFile);
						}
						catch (const std::exception& e)
						{
							LogError("Exception when loading "s << file << ": " << e.what());
						}
					}

					return collection;
				});
		}

		void SaveFile() const
		{
			using namespace std::string_literals;

			auto mutableList = GetMutableList();
			if (!mutableList)
				return; // Nothing to save

			mutableList->SaveFile(IsOfficial() ?
				("cfg/"s << GetBaseFileName() << ".official.json") :
				("cfg/"s << GetBaseFileName() << ".json"));
		}

		bool IsOfficial() const { return m_Settings->GetLocalSteamID().IsPazer(); }

		T& GetMutableList()
		{
			if (IsOfficial())
				return const_cast<T&>(m_OfficialList.get()); // forgive me for i have sinned

			if (!m_UserList)
				m_UserList.emplace();

			return m_UserList.value();
		}
		const T* GetMutableList() const
		{
			if (IsOfficial())
				return &m_OfficialList.get();

			if (!m_UserList)
				return nullptr;

			return &m_UserList.value();
		}

		size_t size() const
		{
			size_t retVal = 0;

			if (mh::is_future_ready(m_OfficialList))
				retVal += m_OfficialList.get().size();
			if (m_UserList)
				retVal += m_UserList->size();
			if (mh::is_future_ready(m_ThirdPartyLists))
				retVal += m_ThirdPartyLists.get().size();

			return retVal;
		}

		const Settings* m_Settings = nullptr;
		std::shared_future<T> m_OfficialList;
		std::optional<T> m_UserList;
		std::shared_future<collection_type> m_ThirdPartyLists;
	};
}

template<typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const tf2_bot_detector::ConfigSchemaInfo& info)
{
	return os << "https://raw.githubusercontent.com/PazerOP/tf2_bot_detector/"
		<< info.m_Branch << "/schemas/v"
		<< info.m_Version << '/'
		<< info.m_Type << ".schema.json";
}
