#pragma once

#include <optional>
#include <string>
#include <unordered_map>

namespace sqlitecpp {

class SqliteRow
{
public:
    void add(const std::string& column_name, const std::optional<std::string>& cell_content);

    template<typename T>
    T get(const std::string& column_name) const
    {
        throw "todo: not supported";
    }

    template<>
    size_t get(const std::string& column_name) const;

    template<>
    std::optional<size_t> get(const std::string& column_name) const;

    template<>
    std::optional<double> get(const std::string& column_name) const;

    template<>
    std::optional<std::string> get(const std::string& column_name) const;

    template<>
    std::string get(const std::string& column_name) const;

    template<>
    bool get(const std::string& column_name) const;

    template<>
    std::optional<int> get(const std::string& column_name) const;

private:
    std::unordered_map<std::string, std::optional<std::string>> cells_;

    void guardColumnExists(const std::string& column_name) const;
};

}// namespace finlytics::model