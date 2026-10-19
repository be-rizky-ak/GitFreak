#pragma once

#include "../Constants.h"
#include "../models/Commit.h"
#include "../models/RepositoryStatus.h"

#include <algorithm>
#include <filesystem>
#include <list>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace GitFreakCache
{
    inline std::string repositoryKey(const std::filesystem::path& path)
    {
        return path.lexically_normal().generic_string();
    }

    // Tracks recently used repositories and bounds retained cache entries.
    class RepositoryCache
    {
    public:
        explicit RepositoryCache(std::size_t maxRepositories = Config::MaxRepositoryCache)
            : m_maxRepositories(std::max<std::size_t>(1, maxRepositories))
        {
        }

        // Returns the repository evicted by this access, if any.
        std::optional<std::string> touch(const std::filesystem::path& path)
        {
            const std::string key = repositoryKey(path);
            m_recent.remove(key);
            m_recent.push_front(key);

            if (m_recent.size() <= m_maxRepositories)
                return std::nullopt;

            std::string evicted = std::move(m_recent.back());
            m_recent.pop_back();
            return evicted;
        }

        bool contains(const std::filesystem::path& path) const
        {
            const auto key = repositoryKey(path);
            return std::find(m_recent.begin(), m_recent.end(), key) != m_recent.end();
        }

        void clear() { m_recent.clear(); }

    private:
        std::size_t m_maxRepositories;
        std::list<std::string> m_recent;
    };

    class StatusCache
    {
    public:
        void put(const std::filesystem::path& path, RepositoryStatus status)
        {
            m_entries.insert_or_assign(repositoryKey(path), std::move(status));
        }

        std::optional<RepositoryStatus> get(const std::filesystem::path& path) const
        {
            const auto it = m_entries.find(repositoryKey(path));
            if (it == m_entries.end())
                return std::nullopt;

            return it->second;
        }

        void invalidate(const std::filesystem::path& path) { m_entries.erase(repositoryKey(path)); }

        void clear() { m_entries.clear(); }

    private:
        std::unordered_map<std::string, RepositoryStatus> m_entries;
    };

    class CommitCache
    {
    public:
        static constexpr std::size_t MaxCommitsPerRepository = 1000;
        static constexpr std::size_t MaxEntries = Config::MaxRepositoryCache;

        void put(const std::filesystem::path& path, int limit, std::vector<Commit> commits)
        {
            if (limit <= 0)
                return;

            if (commits.size() > MaxCommitsPerRepository)
                commits.resize(MaxCommitsPerRepository);

            const std::string key = makeKey(path, limit);

            m_entries.insert_or_assign(key, std::move(commits));
            m_recent.remove(key);
            m_recent.push_front(key);

            while (m_recent.size() > std::max<std::size_t>(1, MaxEntries))
            {
                m_entries.erase(m_recent.back());
                m_recent.pop_back();
            }
        }

        std::optional<std::vector<Commit>> get(const std::filesystem::path& path, int limit) const
        {
            const auto it = m_entries.find(makeKey(path, limit));
            if (it == m_entries.end())
                return std::nullopt;

            return it->second;
        }

        void invalidate(const std::filesystem::path& path)
        {
            const std::string prefix = repositoryKey(path) + '\n';

            for (auto it = m_entries.begin(); it != m_entries.end();)
            {
                if (it->first.rfind(prefix, 0) == 0)
                    it = m_entries.erase(it);
                else
                    ++it;
            }

            for (auto it = m_recent.begin(); it != m_recent.end();)
            {
                if (it->rfind(prefix, 0) == 0)
                    it = m_recent.erase(it);
                else
                    ++it;
            }
        }

        void clear()
        {
            m_entries.clear();
            m_recent.clear();
        }

    private:
        static std::string makeKey(const std::filesystem::path& path, int limit)
        {
            return repositoryKey(path) + '\n' + std::to_string(limit);
        }

        std::unordered_map<std::string, std::vector<Commit>> m_entries;
        std::list<std::string> m_recent;
    };
} // namespace GitFreakCache
