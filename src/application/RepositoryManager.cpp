#include "RepositoryManager.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>

namespace
{

    bool pathsEqual(const std::filesystem::path& a, const std::filesystem::path& b)
    {
        return a.lexically_normal() == b.lexically_normal();
    }

} // namespace

RepositoryManager::RepositoryManager() {}

void RepositoryManager::load()
{
    m_repositories.clear();

    const std::filesystem::path filePath = storagePath();

    std::ifstream file(filePath);

    if (!file.is_open())
        return;

    std::string line;

    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        std::filesystem::path path(line);

        if (path.empty())
            continue;

        m_repositories.push_back(RepositoryEntry{path, repositoryName(path)});
    }
}

void RepositoryManager::save()
{
    const std::filesystem::path filePath = storagePath();

    std::error_code error;

    std::filesystem::create_directories(filePath.parent_path(), error);

    if (error)
        return;

    std::ofstream file(filePath, std::ios::trunc);

    if (!file.is_open())
        return;

    for (const RepositoryEntry& repository : m_repositories)
    {
        file << repository.path.string() << '\n';
    }
}

void RepositoryManager::add(const std::filesystem::path& path)
{
    if (path.empty())
        return;

    const std::filesystem::path normalized = path.lexically_normal();

    auto iterator = std::find_if(m_repositories.begin(), m_repositories.end(),
        [&normalized](const RepositoryEntry& repository)
    { return pathsEqual(repository.path, normalized); });

    if (iterator != m_repositories.end())
    {
        RepositoryEntry entry = std::move(*iterator);

        m_repositories.erase(iterator);

        m_repositories.insert(m_repositories.begin(), std::move(entry));
    }
    else
    {
        m_repositories.insert(
            m_repositories.begin(), RepositoryEntry{normalized, repositoryName(normalized)});
    }

    save();
}

void RepositoryManager::remove(const std::filesystem::path& path)
{
    m_repositories.erase(std::remove_if(m_repositories.begin(), m_repositories.end(),
                             [&path](const RepositoryEntry& repository)
    { return pathsEqual(repository.path, path); }),
        m_repositories.end());

    save();
}

const std::vector<RepositoryEntry>& RepositoryManager::repositories() const
{
    return m_repositories;
}

std::optional<std::filesystem::path> RepositoryManager::lastOpened() const
{
    if (m_repositories.empty())
        return std::nullopt;

    return m_repositories.front().path;
}

std::filesystem::path RepositoryManager::storagePath() const
{
    const char* appData = std::getenv("APPDATA");

    if (!appData || appData[0] == '\0')
    {
        return std::filesystem::path("GitFreak") / "repositories.txt";
    }

    return std::filesystem::path(appData) / "GitFreak" / "repositories.txt";
}

std::string RepositoryManager::repositoryName(const std::filesystem::path& path)
{
    if (!path.filename().empty())
        return path.filename().string();

    return path.string();
}