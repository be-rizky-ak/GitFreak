#include "FileWatcher.h"

#define NOMINMAX
#include <Windows.h>

#include <atomic>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{

    constexpr DWORD BufferSize = 64 * 1024;

    std::filesystem::path makeRelativePath(const std::wstring& fileName)
    {
        return std::filesystem::path(fileName);
    }

    bool isGitPath(const std::filesystem::path& path)
    {
        auto iterator = path.begin();

        if (iterator == path.end())
        {
            return false;
        }

        return *iterator == ".git";
    }

} // namespace

struct FileWatcher::Impl
{
    std::atomic<bool> running = false;

    HANDLE directoryHandle = INVALID_HANDLE_VALUE;

    std::thread workerThread;

    FileWatcher::Callback callback;

    std::filesystem::path directory;

    HANDLE stopEvent = nullptr;

    void run()
    {
        std::vector<std::byte> buffer(BufferSize);

        while (running)
        {
            OVERLAPPED overlapped{};

            overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

            if (!overlapped.hEvent)
            {
                break;
            }

            DWORD bytesReturned = 0;

            const BOOL result = ReadDirectoryChangesW(directoryHandle, buffer.data(),
                static_cast<DWORD>(buffer.size()), TRUE,
                FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                    FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE |
                    FILE_NOTIFY_CHANGE_CREATION,
                &bytesReturned, &overlapped, nullptr);

            if (!result)
            {
                CloseHandle(overlapped.hEvent);

                break;
            }

            HANDLE waitHandles[] = {overlapped.hEvent, stopEvent};

            const DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);

            if (waitResult == WAIT_OBJECT_0 + 1)
            {
                CancelIoEx(directoryHandle, &overlapped);

                CloseHandle(overlapped.hEvent);

                break;
            }

            if (waitResult != WAIT_OBJECT_0)
            {
                CloseHandle(overlapped.hEvent);

                break;
            }

            DWORD transferred = 0;

            if (!GetOverlappedResult(directoryHandle, &overlapped, &transferred, FALSE))
            {
                CloseHandle(overlapped.hEvent);

                continue;
            }

            CloseHandle(overlapped.hEvent);

            if (!running)
            {
                break;
            }

            if (transferred == 0)
            {
                continue;
            }

            std::size_t offset = 0;

            while (offset < transferred)
            {
                const auto* notification =
                    reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(buffer.data() + offset);

                const std::wstring fileName(
                    notification->FileName, notification->FileNameLength / sizeof(wchar_t));

                const std::filesystem::path relativePath(fileName);

                if (!isGitPath(relativePath))
                {
                    if (callback)
                    {
                        callback(relativePath);
                    }
                }

                if (notification->NextEntryOffset == 0)
                {
                    break;
                }

                offset += notification->NextEntryOffset;
            }
        }
    }
};

FileWatcher::FileWatcher() : m_impl(std::make_unique<Impl>()) {}

FileWatcher::~FileWatcher()
{
    stop();
}

void FileWatcher::start(const std::filesystem::path& directory, Callback callback)
{
    stop();

    if (directory.empty())
    {
        throw std::invalid_argument("FileWatcher directory is empty.");
    }

    const std::wstring directoryPath = directory.wstring();

    HANDLE handle = CreateFileW(directoryPath.c_str(), FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);

    if (handle == INVALID_HANDLE_VALUE)
    {
        throw std::runtime_error("Failed to open directory for "
                                 "file watching.");
    }

    HANDLE stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    if (!stopEvent)
    {
        CloseHandle(handle);

        throw std::runtime_error("Failed to create file watcher "
                                 "stop event.");
    }

    m_impl->directory = directory;

    m_impl->callback = std::move(callback);

    m_impl->directoryHandle = handle;

    m_impl->stopEvent = stopEvent;

    m_impl->running = true;

    m_impl->workerThread = std::thread([impl = m_impl.get()]() { impl->run(); });
}

void FileWatcher::stop()
{
    if (!m_impl)
    {
        return;
    }

    m_impl->running = false;

    if (m_impl->stopEvent)
    {
        SetEvent(m_impl->stopEvent);
    }

    if (m_impl->workerThread.joinable())
    {
        m_impl->workerThread.join();
    }

    if (m_impl->directoryHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_impl->directoryHandle);

        m_impl->directoryHandle = INVALID_HANDLE_VALUE;
    }

    if (m_impl->stopEvent)
    {
        CloseHandle(m_impl->stopEvent);

        m_impl->stopEvent = nullptr;
    }

    m_impl->callback = nullptr;
}