#include "TaskScheduler.h"

#include <stdexcept>

TaskScheduler::TaskScheduler(std::size_t workerCount)
{
    if (workerCount == 0)
    {
        throw std::invalid_argument("TaskScheduler requires at least one worker.");
    }

    m_workers.reserve(workerCount);

    for (std::size_t i = 0; i < workerCount; ++i)
    {
        m_workers.emplace_back([this] { workerLoop(); });
    }
}

TaskScheduler::~TaskScheduler()
{
    shutdown();
}

void TaskScheduler::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_stopping)
        {
            return;
        }

        m_stopping = true;
    }

    m_condition.notify_all();

    for (std::thread& worker : m_workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }

    m_workers.clear();
}

void TaskScheduler::workerLoop()
{
    while (true)
    {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(m_mutex);

            m_condition.wait(lock, [this] { return m_stopping || !m_tasks.empty(); });

            if (m_stopping && m_tasks.empty())
            {
                return;
            }

            task = std::move(m_tasks.front());
            m_tasks.pop();
        }

        task();
    }
}