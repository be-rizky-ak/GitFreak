#include "GitRepository.h"

#include "GitHistoryParser.h"
#include "GitStatusParser.h"

#include <array>
#include <stdexcept>
#include <utility>
#include <vector>

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

std::vector<Commit> GitRepository::history(
    int limit) const
{
    if (limit <= 0)
    {
        return {};
    }

    const std::string limitArgument =
        "-" + std::to_string(limit);

    const std::string format =
        "--pretty=format:%H%x00%P%x00%an%x00%ae%x00%ad%x00%s%x00";

    const std::array<std::string, 5> arguments =
    {
        "log",
        "HEAD",
        limitArgument,
        "--date=iso-strict",
        format
    };

    GitCommandResult result =
        m_gitProcess.execute(
            m_workingDirectory,
            arguments);

    if (result.exitCode != 0)
    {
        throw std::runtime_error(
            "git log failed: " +
            result.stderrText);
    }

    return GitHistoryParser::parse(
        result.stdoutText);
}

Diff GitRepository::diff(
    const std::filesystem::path& path,
    bool staged) const
{
    std::vector<std::string> arguments;

    arguments.push_back("diff");

    if (staged)
    {
        arguments.push_back("--cached");
    }

    arguments.push_back("--");
    arguments.push_back(path.string());

    GitCommandResult result =
        m_gitProcess.execute(
            m_workingDirectory,
            arguments);

    if (result.exitCode != 0)
    {
        throw std::runtime_error(
            "git diff failed: " +
            result.stderrText);
    }

    Diff diff;

    diff.text = std::move(result.stdoutText);

    return diff;
}

std::vector<Branch> GitRepository::branches() const
{
    const std::array<std::string, 3> arguments =
    {
        "for-each-ref",
        "--format=%(refname:short)%00%(objectname)%00",
        "refs/heads/"
    };

    GitCommandResult result =
        m_gitProcess.execute(
            m_workingDirectory,
            arguments);

    if (result.exitCode != 0)
    {
        throw std::runtime_error(
            "git for-each-ref failed: " +
            result.stderrText);
    }

    std::string currentBranch;

    {
        const std::array<std::string, 2> currentArguments =
        {
            "branch",
            "--show-current"
        };

        GitCommandResult currentResult =
            m_gitProcess.execute(
                m_workingDirectory,
                currentArguments);

        if (currentResult.exitCode != 0)
        {
            throw std::runtime_error(
                "git branch failed: " +
                currentResult.stderrText);
        }

        currentBranch =
            currentResult.stdoutText;

        while (!currentBranch.empty() &&
               (currentBranch.back() == '\r' ||
                currentBranch.back() == '\n'))
        {
            currentBranch.pop_back();
        }
    }

    auto trim = [](std::string s)
    {
        while (!s.empty() &&
               (s.back() == '\r' ||
                s.back() == '\n' ||
                s.back() == ' '))
        {
            s.pop_back();
        }

        std::size_t start = 0;

        while (start < s.size() &&
               (s[start] == '\r' ||
                s[start] == '\n' ||
                s[start] == ' '))
        {
            ++start;
        }

        return s.substr(start);
    };

    std::vector<Branch> branches;

    std::size_t position = 0;

    while (position < result.stdoutText.size())
    {
        std::size_t nameEnd =
            result.stdoutText.find(
                '\0',
                position);

        if (nameEnd == std::string::npos)
        {
            break;
        }

        std::size_t hashStart =
            nameEnd + 1;

        std::size_t hashEnd =
            result.stdoutText.find(
                '\0',
                hashStart);

        if (hashEnd == std::string::npos)
        {
            break;
        }

        Branch branch;

        branch.name = trim(result.stdoutText.substr(
            position,
            nameEnd - position));

        branch.hash = trim(result.stdoutText.substr(
            hashStart,
            hashEnd - hashStart));

        branch.current =
            branch.name == currentBranch;

        branch.remote = false;

        branches.push_back(
            std::move(branch));

        position = hashEnd + 1;
    }

    return branches;
}

void GitRepository::checkout(
    const std::string& branchName) const
{
    const std::array<std::string, 2> arguments =
    {
        "switch",
        branchName
    };

    GitCommandResult result =
        m_gitProcess.execute(
            m_workingDirectory,
            arguments);

    if (result.exitCode != 0)
    {
        throw std::runtime_error(
            "git switch failed: " +
            result.stderrText);
    }
}