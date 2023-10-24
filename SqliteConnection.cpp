#include "SqliteConnection.hpp"

#include "SqliteDataSourceException.hpp"
#include "SqliteRow.hpp"
#include "db.h"

namespace finlytics::model {

SqliteConnection::SqliteConnection()
{
    if (sqlite3_open(":memory:", &database_) != SQLITE_OK) {
        throw exception::SqliteDataSourceException("Could not open database");
    }

    if (sqlite3_deserialize(database_, "main", finlytics_db, finlytics_db_len, finlytics_db_len, SQLITE_DESERIALIZE_RESIZEABLE) !=
        SQLITE_OK) {
        std::string error_message = "Failed to deserialize SQLite database: ";
        error_message += sqlite3_errmsg(database_);
        throw exception::SqliteDataSourceException(error_message);
    }
}

SqliteConnection::~SqliteConnection()
{
    if (database_) {
        sqlite3_close_v2(database_);
    }
}

std::vector<SqliteRow> SqliteConnection::select(const std::string& query)
{
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
        throw exception::SqliteDataSourceException(exception_message);
    }

    if (error_message) {
        sqlite3_free(error_message);
    }

    return rows;
}

}// namespace finlytics::model