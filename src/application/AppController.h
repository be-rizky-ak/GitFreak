#pragma once

#include "MainWindow.h"

#include "../models/RepositoryStatus.h"

#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class GitRepository;
class TaskScheduler;

class AppController
{
public:
    AppController(
        const slint::ComponentHandle<MainWindow>& window,
        TaskScheduler& scheduler);

    ~AppController();

    AppController(const AppController&) = delete;
    AppController& operator=(const AppController&) = delete;

private:
    void connectSignals();
    void openRepository();
    void pollRepositoryOpen();
    void pollStatus();
    void stageFile(int index);
    void unstageFile(int index);
    void startFileOperation(
        int index,
        bool stage);
    void pollFileOperation();
    void startStatusRefresh();
    void commit(
        const std::string& message);
    void pollCommit();

    slint::ComponentHandle<MainWindow> m_window;

    TaskScheduler& m_scheduler;

    std::unique_ptr<GitRepository> m_repository;

    std::future<std::optional<std::filesystem::path>>
        m_openRepositoryTask;

    std::future<RepositoryStatus> m_statusTask;
    RepositoryStatus m_repositoryStatus;
    std::future<void> m_fileOperationTask;
    slint::Timer m_repositoryTimer;
    std::vector<std::filesystem::path> m_fileOrder;
    std::future<void> m_commitTask;
};