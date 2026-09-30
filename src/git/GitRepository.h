#pragma once

#include "GitProcess.h"
#include "../models/RepositoryStatus.h"

#include <filesystem>
#include <optional>
#include <string>

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

    GitCommandResult revParse(
        const std::string& argument) const;

private:
    std::filesystem::path m_workingDirectory;
    GitProcess m_gitProcess;
};