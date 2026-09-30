#include "Application.h"

#include "AppController.h"

#include "../tasks/TaskScheduler.h"

Application::Application()
    : m_window(MainWindow::create())
    , m_scheduler(std::make_unique<TaskScheduler>(2))
    , m_controller(
        std::make_unique<AppController>(
            m_window,
            *m_scheduler))
{
}

Application::~Application() = default;

void Application::run()
{
    m_window->run();
}