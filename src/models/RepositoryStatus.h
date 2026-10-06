#pragma once

#include <filesystem>
#include <string>
#include <vector>

enum class FileStatus
{
    Modified,
    Added,
    Deleted,
    Renamed,
    Copied,
    Untracked,
    Conflicted
};

struct ChangedFile
{
    std::filesystem::path path;

    FileStatus status = FileStatus::Modified;

    bool staged = false;
    bool unstaged = false;

    std::filesystem::path originalPath;
};

struct RepositoryStatus
{
    std::string branch;
    std::string head;

    std::string upstream;

    int ahead = 0;
    int behind = 0;

    bool detached = false;

    std::vector<ChangedFile> files;
};