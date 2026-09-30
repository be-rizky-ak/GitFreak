#pragma once

#include <filesystem>
#include <optional>

class NativeDialogs
{
public:
    static std::optional<std::filesystem::path>
    pickFolder();
};