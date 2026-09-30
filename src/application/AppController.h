#pragma once

#include "MainWindow.h"

#include <filesystem>
#include <future>
#include <memory>
#include <optional>

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

    slint::ComponentHandle<MainWindow> m_window;

    TaskScheduler& m_scheduler;

    std::unique_ptr<GitRepository> m_repository;

    std::future<std::optional<std::filesystem::path>>
        m_openRepositoryTask;

    slint::Timer m_repositoryTimer;
};