#pragma once

#include "../models/Remote.h"

#include <string_view>
#include <vector>

class GitRemoteParser
{
public:
    static std::vector<Remote> parse(
        std::string_view output);
};