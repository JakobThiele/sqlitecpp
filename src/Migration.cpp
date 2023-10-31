#include "Migration.hpp"

#include <utility>

namespace sqlitecpp {

Migration::Migration(std::string title, std::string migration) : title_(std::move(title)), migration_(std::move(migration))
{
}

const std::string& Migration::getTitle() const
{
    return title_;
}

const std::string& Migration::getMigration() const
{
    return migration_;
}

}// namespace sqlitecpp