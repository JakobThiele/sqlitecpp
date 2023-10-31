#include "SqliteCpp.hpp"

#include "sqlite3.h"

#include "SqliteException.hpp"
#include "SqliteRow.hpp"

namespace sqlitecpp {

SqliteCpp SqliteCpp::createOrOpenDatabase(const std::filesystem::path& db_path)
{
    return SqliteCpp(db_path);
}

SqliteCpp SqliteCpp::openDatabase(const std::filesystem::path& db_path)
{
    if (!std::filesystem::exists(db_path)) {
        throw exception::SqliteException("Database file does not exist");
    }
    return SqliteCpp(db_path);
}

SqliteCpp::SqliteCpp(const std::filesystem::path& db_path)
{
    int rc;

    rc = sqlite3_open(db_path.c_str(), &database_);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(database_));
        sqlite3_close(database_);
        throw exception::SqliteException("Could not open database");
    }
}

SqliteCpp::SqliteCpp(SqliteCpp&& other) noexcept : database_(other.database_)
{
    other.database_ = nullptr;
}

SqliteCpp& SqliteCpp::operator=(SqliteCpp&& other) noexcept
{
    if (this != &other) {                 // 1. Self-assignment check
        sqlite3_close(database_);         // 2. Close current database if it's open
        database_       = other.database_;// 3. Acquire ownership of the source's database handle
        other.database_ = nullptr;        // 4. Ensure the source gives up ownership
    }
    return *this;
}

SqliteCpp::~SqliteCpp()
{
    if (database_) {
        sqlite3_close_v2(database_);
    }
}

void SqliteCpp::runMigrations(const std::vector<Migration>& migrations)
{
    if (!tableExists(MIGRATIONS_TABLE)) {
        createMigrationsTable();
    }

    for (const auto& migration : migrations) {
        runMigration(migration);
    }
}

std::vector<SqliteRow> SqliteCpp::selectStarFromTable(const std::string& table) const
{
    std::string query = "SELECT * FROM " + table;

    std::vector<SqliteRow> rows;

    char* error_message;

    auto response_code = sqlite3_exec(
        database_,
        query.c_str(),
        +[](void* rows_void_ptr, int argc, char** argv, char** column_names) {
            auto rows_ptr = static_cast<std::vector<SqliteRow>*>(rows_void_ptr);

            SqliteRow row;
            for (size_t i = 0; i < argc; ++i) {
                const auto                 column_name = std::string(column_names[i]);
                std::optional<std::string> cell_content;

                if (argv[i] != nullptr) {
                    cell_content = std::string(argv[i]);
                }

                row.add(column_name, cell_content);
            }
            rows_ptr->emplace_back(row);

            return 0;
        },
        &rows,
        &error_message);

    if (response_code != SQLITE_OK) {
        std::string exception_message = "Error querying database: ";
        exception_message += error_message;
        throw exception::SqliteException(exception_message);
    }

    if (error_message) {
        sqlite3_free(error_message);
    }

    return rows;
}

void SqliteCpp::upsert(const std::string& table, const std::map<std::string, SqliteData>& column_to_data)
{
    if (column_to_data.empty()) {
        throw exception::SqliteException("Cannot upsert empty data");
    }

    std::string query  = "INSERT OR REPLACE INTO " + table + " (";
    std::string values = "VALUES (";

    for (const auto& [column, data] : column_to_data) {
        query += (column + ", ");
        values += "?, ";
    }

    query.erase(query.size() - 2);
    values.erase(values.size() - 2);

    query += ") " + values + ")";

    sqlite3_stmt* statement;
    auto          preparation_result = sqlite3_prepare_v2(database_, query.c_str(), -1, &statement, nullptr);

    int param_index = 1;
    for (const auto& [column, data] : column_to_data) {
        if (std::holds_alternative<int>(data)) {
            sqlite3_bind_int(statement, param_index, std::get<int>(data));
            ++param_index;
            continue;
        }

        if (std::holds_alternative<std::string>(data)) {
            sqlite3_bind_text(statement, param_index, std::get<std::string>(data).c_str(), -1, SQLITE_STATIC);
            ++param_index;
            continue;
        }

        if (std::holds_alternative<nullptr_t>(data)) {
            sqlite3_bind_null(statement, param_index);
            ++param_index;
            continue;
        }

        throw exception::SqliteException("Invalid data type");
    }

    //    const char* expandedSql = sqlite3_expanded_sql(statement);
    //    if (expandedSql) {
    //        std::string success = expandedSql;
    //        sqlite3_free((void*)expandedSql); // Free the allocated memory
    //    } else {
    //        std::string error = "Could not retrieve expanded SQL.";
    //    }

    int result = sqlite3_step(statement);
    sqlite3_finalize(statement);

    if (result != SQLITE_DONE) {
        throw exception::SqliteException("Error upserting data");
    }
}

bool SqliteCpp::tableExists(const std::string& tableName) const
{
    std::string   sql = "SELECT name FROM sqlite_master WHERE type='table' AND name=?;";
    sqlite3_stmt* stmt;

    // Prepare the SQL statement
    int rc = sqlite3_prepare_v2(database_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw exception::SqliteException("Failed to prepare statement: " + std::string(sqlite3_errmsg(database_)));
    }

    // Bind the table name parameter
    sqlite3_bind_text(stmt, 1, tableName.c_str(), -1, SQLITE_STATIC);

    // Execute the query
    rc          = sqlite3_step(stmt);
    bool exists = (rc == SQLITE_ROW);// true if a row is returned, false otherwise

    // Clean up
    sqlite3_finalize(stmt);
    return exists;
}

void SqliteCpp::createMigrationsTable()
{
    const std::string query = "CREATE TABLE IF NOT EXISTS " + MIGRATIONS_TABLE + R"( (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                title TEXT NOT NULL UNIQUE,
                executed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL
            );
        )";

    char* error_message = nullptr;
    int   result        = sqlite3_exec(database_, query.c_str(), nullptr, nullptr, &error_message);

    if (result != SQLITE_OK) {
        std::string errorStr(error_message);
        sqlite3_free(error_message);// Don't forget to free the error message
        throw exception::SqliteException("Failed to create migrations table: " + errorStr);
    }
}

void SqliteCpp::runMigration(const Migration& migration)
{
    const auto& rows = selectStarFromTable(MIGRATIONS_TABLE);
    for (const auto& row: rows) {
        if (row.get<std::string>("title") == migration.getTitle()) {
            return;
        }
    }

    char*              errorMessage           = nullptr;
    const std::string& migrations_table_query = "INSERT INTO " + MIGRATIONS_TABLE + " (title) VALUES ('" + migration.getTitle() + "');";
    int                result                 = sqlite3_exec(database_, migrations_table_query.c_str(), nullptr, nullptr, &errorMessage);

    if (result != SQLITE_OK) {
        std::string errorStr(errorMessage);
        sqlite3_free(errorMessage);
        throw exception::SqliteException("SQL execution failed: " + errorStr);
    }

    result = sqlite3_exec(database_, migration.getMigration().c_str(), nullptr, nullptr, &errorMessage);

    if (result != SQLITE_OK) {
        std::string errorStr(errorMessage);
        sqlite3_free(errorMessage);
        throw exception::SqliteException("SQL execution failed: " + errorStr);
    }
}

}// namespace sqlitecpp