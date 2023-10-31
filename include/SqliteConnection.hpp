#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <variant>

#include "SqliteRow.hpp"

class sqlite3;

using SqliteData = std::variant<std::string, int, std::nullptr_t>;

namespace sqlitecpp {

class SqliteConnection
{
public:
    SqliteConnection(const std::string& db_path);
    virtual ~SqliteConnection();

    std::vector<SqliteRow> selectStarFromTable(const std::string& table) const;
    void upsert(const std::string& table, const std::map<std::string, SqliteData>& column_to_data);

private:
    sqlite3* database_ = nullptr;
};

}// namespace finlytics::model
