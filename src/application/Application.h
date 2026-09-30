#pragma once

#include "MainWindow.h"

#include <memory>

class AppController;
class TaskScheduler;

class Application
{
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void run();

private:
    slint::ComponentHandle<MainWindow> m_window;

    std::unique_ptr<TaskScheduler> m_scheduler;
    std::unique_ptr<AppController> m_controller;
};