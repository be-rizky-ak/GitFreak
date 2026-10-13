#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct RepositoryEntry
{
    std::filesystem::path path;
    std::string name;
};

class RepositoryManager
{
public:
    RepositoryManager();

    void load();
    void save();

    void add(const std::filesystem::path& path);
    void remove(const std::filesystem::path& path);

    const std::vector<RepositoryEntry>& repositories() const;

    std::optional<std::filesystem::path> lastOpened() const;

private:
    std::filesystem::path storagePath() const;
    static std::string repositoryName(const std::filesystem::path& path);

private:
    std::vector<RepositoryEntry> m_repositories;
};