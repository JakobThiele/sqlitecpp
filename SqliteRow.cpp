#include "SqliteRow.hpp"

#include "SqliteException.hpp"

namespace sqlitecpp {

void SqliteRow::add(const std::string& column_name, const std::optional<std::string>& cell_content)
{
    cells_[column_name] = cell_content;
}

void SqliteRow::guardColumnExists(const std::string& column_name) const
{
    if (cells_.find(column_name) == cells_.end()) {
        throw exception::SqliteException("Column not found");
    }
}

template<>
size_t SqliteRow::get(const std::string& column_name) const
{
    guardColumnExists(column_name);

    if (!cells_.at(column_name).has_value()) {
        throw exception::SqliteException("Requested value is null");
    }
    return std::stoul(cells_.at(column_name).value());
}

template<>
std::optional<size_t> SqliteRow::get(const std::string& column_name) const
{
    guardColumnExists(column_name);

    if (!cells_.at(column_name).has_value()) {
        return std::nullopt;
    }
    return std::stoul(cells_.at(column_name).value());
}

template<>
std::optional<double> SqliteRow::get(const std::string& column_name) const
{
    guardColumnExists(column_name);

    if (!cells_.at(column_name).has_value()) {
        return std::nullopt;
    }
    return std::stod(cells_.at(column_name).value());
}

template<>
std::optional<std::string> SqliteRow::get(const std::string& column_name) const
{
    guardColumnExists(column_name);

    return cells_.at(column_name);
}

template<>
std::string SqliteRow::get(const std::string& column_name) const
{
    guardColumnExists(column_name);

    if (!cells_.at(column_name).has_value()) {
        throw exception::SqliteException("Requested value is null");
    }

    return cells_.at(column_name).value();
}

template<>
bool SqliteRow::get(const std::string& column_name) const
{
    guardColumnExists(column_name);

    if (!cells_.at(column_name).has_value()) {
        throw exception::SqliteException("Requested value is null");
    }

    return cells_.at(column_name).value() == "1";
}

template<>
std::optional<int> SqliteRow::get(const std::string& column_name) const
{
    guardColumnExists(column_name);

    if (!cells_.at(column_name).has_value()) {
        return std::nullopt;
    }

    return std::stoi(cells_.at(column_name).value());
}

}// namespace sqlitecpp