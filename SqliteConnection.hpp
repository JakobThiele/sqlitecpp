#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "SqliteRow.hpp"

class sqlite3;

namespace sqlitecpp {

class SqliteConnection
{
public:
    SqliteConnection(const std::string& db_path);
    virtual ~SqliteConnection();

    std::vector<SqliteRow> selectStarFromTable(const std::string& table) const;

private:
    sqlite3* database_ = nullptr;
};

}// namespace finlytics::model
