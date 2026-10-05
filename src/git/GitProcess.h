#pragma once

#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>

using GitOutputCallback =
    std::function<void(
        bool isError,
        const std::string& text)>;

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

    int executeStreaming(
        const std::filesystem::path& workingDirectory,
        std::span<const std::string> arguments,
        const GitOutputCallback& outputCallback) const;

    int cloneStreaming(
        const std::string& url,
        const std::filesystem::path& destination,
        const GitOutputCallback& outputCallback) const;

private:
    static std::wstring utf8ToWide(std::string_view text);

    static std::wstring buildCommandLine(
        std::span<const std::string> arguments);

    static std::string readPipe(void* pipe);
};