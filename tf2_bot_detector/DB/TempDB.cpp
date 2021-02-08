#define DB_HELPERS_OK 1

#include "TempDB.h"
#include "DBHelpers.h"
#include "Filesystem.h"
#include "SteamID.h"

#include <mh/error/ensure.hpp>
#include <mh/concurrency/thread_sentinel.hpp>
#include <mh/types/enum_class_bit_ops.hpp>
#include <sqlite3.h>
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
		void GetNearestAccountAgeInfos(SteamID id, std::optional<AccountAgeInfo>& lower, std::optional<AccountAgeInfo>& upper) const override;

		void Store(const LogsTFCacheInfo& info) override;
		bool TryGet(LogsTFCacheInfo& info) const override;

	private:
		static constexpr size_t DB_VERSION = 3;
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

	struct BASETABLE
	{
		static constexpr ColumnDefinition COL_ACCOUNT_ID{ "AccountID", ColumnType::Integer, ColumnFlags::PrimaryKeyDefaults };
	};

	struct BASETABLE_EXPIRABLE : BASETABLE
	{
		static constexpr ColumnDefinition COL_LAST_UPDATE_TIME{ "LastUpdateTime", ColumnType::Integer, ColumnFlags::NotNull };
	};

	struct TABLE_ACCOUNT_AGES : BASETABLE
	{
		static constexpr char TABLENAME[] = "TABLE_ACCOUNT_AGES";

		static constexpr ColumnDefinition COL_CREATION_TIME{ "CreationTime", ColumnType::Integer, ColumnFlags::NotNull };
	};

	struct TABLE_LOGSTF_CACHE : BASETABLE_EXPIRABLE
	{
		static constexpr char TABLENAME[] = "TABLE_LOGSTF_CACHE";

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

		CreateAccountAgesTable();
		CreateLogsTFCacheTable();
	}
	catch (...)
	{
		LogException();
		throw;
	}
}

namespace tf2_bot_detector::DB
{
	template<>
	struct ColumnDataSerializer<time_point_t>
	{
		static int64_t Serialize(time_point_t time)
		{
			return std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();
		}
		static time_point_t Deserialize(const SQLite::Column& column)
		{
			return time_point_t(std::chrono::seconds(column.getInt64()));
		}
	};

	template<>
	struct ColumnDataSerializer<SteamID>
	{
		static uint32_t Serialize(SteamID id)
		{
			assert(id.Type == SteamAccountType::Individual);
			return id.GetAccountID();
		}
		static SteamID Deserialize(const SQLite::Column& column)
		{
			return SteamID(column.getUInt(), SteamAccountType::Individual);
		}
	};
}

namespace
{
	static int64_t ToSeconds(time_point_t timePoint)
	{
		return std::chrono::duration_cast<std::chrono::seconds>(timePoint.time_since_epoch()).count();
	}
	static time_point_t ToTimePoint(int64_t seconds)
	{
		return time_point_t(std::chrono::seconds(seconds));
	}

