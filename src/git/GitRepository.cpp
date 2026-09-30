#include "GitRepository.h"

#include <array>

GitRepository::GitRepository(
    std::filesystem::path workingDirectory)
    : m_workingDirectory(std::move(workingDirectory))
{
}

const std::filesystem::path&
GitRepository::workingDirectory() const
{
    return m_workingDirectory;
}

bool GitRepository::isValid() const
{
    return findRoot().has_value();
}

std::optional<std::filesystem::path>
GitRepository::findRoot() const
{
    GitCommandResult result =
        revParse("--show-toplevel");

    if (result.exitCode != 0)
    {
        return std::nullopt;
    }

    std::string root = result.stdoutText;

    while (!root.empty() &&
           (root.back() == '\n' ||
            root.back() == '\r'))
    {
        root.pop_back();
    }

    if (root.empty())
    {
        return std::nullopt;
    }

    return std::filesystem::path(root);
}

GitCommandResult GitRepository::revParse(
    const std::string& argument) const
{
    const std::array<std::string, 2> arguments =
    {
        "rev-parse",
        argument
    };

    return m_gitProcess.execute(
        m_workingDirectory,
        arguments);
}