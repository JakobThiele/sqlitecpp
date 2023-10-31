#pragma once

#include <string>

namespace sqlitecpp {

class Migration
{
public:
    Migration(std::string title, std::string migration);

    const std::string& getTitle() const;
    const std::string& getMigration() const;

private:
    std::string title_;
    std::string migration_;
};

}// namespace sqlitecpp
