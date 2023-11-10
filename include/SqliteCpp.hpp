#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "Migration.hpp"
#include "SqliteRow.hpp"

class sqlite3;

using SqliteData = std::variant<std::string, int, std::nullptr_t>;

namespace sqlitecpp {

class SqliteCpp
{
public:
    static SqliteCpp createOrOpenDatabase(const std::filesystem::path& db_path);
    static SqliteCpp openDatabase(const std::filesystem::path& db_path);

               SqliteCpp(SqliteCpp&& other) noexcept;
    SqliteCpp& operator=(SqliteCpp&& other) noexcept;
               SqliteCpp(const SqliteCpp&) = delete;
    SqliteCpp& operator=(const SqliteCpp&) = delete;

    virtual ~SqliteCpp();

    void runMigrations(const std::vector<Migration>& migrations);

    std::vector<SqliteRow> selectStarFromTable(const std::string& table) const;
    std::vector<SqliteRow> selectFromTableWhere(
        const std::string&                       table,
        const std::vector<std::string>&          columns       = { "*" },
        const std::map<std::string, SqliteData>& where_clauses = {}) const;

    void upsert(const std::string& table, const std::map<std::string, SqliteData>& column_to_data);
    void deleteFrom(const std::string& table, const std::map<std::string, SqliteData>& where_clauses);

private:
    const std::string MIGRATIONS_TABLE = "sqlitecpp_migrations";
    explicit          SqliteCpp(const std::filesystem::path& db_path);
    sqlite3*          database_ = nullptr;

    bool tableExists(const std::string& tableName) const;
    void createMigrationsTable();
    void runMigration(const Migration& migration);

    void beginTransaction();
    void rollback();
    void commit();
};

}// namespace sqlitecpp
