#pragma once

#include <filesystem>
#include <functional>
#include <memory>

class FileWatcher
{
public:
    using Callback = std::function<void(const std::filesystem::path&)>;

    FileWatcher();
    ~FileWatcher();

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    void start(const std::filesystem::path& directory, Callback callback);

    void stop();

private:
    struct Impl;

    std::unique_ptr<Impl> m_impl;
};