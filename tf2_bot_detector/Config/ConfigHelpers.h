#pragma once
#include "Log.h"

#include <mh/coroutine/task.hpp>
#include <mh/coroutine/thread.hpp>
#include <mh/reflection/enum.hpp>
#include <mh/text/format.hpp>
#include <nlohmann/json_fwd.hpp>

#include <cassert>
#include <filesystem>
#include <optional>
#include <vector>

namespace tf2_bot_detector
{
	class IHTTPClient;
	class Settings;

	enum class ConfigFileType
	{
		User,
		Official,
		ThirdParty,

		COUNT,
	};

	enum class ConfigErrorType
	{
		Success = 0,

		ReadFileFailed,
		JSONParseFailed,
		SchemaValidationFailed,
		DeserializeFailed,
		SerializeFailed,
		SerializedSchemaValidationFailed,
		WriteFileFailed,
		PostLoadFailed,
	};
	const std::error_category& ConfigErrorCategory();
	std::error_condition make_error_condition(ConfigErrorType e);

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
	template<typename CharT, typename Traits>
	std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const ConfigSchemaInfo& info)
	{
		return os << "https://raw.githubusercontent.com/PazerOP/tf2_bot_detector/"
			<< info.m_Branch << "/schemas/v"
			<< info.m_Version << '/'
			<< info.m_Type << ".schema.json";
	}

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

		mh::task<std::error_condition> LoadFileAsync(const std::filesystem::path& filename, std::shared_ptr<const IHTTPClient> client = nullptr);
		std::error_condition SaveFile(const std::filesystem::path& filename) const;

		virtual void ValidateSchema(const ConfigSchemaInfo& schema) const = 0 {}
		virtual void Deserialize(const nlohmann::json& json) = 0 {}
		virtual void Serialize(nlohmann::json& json) const = 0;

		std::optional<ConfigSchemaInfo> m_Schema;
		std::string m_FileName; // Name of the file this was loaded from

	protected:
		virtual void PostLoad(bool deserialized) {}

	private:
		mh::task<std::error_condition> LoadFileInternalAsync(std::filesystem::path filename, std::shared_ptr<const IHTTPClient> client);
	};

	class SharedConfigFileBase : public ConfigFileBase
	{
	public:
		void Deserialize(const nlohmann::json& json) override = 0;
		void Serialize(nlohmann::json& json) const override = 0;

		const std::string& GetName() const;
		ConfigFileInfo GetFileInfo() const;

	private:
		friend class ConfigFileBase;

		std::optional<ConfigFileInfo> m_FileInfo;
	};

	namespace detail
	{
		mh::task<std::error_condition> LoadConfigFileAsync(ConfigFileBase& file, std::filesystem::path filename, bool allowAutoUpdate, const Settings& settings);
	}

	template<typename T, typename = std::enable_if_t<std::is_base_of_v<ConfigFileBase, T>>>
	mh::task<T> LoadConfigFileAsync(std::filesystem::path filename, bool allowAutoUpdate, const Settings& settings)
	{
		T file;
		co_await detail::LoadConfigFileAsync(file, filename, allowAutoUpdate, settings);
		co_return file;
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
				m_UserList = LoadConfigFileAsync<T>(paths.m_User, false, *m_Settings).get();

			if (!paths.m_Official.empty())
				m_OfficialList = LoadConfigFileAsync<T>(paths.m_Official, !IsOfficial(), *m_Settings);
			else
				m_OfficialList = mh::make_ready_task<T>();

			m_ThirdPartyLists = LoadThirdPartyListsAsync(paths);
		}

		void SaveFiles() const
		{
			const T* defaultMutableList = GetDefaultMutableList();
			const T* localList = GetLocalList();
			if (localList)
				localList->SaveFile(mh::format("cfg/{}.json", GetBaseFileName()));

			if (defaultMutableList && defaultMutableList != localList)
			{
				const std::filesystem::path filename = mh::format("cfg/{}.official.json", GetBaseFileName());

				if (!IsOfficial())
					throw std::runtime_error(mh::format("Attempted to save non-official data to {}", filename));

				defaultMutableList->SaveFile(filename);
			}
		}

		bool IsOfficial() const { return m_Settings->GetLocalSteamID().IsPazer(); }

		T& GetDefaultMutableList()
		{
			if (IsOfficial())
				return m_OfficialList.get();

			return GetLocalList();
		}
		const T* GetDefaultMutableList() const
		{
			if (IsOfficial())
			{
				// This might cause occasional UI hiccups, but I am the only one this code path will run for
				return &m_OfficialList.get();
			}

			return GetLocalList();
		}

		T& GetLocalList()
		{
			if (!m_UserList)
				m_UserList.emplace();

			return *m_UserList;
		}
		const T* GetLocalList() const
		{
			if (!m_UserList)
				return nullptr;

			return &*m_UserList;
		}

		size_t size() const
		{
			size_t retVal = 0;

			if (auto list = m_OfficialList.try_get())
				retVal += list->size();
			if (m_UserList)
				retVal += m_UserList->size();
			if (auto list = m_ThirdPartyLists.try_get())
				retVal += list->size();

			return retVal;
		}

		const Settings* m_Settings = nullptr;
		mh::task<T> m_OfficialList;
		std::optional<T> m_UserList;
		mh::task<collection_type> m_ThirdPartyLists;

	private:
		mh::task<collection_type> LoadThirdPartyListsAsync(ConfigFilePaths paths)
		{
			collection_type collection;

			for (const auto& file : paths.m_Others)
			{
				try
				{
					auto parsedFile = co_await LoadConfigFileAsync<T>(file, true, *m_Settings);
					CombineEntries(collection, parsedFile);
				}
				catch (...)
				{
					LogException(MH_SOURCE_LOCATION_CURRENT(), "Exception when loading {}", file);
				}
			}

			co_return collection;
		}
	};
}

MH_ENUM_REFLECT_BEGIN(tf2_bot_detector::ConfigErrorType)
	MH_ENUM_REFLECT_VALUE(Success)

	MH_ENUM_REFLECT_VALUE(ReadFileFailed)
	MH_ENUM_REFLECT_VALUE(JSONParseFailed)
	MH_ENUM_REFLECT_VALUE(SchemaValidationFailed)
	MH_ENUM_REFLECT_VALUE(DeserializeFailed)
	MH_ENUM_REFLECT_VALUE(SerializeFailed)
	MH_ENUM_REFLECT_VALUE(SerializedSchemaValidationFailed)
	MH_ENUM_REFLECT_VALUE(WriteFileFailed)
	MH_ENUM_REFLECT_VALUE(PostLoadFailed)
MH_ENUM_REFLECT_END()

namespace std
{
	template<> struct is_error_condition_enum<tf2_bot_detector::ConfigErrorType> : true_type {};
}
