#pragma once

#include "../models/Commit.h"

#include <string_view>
#include <vector>

class GitHistoryParser
{
public:
    static std::vector<Commit> parse(
        std::string_view output);
};