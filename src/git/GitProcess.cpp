#include "GitProcess.h"

#include <windows.h>

#include <array>
#include <stdexcept>
#include <thread>
#include <vector>

GitCommandResult GitProcess::execute(
    const std::filesystem::path& workingDirectory,
    std::span<const std::string> arguments) const
{
    SECURITY_ATTRIBUTES securityAttributes{};
    securityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    securityAttributes.bInheritHandle = TRUE;

    HANDLE stdoutRead = nullptr;
    HANDLE stdoutWrite = nullptr;

    HANDLE stderrRead = nullptr;
    HANDLE stderrWrite = nullptr;

    if (!CreatePipe(
            &stdoutRead,
            &stdoutWrite,
            &securityAttributes,
            0))
    {
        throw std::runtime_error(
            "Failed to create stdout pipe.");
    }

    if (!SetHandleInformation(
            stdoutRead,
            HANDLE_FLAG_INHERIT,
            0))
    {
        CloseHandle(stdoutRead);
        CloseHandle(stdoutWrite);

        throw std::runtime_error(
            "Failed to configure stdout pipe.");
    }

    if (!CreatePipe(
            &stderrRead,
            &stderrWrite,
            &securityAttributes,
            0))
    {
        CloseHandle(stdoutRead);
        CloseHandle(stdoutWrite);

        throw std::runtime_error(
            "Failed to create stderr pipe.");
    }

    if (!SetHandleInformation(
            stderrRead,
            HANDLE_FLAG_INHERIT,
            0))
    {
        CloseHandle(stdoutRead);
        CloseHandle(stdoutWrite);
        CloseHandle(stderrRead);
        CloseHandle(stderrWrite);

        throw std::runtime_error(
            "Failed to configure stderr pipe.");
    }

    // Prevent Git from waiting for terminal input.
    HANDLE stdinHandle = CreateFileW(
        L"NUL",
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        &securityAttributes,
        OPEN_EXISTING,
        0,
        nullptr);

    if (stdinHandle == INVALID_HANDLE_VALUE)
    {
        CloseHandle(stdoutRead);
        CloseHandle(stdoutWrite);
        CloseHandle(stderrRead);
        CloseHandle(stderrWrite);

        throw std::runtime_error(
            "Failed to open NUL for stdin.");
    }

    std::wstring commandLine =
        buildCommandLine(arguments);

    std::vector<wchar_t> commandLineBuffer(
        commandLine.begin(),
        commandLine.end());

    commandLineBuffer.push_back(L'\0');

    std::wstring workingDirectoryWide =
        workingDirectory.wstring();

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(STARTUPINFOW);

    startupInfo.dwFlags |= STARTF_USESTDHANDLES;
    startupInfo.hStdInput = stdinHandle;
    startupInfo.hStdOutput = stdoutWrite;
    startupInfo.hStdError = stderrWrite;

    PROCESS_INFORMATION processInfo{};

    BOOL created = CreateProcessW(
        nullptr,
        commandLineBuffer.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        workingDirectoryWide.c_str(),
        &startupInfo,
        &processInfo);

    CloseHandle(stdinHandle);
    CloseHandle(stdoutWrite);
    CloseHandle(stderrWrite);

    if (!created)
    {
        DWORD errorCode = GetLastError();

        CloseHandle(stdoutRead);
        CloseHandle(stderrRead);

        throw std::runtime_error(
            "Failed to start git.exe. Windows error: " +
            std::to_string(errorCode));
    }

    std::string stdoutText;
    std::string stderrText;

    // Read both streams concurrently to prevent
    // pipe-buffer deadlocks.
    std::thread stdoutThread(
        [&]()
        {
            stdoutText = readPipe(stdoutRead);
        });

    std::thread stderrThread(
        [&]()
        {
            stderrText = readPipe(stderrRead);
        });

    WaitForSingleObject(
        processInfo.hProcess,
        INFINITE);

    DWORD exitCode = 0;

    GetExitCodeProcess(
        processInfo.hProcess,
        &exitCode);

    stdoutThread.join();
    stderrThread.join();

    CloseHandle(stdoutRead);
    CloseHandle(stderrRead);

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);

    GitCommandResult result;

    result.exitCode = static_cast<int>(exitCode);
    result.stdoutText = std::move(stdoutText);
    result.stderrText = std::move(stderrText);

    return result;
}

