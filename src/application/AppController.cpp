#include "AppController.h"

#include "../tasks/TaskScheduler.h"

#include <chrono>
#include <iostream>
#include <thread>

AppController::AppController(
    const slint::ComponentHandle<MainWindow>& window,
    TaskScheduler& scheduler)
    : m_window(window)
    , m_scheduler(scheduler)
{
    connectSignals();
}

void AppController::connectSignals()
{
    m_window->on_open_repository([this]
    {
        m_scheduler.submit(
            []
            {
                std::cout
                    << "Task started on worker thread.\n";

                std::this_thread::sleep_for(
                    std::chrono::seconds(2));

                std::cout
                    << "Task finished on worker thread.\n";
            });

        std::cout
            << "UI callback finished immediately.\n";
    });
}