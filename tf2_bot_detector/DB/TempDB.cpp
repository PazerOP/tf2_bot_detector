#define DB_HELPERS_OK 1

#include "TempDB.h"
#include "DBHelpers.h"
#include "Filesystem.h"
#include "SteamID.h"

#include <mh/error/ensure.hpp>
#include <mh/concurrency/thread_sentinel.hpp>
#include <mh/types/enum_class_bit_ops.hpp>
#include <SQLiteCpp/SQLiteCpp.h>

#include <cassert>

using namespace tf2_bot_detector;
using namespace tf2_bot_detector::DB;

namespace
{
	class TempDB final : public ITempDB
	{
	public:
		TempDB();

		void Store(const AccountAgeInfo& info) override;
		bool TryGet(AccountAgeInfo& info) const override;

		void Store(const LogsTFCacheInfo& info) override;
		bool TryGet(LogsTFCacheInfo& info) const override;

	private:
		static constexpr size_t DB_VERSION = 3;
		//void CreateTableVersionsTable();
		void CreateAccountAgesTable();
		void CreateLogsTFCacheTable();
		void Connect();

		std::optional<SQLite::Database> m_Connection;
	};

	static std::string CreateDBPath()
	{
		const auto folderPath = IFilesystem::Get().ResolvePath("temp/db", PathUsage::WriteLocal);
		std::filesystem::create_directories(folderPath);
		return (folderPath / "tf2bd_temp_db.sqlite").string();
	}

#if 0
	struct TABLE_VERSIONS
	{
		static constexpr char TABLENAME[] = "TABLE_VERSIONS";
		static constexpr char COL_TABLE_NAME[] = "TableName";
		static constexpr char COL_VERSION[] = "Version";
	};
#endif

	struct TABLE_ACCOUNT_AGES
	{
		static constexpr char TABLENAME[] = "TABLE_ACCOUNT_AGES";

		static constexpr ColumnDefinition COL_ACCOUNT_ID{ "AccountID", ColumnType::Integer, ColumnFlags::PrimaryKeyDefaults };
		static constexpr ColumnDefinition COL_CREATION_TIME{ "CreationTime", ColumnType::Integer, ColumnFlags::NotNull };
	};

	struct TABLE_LOGSTF_CACHE
	{
		static constexpr char TABLENAME[] = "TABLE_LOGSTF_CACHE";

		static constexpr ColumnDefinition COL_ACCOUNT_ID{ "AccountID", ColumnType::Integer, ColumnFlags::PrimaryKeyDefaults };
		static constexpr ColumnDefinition COL_LAST_UPDATE_TIME{ "LastUpdateTime", ColumnType::Integer, ColumnFlags::NotNull };
		static constexpr ColumnDefinition COL_LOG_COUNT{ "LogCount", ColumnType::Integer, ColumnFlags::NotNull };
	};

	TempDB::TempDB() try
	{
		Connect();

		// Delete and recreate the DB if its an old version
		if (const auto currentUserVersion = m_Connection->execAndGet(mh::format("PRAGMA user_version")).getInt();
			currentUserVersion != DB_VERSION)
		{
			LogWarning("Current {} version = {}. Deleting and recreating...", CreateDBPath(), currentUserVersion);
			m_Connection.reset();
			std::filesystem::remove(CreateDBPath());
			Connect();
			m_Connection->exec(mh::format("PRAGMA user_version = {}", DB_VERSION)); // TODO check current user_version and delete if different
		}

		m_Connection->exec("PRAGMA journal_mode = WAL;");

		//CreateTableVersionsTable();
		CreateAccountAgesTable();
		CreateLogsTFCacheTable();
	}
	catch (...)
	{
		LogException();
		throw;
	}

	void TempDB::Store(const AccountAgeInfo& info) try
	{
		ReplaceInto(m_Connection.value(), TABLE_ACCOUNT_AGES::TABLENAME,
			{
				{ TABLE_ACCOUNT_AGES::COL_ACCOUNT_ID, info.m_ID.GetAccountID() },
				{ TABLE_ACCOUNT_AGES::COL_CREATION_TIME, std::chrono::duration_cast<std::chrono::seconds>(info.m_CreationTime.time_since_epoch()).count() },
			});
	}
	catch (...)
	{
		LogException();
		throw;
	}

