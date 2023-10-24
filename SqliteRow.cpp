#include "SqliteRow.hpp"

#include "SqliteDataSourceException.hpp"

namespace finlytics::model {

void SqliteRow::add(const std::string& column_name, const std::optional<std::string>& cell_content)
{
    cells_[column_name] = cell_content;
}

template<>
size_t SqliteRow::get(const std::string& column_name) const
{
    if (!cells_.at(column_name).has_value()) {
        throw exception::SqliteDataSourceException("Requested value is null");
    }
    return std::stoul(cells_.at(column_name).value());
}

template<>
std::optional<size_t> SqliteRow::get(const std::string& column_name) const
{
    if (!cells_.at(column_name).has_value()) {
        return std::nullopt;
    }
    return std::stoul(cells_.at(column_name).value());
}

template<>
std::optional<double> SqliteRow::get(const std::string& column_name) const
{
    if (!cells_.at(column_name).has_value()) {
        return std::nullopt;
    }
    return std::stod(cells_.at(column_name).value());
}

template<>
std::optional<std::string> SqliteRow::get(const std::string& column_name) const
{
    return cells_.at(column_name);
}

template<>
std::string SqliteRow::get(const std::string& column_name) const
{
    if (!cells_.at(column_name).has_value()) {
        throw exception::SqliteDataSourceException("Requested value is null");
    }

    return cells_.at(column_name).value();
}

template<>
bool SqliteRow::get(const std::string& column_name) const
{
    if (!cells_.at(column_name).has_value()) {
        throw exception::SqliteDataSourceException("Requested value is null");
    }

    return cells_.at(column_name).value() == "1";
}

template<>
std::optional<int> SqliteRow::get(const std::string& column_name) const
{
    if (!cells_.at(column_name).has_value()) {
        return std::nullopt;
    }

    return std::stoi(cells_.at(column_name).value());
}

}// namespace finlytics::model