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

		void Store(const AccountInventorySizeInfo& info) override;
		bool TryGet(AccountInventorySizeInfo& info) const override;

	private:
		static constexpr size_t DB_VERSION = 4;
		void Connect();

		std::optional<SQLite::Database> m_Connection;
	};

	static std::string CreateDBPath()
	{
		const auto folderPath = IFilesystem::Get().ResolvePath("temp/db", PathUsage::WriteLocal);
		std::filesystem::create_directories(folderPath);
		return (folderPath / "tf2bd_temp_db.sqlite").string();
	}

	struct BASETABLE : TableDefinition
	{
		using TableDefinition::TableDefinition;

		const ColumnDefinition COL_ACCOUNT_ID = Column("AccountID", ColumnType::Integer, ColumnFlags::PrimaryKeyDefaults);
	};

	struct BASETABLE_EXPIRABLE : BASETABLE
	{
		using BASETABLE::BASETABLE;

		const ColumnDefinition COL_LAST_UPDATE_TIME = Column("LastUpdateTime", ColumnType::Integer, ColumnFlags::NotNull);
	};

	struct TABLE_ACCOUNT_AGES final : BASETABLE
	{
		TABLE_ACCOUNT_AGES() : BASETABLE("TABLE_ACCOUNT_AGES") {}

		const ColumnDefinition COL_CREATION_TIME = Column("CreationTime", ColumnType::Integer, ColumnFlags::NotNull);

	} static const s_TableAccountAges;

	struct TABLE_LOGSTF_CACHE final : BASETABLE_EXPIRABLE
	{
		TABLE_LOGSTF_CACHE() : BASETABLE_EXPIRABLE("TABLE_LOGSTF_CACHE") {}

		const ColumnDefinition COL_LOG_COUNT = Column("LogCount", ColumnType::Integer, ColumnFlags::NotNull);

	} static const s_TableLogsTFCache;

	struct TABLE_INVENTORY_SIZE final : BASETABLE_EXPIRABLE
	{
		TABLE_INVENTORY_SIZE() : BASETABLE_EXPIRABLE("TABLE_INVENTORY_SIZE") {}

		const ColumnDefinition COL_ITEM_COUNT = Column("ItemCount", ColumnType::Integer, ColumnFlags::NotNull);
		const ColumnDefinition COL_SLOT_COUNT = Column("SlotCount", ColumnType::Integer, ColumnFlags::NotNull);

	} static const s_TableInventorySize;

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

		CreateTable(m_Connection.value(), s_TableAccountAges, CreateTableFlags::IfNotExists);
		CreateTable(m_Connection.value(), s_TableLogsTFCache, CreateTableFlags::IfNotExists);
		CreateTable(m_Connection.value(), s_TableInventorySize, CreateTableFlags::IfNotExists);
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
	void TempDB::Store(const AccountAgeInfo& info) try
	{
		ReplaceInto(m_Connection.value(), s_TableAccountAges.GetTableName(),
			{
				{ s_TableAccountAges.COL_ACCOUNT_ID, info.m_SteamID },
				{ s_TableAccountAges.COL_CREATION_TIME, info.m_CreationTime },
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

		auto query = SelectStatementBuilder(s_TableAccountAges.GetTableName())
			.Where(s_TableAccountAges.COL_ACCOUNT_ID == info.m_SteamID)
			.Run(db);

		if (query.executeStep())
		{
			info.m_CreationTime = query.getColumn(s_TableAccountAges.COL_CREATION_TIME);
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

			mh::fmtarg("col_AccountID", s_TableAccountAges.COL_ACCOUNT_ID.m_Name),
			mh::fmtarg("col_CreationTime", s_TableAccountAges.COL_CREATION_TIME.m_Name),
			mh::fmtarg("tbl_AccountAges", s_TableAccountAges.GetTableName()));

		Statement2 query(SQLite::Statement(const_cast<SQLite::Database&>(m_Connection.value()), queryStr));
		query.bind("$steamID", id.GetAccountID());

		const auto DeserializeAccountInfo = [&]()
		{
			AccountAgeInfo info;
			info.m_SteamID = query.getColumn(s_TableAccountAges.COL_ACCOUNT_ID);
			info.m_CreationTime = query.getColumn(s_TableAccountAges.COL_CREATION_TIME);
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

	void TempDB::Store(const LogsTFCacheInfo& info) try
	{
		ReplaceInto(m_Connection.value(), s_TableLogsTFCache.GetTableName(),
			{
				{ s_TableLogsTFCache.COL_ACCOUNT_ID, info.GetSteamID() },
				{ s_TableLogsTFCache.COL_LAST_UPDATE_TIME, info.m_LastCacheUpdateTime },
				{ s_TableLogsTFCache.COL_LOG_COUNT, info.m_LogsCount }
			});
	}
	catch (...)
	{
		LogException();
		throw;
	}

	bool TempDB::TryGet(LogsTFCacheInfo& info) const
	{
		auto query = SelectStatementBuilder(s_TableLogsTFCache.GetTableName())
			.Where(s_TableLogsTFCache.COL_ACCOUNT_ID == info.m_ID)
			.Run(m_Connection.value());

		if (query.executeStep())
		{
			info.m_LastCacheUpdateTime = query.getColumn(s_TableLogsTFCache.COL_LAST_UPDATE_TIME);
			info.m_LogsCount = query.getColumn(s_TableLogsTFCache.COL_LOG_COUNT);
			return true;
		}

		return false;
	}

	void TempDB::Store(const AccountInventorySizeInfo& info) try
	{
		ReplaceInto(m_Connection.value(), s_TableInventorySize.GetTableName(),
			{
				{ s_TableInventorySize.COL_ACCOUNT_ID, info.GetSteamID() },
				{ s_TableInventorySize.COL_LAST_UPDATE_TIME, info.m_LastCacheUpdateTime },
				{ s_TableInventorySize.COL_ITEM_COUNT, info.m_Items },
				{ s_TableInventorySize.COL_SLOT_COUNT, info.m_Slots },
			});
	}
	catch (...)
	{
		LogException();
		throw;
	}

	bool TempDB::TryGet(AccountInventorySizeInfo& info) const
	{
		auto query = SelectStatementBuilder(s_TableInventorySize.GetTableName())
			.Where(s_TableInventorySize.COL_ACCOUNT_ID == info.GetSteamID())
			.Run(m_Connection.value());

		if (query.executeStep())
		{
			info.m_LastCacheUpdateTime = query.getColumn(s_TableInventorySize.COL_LAST_UPDATE_TIME);
			info.m_Items = query.getColumn(s_TableInventorySize.COL_ITEM_COUNT);
			info.m_Slots = query.getColumn(s_TableInventorySize.COL_SLOT_COUNT);
			return true;
		}

		return false;
	}
}

std::unique_ptr<ITempDB> tf2_bot_detector::DB::ITempDB::Create()
{
	return std::make_unique<TempDB>();
}
