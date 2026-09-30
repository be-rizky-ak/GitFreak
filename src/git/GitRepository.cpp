#include "GitRepository.h"

#include "GitStatusParser.h"

#include <array>
#include <stdexcept>

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

RepositoryStatus GitRepository::status() const
{
    const std::array<std::string, 5> arguments =
    {
        "status",
        "--porcelain=v2",
        "-z",
        "--branch",
        "--untracked-files=all"
    };

    GitCommandResult result =
        m_gitProcess.execute(
            m_workingDirectory,
            arguments);

    if (result.exitCode != 0)
    {
        throw std::runtime_error(
            "git status failed: " +
            result.stderrText);
    }

    return GitStatusParser::parse(
        result.stdoutText);
}

void GitRepository::stage(
    const std::filesystem::path& path) const
{
    const std::array<std::string, 3> arguments =
    {
        "add",
        "--",
        path.string()
    };

    GitCommandResult result =
        m_gitProcess.execute(
            m_workingDirectory,
            arguments);

    if (result.exitCode != 0)
    {
        throw std::runtime_error(
            "git add failed: " +
            result.stderrText);
    }
}

void GitRepository::unstage(
    const std::filesystem::path& path) const
{
    const std::array<std::string, 4> arguments =
    {
        "restore",
        "--staged",
        "--",
        path.string()
    };

    GitCommandResult result =
        m_gitProcess.execute(
            m_workingDirectory,
            arguments);

    if (result.exitCode != 0)
    {
        throw std::runtime_error(
            "git restore --staged failed: " +
            result.stderrText);
    }
}

void GitRepository::commit(
    const std::string& message) const
{
    const std::array<std::string, 3> arguments =
    {
        "commit",
        "-m",
        message
    };

    GitCommandResult result =
        m_gitProcess.execute(
            m_workingDirectory,
            arguments);

    if (result.exitCode != 0)
    {
        throw std::runtime_error(
            "git commit failed: " +
            result.stderrText);
    }
}