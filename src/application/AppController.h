#pragma once

#include "MainWindow.h"

class TaskScheduler;

class AppController
{
public:
    AppController(
        const slint::ComponentHandle<MainWindow>& window,
        TaskScheduler& scheduler);

private:
    void connectSignals();

    slint::ComponentHandle<MainWindow> m_window;

    TaskScheduler& m_scheduler;
};