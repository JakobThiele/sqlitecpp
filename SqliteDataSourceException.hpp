#pragma once

#include <exception>
#include <string>

namespace finlytics::model::exception {

class SqliteDataSourceException : public std::invalid_argument
{
public:
    explicit SqliteDataSourceException(const std::string& what) : std::invalid_argument(what)
    {
    }
};

}// namespace finlytics::model::exception