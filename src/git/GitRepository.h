#pragma once

#include "GitProcess.h"
#include "../models/Branch.h"
#include "../models/Commit.h"
#include "../models/Diff.h"
#include "../models/RepositoryStatus.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

class GitRepository
{
public:
    explicit GitRepository(
        std::filesystem::path workingDirectory);

    const std::filesystem::path& workingDirectory() const;

    bool isValid() const;

    std::optional<std::filesystem::path>
    findRoot() const;

    RepositoryStatus status() const;

    std::vector<Commit> history(
        int limit = 100) const;
    
    void stage(
        const std::filesystem::path& path) const;

    void unstage(
        const std::filesystem::path& path) const;

    void commit(
        const std::string& message) const;
    
    Diff diff(
        const std::filesystem::path& path,
        bool staged) const;

    std::vector<Branch> branches() const;

    void checkout(const std::string& branchName) const;

    GitCommandResult revParse(
        const std::string& argument) const;

private:
    std::filesystem::path m_workingDirectory;
    GitProcess m_gitProcess;
};