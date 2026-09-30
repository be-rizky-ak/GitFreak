#pragma once

#include <string>
#include <vector>

struct Commit
{
    std::string hash;
    std::vector<std::string> parents;

    std::string author;
    std::string email;
    std::string date;

    std::string subject;
};