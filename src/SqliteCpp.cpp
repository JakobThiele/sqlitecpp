#include "SqliteCpp.hpp"

#include <iostream>

#include "../sqlite/sqlite3.h"//todo: fix once the other sqlite thingy is gone :D

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

    // Enable foreign key constraints
    char* errmsg;
    rc = sqlite3_exec(database_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", errmsg);
        sqlite3_free(errmsg);
        sqlite3_close(database_);
        throw exception::SqliteException("Could not enable foreign key constraints");
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
    try {
        beginTransaction();

        if (!tableExists(MIGRATIONS_TABLE)) {
            createMigrationsTable();
        }

        for (const auto& migration : migrations) {
            runMigration(migration);
        }

        commit();
    } catch (exception::SqliteException& e) {
        rollback();
        throw e;
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

std::vector<SqliteRow> SqliteCpp::selectFromTableWhere(
    const std::string&                       table,
    const std::vector<std::string>&          columns,
    const std::map<std::string, SqliteData>& where_clauses) const
{
    std::string query = "SELECT ";
    for (const auto& column : columns) {
        query += column + ", ";
    }
    query.erase(query.size() - 2);
    query += " FROM " + table;

    if (!where_clauses.empty()) {
        query += " WHERE ";

        for (const auto& where_clause : where_clauses) {
            query += where_clause.first + " = ? AND ";
        }
        query.erase(query.size() - 5);
    }

    sqlite3_stmt* statement;
    auto          preparation_result = sqlite3_prepare_v2(database_, query.c_str(), -1, &statement, nullptr);

    int param_index = 1;
    for (const auto& [column, data] : where_clauses) {
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
    std::vector<SqliteRow> rows;

    // Execute the statement and process the results
    while (sqlite3_step(statement) == SQLITE_ROW) {
        SqliteRow row;

        // Iterate over each column in the result row
        for (int i = 0; i < sqlite3_column_count(statement); ++i) {
            std::optional<std::string> cell_content;
            auto                       column_name = std::string(sqlite3_column_name(statement, i));

            if (sqlite3_column_type(statement, i) != SQLITE_NULL) {
                cell_content = std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement, i)));
            }

            row.add(column_name, cell_content);
        }

        rows.emplace_back(std::move(row));
    }

    // Finalize the statement to avoid resource leaks
    sqlite3_finalize(statement);

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
        throw exception::SqliteException("Error upserting data, Error Code: " + std::to_string(result));
    }
}

void SqliteCpp::deleteFrom(const std::string& table, const std::map<std::string, SqliteData>& where_clauses)
{
    if (where_clauses.empty()) {
        throw exception::SqliteException("Cannot delete without where clauses");
    }

    std::string query = "DELETE FROM " + table + " WHERE ";
    for (const auto& where_clause : where_clauses) {
        query += where_clause.first + " = ? AND ";
    }

    query.erase(query.size() - 5);

    sqlite3_stmt* statement;
    auto          preparation_result = sqlite3_prepare_v2(database_, query.c_str(), -1, &statement, nullptr);

    int param_index = 1;
    for (const auto& [column, data] : where_clauses) {
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

    int result = sqlite3_step(statement);
    sqlite3_finalize(statement);

    if (result != SQLITE_DONE) {
        throw exception::SqliteException("Error deleting data");
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
    for (const auto& row : rows) {
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

void SqliteCpp::beginTransaction()
{
    char* errMsg = nullptr;
    if (sqlite3_exec(database_, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string errorStr = errMsg;
        sqlite3_free(errMsg);
        throw exception::SqliteException("Failed to begin transaction: " + errorStr);
    }
}

void SqliteCpp::rollback()
{
    char* errMsg = nullptr;
    if (sqlite3_exec(database_, "ROLLBACK;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string errorStr = errMsg;
        sqlite3_free(errMsg);
        throw exception::SqliteException("Failed to begin transaction: " + errorStr);
    }
}

void SqliteCpp::commit()
{
    char* errMsg = nullptr;
    if (sqlite3_exec(database_, "COMMIT;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string errorStr = errMsg;
        sqlite3_free(errMsg);
        throw exception::SqliteException("Failed to begin transaction: " + errorStr);
    }
}

}// namespace sqlitecpp