	bool TempDB::TryGet(AccountAgeInfo& info) const try
	{
		auto& db = const_cast<SQLite::Database&>(m_Connection.value());

		SQLite::Statement query(db, mh::format("SELECT * FROM \"{}\" WHERE \"{}\" = ?1",
			TABLE_ACCOUNT_AGES::TABLENAME, TABLE_ACCOUNT_AGES::COL_ACCOUNT_ID));
		query.bind("?accID", info.m_ID.GetAccountID());

		if (query.executeStep())
		{
			info.m_CreationTime = time_point_t(std::chrono::seconds(query.getColumn(TABLE_ACCOUNT_AGES::COL_CREATION_TIME).getInt64()));
			return true;
		}

		return false;
	}
	catch (...)
	{
		LogException();
		throw;
	}

	void TempDB::Connect()
	{
		assert(!m_Connection.has_value());
		m_Connection.emplace(CreateDBPath(), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
	}

#if 0
	void TempDB::CreateTableVersionsTable() try
	{
		CreateTable(m_Connection, TABLE_VERSIONS::TABLENAME,
			{
				{ TABLE_VERSIONS::COL_TABLE_NAME, ColumnType::Text, ColumnFlags::NotNull | ColumnFlags::PrimaryKey | ColumnFlags::Unique },
				{ TABLE_VERSIONS::COL_VERSION, ColumnType::Integer, ColumnFlags::NotNull },
			}, CreateTableFlags::IfNotExists);
	}
	catch (...)
	{
		LogException();
		throw;
	}
#endif

	void TempDB::CreateAccountAgesTable() try
	{
		CreateTable(m_Connection.value(), TABLE_ACCOUNT_AGES::TABLENAME,
			{
				TABLE_ACCOUNT_AGES::COL_ACCOUNT_ID,
				TABLE_ACCOUNT_AGES::COL_CREATION_TIME,
			}, CreateTableFlags::IfNotExists);

#if 0
		InsertInto(m_Connection, TABLE_VERSIONS::TABLENAME,
			{
				{ TABLE_VERSIONS::COL_TABLE_NAME, TABLE_ACCOUNT_AGES::TABLENAME },

			});
#endif
	}
	catch (...)
	{
		LogException();
		throw;
	}

	void TempDB::CreateLogsTFCacheTable() try
	{
		CreateTable(m_Connection.value(), TABLE_LOGSTF_CACHE::TABLENAME,
			{
				TABLE_LOGSTF_CACHE::COL_ACCOUNT_ID,
				TABLE_LOGSTF_CACHE::COL_LAST_UPDATE_TIME,
				TABLE_LOGSTF_CACHE::COL_LOG_COUNT,
			}, CreateTableFlags::IfNotExists);
	}
	catch (...)
	{
		LogException();
		throw;
	}

	void TempDB::Store(const LogsTFCacheInfo& info) try
	{
		ReplaceInto(m_Connection.value(), TABLE_LOGSTF_CACHE::TABLENAME,
			{
				{ TABLE_LOGSTF_CACHE::COL_ACCOUNT_ID, info.m_ID.GetAccountID() },
				{ TABLE_LOGSTF_CACHE::COL_LAST_UPDATE_TIME, std::chrono::duration_cast<std::chrono::seconds>(info.m_LastUpdateTime.time_since_epoch()).count() },
				{ TABLE_LOGSTF_CACHE::COL_LOG_COUNT, info.m_LogCount }
			});
	}
	catch (...)
	{
		LogException();
		throw;
	}

	bool TempDB::TryGet(LogsTFCacheInfo& info) const
	{
		auto& db = const_cast<SQLite::Database&>(m_Connection.value());

		SQLite::Statement query(db, mh::format("SELECT * FROM \"{}\" WHERE \"{}\" = $accID",
			TABLE_LOGSTF_CACHE::TABLENAME, TABLE_LOGSTF_CACHE::COL_ACCOUNT_ID));
		query.bind("$accID", info.m_ID.GetAccountID());

		if (query.executeStep())
		{
			info.m_LastUpdateTime = time_point_t(std::chrono::seconds(query.getColumn(TABLE_LOGSTF_CACHE::COL_LAST_UPDATE_TIME).getInt64()));
			info.m_LogCount = query.getColumn(TABLE_LOGSTF_CACHE::COL_LOG_COUNT).getUInt();
			return true;
		}

		return false;
	}
}
std::unique_ptr<ITempDB> tf2_bot_detector::DB::ITempDB::Create()
{
	return std::make_unique<TempDB>();
}
