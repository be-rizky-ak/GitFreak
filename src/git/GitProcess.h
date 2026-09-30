#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>

struct GitCommandResult
{
    int exitCode = -1;

    std::string stdoutText;
    std::string stderrText;
};

class GitProcess
{
public:
    GitCommandResult execute(
        const std::filesystem::path& workingDirectory,
        std::span<const std::string> arguments) const;

private:
    static std::wstring utf8ToWide(std::string_view text);

    static std::wstring buildCommandLine(
        std::span<const std::string> arguments);

    static std::string readPipe(void* pipe);
};