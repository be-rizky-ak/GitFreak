#pragma once

#include <slint.h>

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class TaskScheduler
{
public:
    explicit TaskScheduler(
        std::size_t workerCount = 2);

    ~TaskScheduler();

    TaskScheduler(const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;

    template<typename Function>
    auto submit(Function&& function)
        -> std::future<std::invoke_result_t<Function>>;
    
    template<typename Function, typename Callback>
    void submitWithCallback(
        Function&& function,
        Callback&& callback);

    void shutdown();

private:
    void workerLoop();

    std::mutex m_mutex;
    std::condition_variable m_condition;

    std::queue<std::function<void()>> m_tasks;

    std::vector<std::thread> m_workers;

    bool m_stopping = false;
};

template<typename Function>
auto TaskScheduler::submit(Function&& function)
    -> std::future<std::invoke_result_t<Function>>
{
    using ReturnType =
        std::invoke_result_t<Function>;

    auto task =
        std::make_shared<std::packaged_task<ReturnType()>>(
            std::forward<Function>(function));

    std::future<ReturnType> result =
        task->get_future();

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_stopping)
        {
            throw std::runtime_error(
                "TaskScheduler is shutting down.");
        }

        m_tasks.emplace(
            [task]()
            {
                (*task)();
            });
    }

    m_condition.notify_one();

    return result;
}

template<typename Function, typename Callback>
void TaskScheduler::submitWithCallback(
    Function&& function,
    Callback&& callback)
{
    using ResultType =
        std::invoke_result_t<Function>;

    submit(
        [function = std::forward<Function>(function),
         callback = std::forward<Callback>(callback)]() mutable
        {
            ResultType result = function();

            slint::invoke_from_event_loop(
                [result = std::move(result),
                 callback = std::move(callback)]() mutable
                {
                    callback(std::move(result));
                });
        });
}