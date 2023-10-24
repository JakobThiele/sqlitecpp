#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include <sqlite3.h>
#include "Model/DataRule/DataRule.hpp"
#include "SqliteRow.hpp"

namespace finlytics::model {

class SqliteConnection
{
public:
    SqliteConnection();
    virtual ~SqliteConnection();

    std::vector<SqliteRow> select(const std::string& query);

private:
    sqlite3* database_ = nullptr;
};

}// namespace finlytics::model
