#pragma once

#include "../models/RepositoryStatus.h"

#include <string_view>

class GitStatusParser
{
public:
    static RepositoryStatus parse(std::string_view output);
};