	void TempDB::Store(const AccountAgeInfo& info) try
	{
		ReplaceInto(m_Connection.value(), TABLE_ACCOUNT_AGES::TABLENAME,
			{
				{ TABLE_ACCOUNT_AGES::COL_ACCOUNT_ID, info.m_SteamID },
				{ TABLE_ACCOUNT_AGES::COL_CREATION_TIME, info.m_CreationTime },
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

		auto query = SelectStatementBuilder(TABLE_ACCOUNT_AGES::TABLENAME)
			.Where(TABLE_ACCOUNT_AGES::COL_ACCOUNT_ID == info.m_SteamID)
			.Run(db);

		if (query.executeStep())
		{
			info.m_CreationTime = query.getColumn(TABLE_ACCOUNT_AGES::COL_CREATION_TIME);
			return true;
		}

		return false;
	}
	catch (...)
	{
		LogException();
		throw;
	}

	void TempDB::GetNearestAccountAgeInfos(SteamID id, std::optional<AccountAgeInfo>& lower, std::optional<AccountAgeInfo>& upper) const
	{
		auto queryStr = mh::format(R"SQL(
SELECT max({col_AccountID}) AS {col_AccountID}, {col_CreationTime} FROM {tbl_AccountAges} WHERE {col_AccountID} <= $steamID
UNION ALL
SELECT min({col_AccountID}) AS {col_AccountID}, {col_CreationTime} FROM {tbl_AccountAges} WHERE {col_AccountID} >= $steamID)SQL",

			mh::fmtarg("col_AccountID", TABLE_ACCOUNT_AGES::COL_ACCOUNT_ID.m_Name),
			mh::fmtarg("col_CreationTime", TABLE_ACCOUNT_AGES::COL_CREATION_TIME.m_Name),
			mh::fmtarg("tbl_AccountAges", TABLE_ACCOUNT_AGES::TABLENAME));

		Statement2 query(SQLite::Statement(const_cast<SQLite::Database&>(m_Connection.value()), queryStr));
		query.bind("$steamID", id.GetAccountID());

		const auto DeserializeAccountInfo = [&]()
		{
			AccountAgeInfo info;
			info.m_SteamID = query.getColumn(TABLE_ACCOUNT_AGES::COL_ACCOUNT_ID);
			info.m_CreationTime = query.getColumn(TABLE_ACCOUNT_AGES::COL_CREATION_TIME);
			return info;
		};

		while (query.executeStep())
		{
			assert(!lower.has_value() || !upper.has_value());

			auto info = DeserializeAccountInfo();
			if (info.m_SteamID == id)
			{
				lower = info;
				upper = info;
				return;
			}

			if (info.m_SteamID.GetAccountID() < id.GetAccountID())
				lower = info;
			else
				upper = info;
		}
	}

	void TempDB::Connect()
	{
		assert(!m_Connection.has_value());
		m_Connection.emplace(CreateDBPath(), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE | SQLite::OPEN_FULLMUTEX);
	}

	void TempDB::CreateAccountAgesTable() try
	{
		CreateTable(m_Connection.value(), TABLE_ACCOUNT_AGES::TABLENAME,
			{
				TABLE_ACCOUNT_AGES::COL_ACCOUNT_ID,
				TABLE_ACCOUNT_AGES::COL_CREATION_TIME,
			}, CreateTableFlags::IfNotExists);
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
				{ TABLE_LOGSTF_CACHE::COL_ACCOUNT_ID, info.GetSteamID() },
				{ TABLE_LOGSTF_CACHE::COL_LAST_UPDATE_TIME, info.m_LastCacheUpdateTime },
				{ TABLE_LOGSTF_CACHE::COL_LOG_COUNT, info.m_LogsCount }
			});
	}
	catch (...)
	{
		LogException();
		throw;
	}

	bool TempDB::TryGet(LogsTFCacheInfo& info) const
	{
		auto query = SelectStatementBuilder(TABLE_LOGSTF_CACHE::TABLENAME)
			.Where(TABLE_LOGSTF_CACHE::COL_ACCOUNT_ID == info.m_ID)
			.Run(m_Connection.value());

		if (query.executeStep())
		{
			info.m_LastCacheUpdateTime = query.getColumn(TABLE_LOGSTF_CACHE::COL_LAST_UPDATE_TIME);
			info.m_LogsCount = query.getColumn(TABLE_LOGSTF_CACHE::COL_LOG_COUNT);
			return true;
		}

		return false;
	}
}

std::unique_ptr<ITempDB> tf2_bot_detector::DB::ITempDB::Create()
{
	return std::make_unique<TempDB>();
}

std::optional<time_point_t> DB::detail::BaseCacheInfo_Expiration::GetCacheCreationTime() const
{
	return m_LastCacheUpdateTime;
}

bool DB::detail::BaseCacheInfo_Expiration::SetCacheCreationTime(time_point_t cacheCreationTime)
{
	m_LastCacheUpdateTime = cacheCreationTime;
	return true;
}

std::optional<time_point_t> DB::detail::ICacheInfo::GetCacheCreationTime() const
{
	return std::nullopt;
}

bool DB::detail::ICacheInfo::SetCacheCreationTime(time_point_t cacheCreationTime)
{
	return false;
}
