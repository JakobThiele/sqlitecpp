#include "SqliteConnection.hpp"

#include <numeric>

#include "sqlite3.h"

#include "SqliteException.hpp"
#include "SqliteRow.hpp"

namespace sqlitecpp {

SqliteConnection::SqliteConnection(const std::string& db_path)
{
    int rc;

    rc = sqlite3_open(db_path.c_str(), &database_);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(database_));
        sqlite3_close(database_);
    }
}

SqliteConnection::~SqliteConnection()
{
    if (database_) {
        sqlite3_close_v2(database_);
    }
}

std::vector<SqliteRow> SqliteConnection::selectStarFromTable(const std::string& table) const
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

void SqliteConnection::upsert(const std::string& table, const std::map<std::string, SqliteData>& column_to_data)
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

    query.erase(query.size() -2);
    values.erase(values.size() -2);

    query += ") " + values + ")";

    sqlite3_stmt* statement;
    auto preparation_result = sqlite3_prepare_v2(database_, query.c_str(), -1, &statement, nullptr);

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

}// namespace sqlitecpp