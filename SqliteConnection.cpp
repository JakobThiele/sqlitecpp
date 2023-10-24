#include "SqliteConnection.hpp"

#include <numeric>

#include <sqlite3.h>

#include "SqliteException.hpp"
#include "SqliteRow.hpp"

namespace sqlitecpp {

SqliteConnection::SqliteConnection(const std::string& db_path)
{
    int      rc;

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

}// namespace finlytics::model