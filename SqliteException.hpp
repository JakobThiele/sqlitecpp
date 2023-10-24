#pragma once

#include <exception>
#include <string>

namespace sqlitecpp::exception {

class SqliteException : public std::invalid_argument
{
public:
    explicit SqliteException(const std::string& what) : std::invalid_argument(what)
    {
    }
};

}// namespace finlytics::model::exception