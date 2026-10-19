#pragma once

#include <stdexcept>
#include <string>
#include <utility>

class GitCommandError : public std::runtime_error
{
public:
    GitCommandError(std::string command, int exitCode, std::string stderrText)
        : std::runtime_error(buildMessage(command, exitCode, stderrText)),
          m_command(std::move(command)), m_exitCode(exitCode), m_stderrText(std::move(stderrText))
    {
    }

    const std::string& command() const noexcept { return m_command; }
    int exitCode() const noexcept { return m_exitCode; }
    const std::string& stderrText() const noexcept { return m_stderrText; }

private:
    static std::string buildMessage(
        const std::string& command, int exitCode, const std::string& stderrText)
    {
        std::string message = command + " failed (exit code " + std::to_string(exitCode) + ")";

        if (!stderrText.empty())
            message += ":\n" + stderrText;

        return message;
    }

    std::string m_command;
    int m_exitCode;
    std::string m_stderrText;
};
