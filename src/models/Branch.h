#pragma once

#include <string>

struct Branch
{
    std::string name;
    std::string hash;

    bool current = false;
    bool remote = false;
};