int GitProcess::executeStreaming(
    const std::filesystem::path& workingDirectory,
    std::span<const std::string> arguments,
    const GitOutputCallback& outputCallback) const
{
    HANDLE stdoutRead = nullptr;
    HANDLE stdoutWrite = nullptr;

    HANDLE stderrRead = nullptr;
    HANDLE stderrWrite = nullptr;

    SECURITY_ATTRIBUTES securityAttributes{};
    securityAttributes.nLength =
        sizeof(SECURITY_ATTRIBUTES);

    securityAttributes.bInheritHandle = TRUE;

    if (!CreatePipe(
            &stdoutRead,
            &stdoutWrite,
            &securityAttributes,
            0))
    {
        throw std::runtime_error(
            "CreatePipe stdout failed.");
    }

    if (!SetHandleInformation(
            stdoutRead,
            HANDLE_FLAG_INHERIT,
            0))
    {
        CloseHandle(stdoutRead);
        CloseHandle(stdoutWrite);

        throw std::runtime_error(
            "SetHandleInformation stdout failed.");
    }

    if (!CreatePipe(
            &stderrRead,
            &stderrWrite,
            &securityAttributes,
            0))
    {
        CloseHandle(stdoutRead);
        CloseHandle(stdoutWrite);

        throw std::runtime_error(
            "CreatePipe stderr failed.");
    }

    if (!SetHandleInformation(
            stderrRead,
            HANDLE_FLAG_INHERIT,
            0))
    {
        CloseHandle(stdoutRead);
        CloseHandle(stdoutWrite);
        CloseHandle(stderrRead);
        CloseHandle(stderrWrite);

        throw std::runtime_error(
            "SetHandleInformation stderr failed.");
    }

    HANDLE stdinHandle =
        CreateFileW(
            L"NUL",
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr);

    if (stdinHandle == INVALID_HANDLE_VALUE)
    {
        CloseHandle(stdoutRead);
        CloseHandle(stdoutWrite);
        CloseHandle(stderrRead);
        CloseHandle(stderrWrite);

        throw std::runtime_error(
            "Failed to open NUL for stdin.");
    }

    std::wstring commandLine =
        buildCommandLine(arguments);

    std::wstring directory =
        workingDirectory.wstring();

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(STARTUPINFOW);

    startupInfo.dwFlags |= STARTF_USESTDHANDLES;
    startupInfo.hStdInput = stdinHandle;
    startupInfo.hStdOutput = stdoutWrite;
    startupInfo.hStdError = stderrWrite;

    PROCESS_INFORMATION processInfo{};

    BOOL created =
        CreateProcessW(
            nullptr,
            commandLine.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr,
            directory.c_str(),
            &startupInfo,
            &processInfo);

    CloseHandle(stdinHandle);
    CloseHandle(stdoutWrite);
    CloseHandle(stderrWrite);

    if (!created)
    {
        CloseHandle(stdoutRead);
        CloseHandle(stderrRead);

        throw std::runtime_error(
            "CreateProcessW failed.");
    }

    auto readOutput =
        [&](HANDLE pipe, bool isError)
        {
            char buffer[4096];

            DWORD bytesRead = 0;

            while (ReadFile(
                       pipe,
                       buffer,
                       sizeof(buffer) - 1,
                       &bytesRead,
                       nullptr) &&
                   bytesRead > 0)
            {
                buffer[bytesRead] = '\0';

                if (outputCallback)
                {
                    outputCallback(
                        isError,
                        std::string(
                            buffer,
                            bytesRead));
                }
            }

            CloseHandle(pipe);
        };

    std::thread stdoutThread(
        readOutput,
        stdoutRead,
        false);

    std::thread stderrThread(
        readOutput,
        stderrRead,
        true);

    WaitForSingleObject(
        processInfo.hProcess,
        INFINITE);

    DWORD exitCode = 0;

    GetExitCodeProcess(
        processInfo.hProcess,
        &exitCode);

    stdoutThread.join();
    stderrThread.join();

    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);

    return static_cast<int>(exitCode);
}

std::wstring GitProcess::utf8ToWide(
    std::string_view text)
{
    if (text.empty())
    {
        return {};
    }

    int requiredSize = MultiByteToWideChar(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0);

    if (requiredSize <= 0)
    {
        throw std::runtime_error(
            "Failed to convert UTF-8 to UTF-16.");
    }

    std::wstring result(
        requiredSize,
        L'\0');

    MultiByteToWideChar(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        result.data(),
        requiredSize);

    return result;
}

std::wstring GitProcess::buildCommandLine(
    std::span<const std::string> arguments)
{
    std::wstring commandLine = L"git.exe";

    for (const std::string& argument : arguments)
    {
        std::wstring wideArgument =
            utf8ToWide(argument);

        commandLine += L" \"";

        for (wchar_t character : wideArgument)
        {
            if (character == L'"')
            {
                commandLine += L'\\';
                commandLine += L'"';
            }
            else
            {
                commandLine += character;
            }
        }

        commandLine += L'"';
    }

    return commandLine;
}

std::string GitProcess::readPipe(void* pipe)
{
    HANDLE handle =
        static_cast<HANDLE>(pipe);

    std::string result;

    char buffer[8192];

    DWORD bytesRead = 0;

    while (true)
    {
        BOOL success = ReadFile(
            handle,
            buffer,
            sizeof(buffer),
            &bytesRead,
            nullptr);

        if (!success || bytesRead == 0)
        {
            break;
        }

        result.append(
            buffer,
            bytesRead);
    }

    return result;
}

int GitProcess::cloneStreaming(
    const std::string& url,
    const std::filesystem::path& destination,
    const GitOutputCallback& outputCallback) const
{
    const std::array<std::string, 3> arguments =
    {
        "clone",
        url,
        destination.filename().string()
    };

    return executeStreaming(
        destination.parent_path(),
        arguments,
        outputCallback);
}