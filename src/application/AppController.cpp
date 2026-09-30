#include "AppController.h"

#include "../git/GitRepository.h"
#include "../platform/NativeDialogs.h"
#include "../tasks/TaskScheduler.h"

#include <chrono>

AppController::AppController(
    const slint::ComponentHandle<MainWindow>& window,
    TaskScheduler& scheduler)
    : m_window(window)
    , m_scheduler(scheduler)
{
    connectSignals();
}

AppController::~AppController()
{
    m_repositoryTimer.stop();
}

void AppController::connectSignals()
{
    m_window->on_open_repository(
        [this]
        {
            openRepository();
        });
}

void AppController::openRepository()
{
    auto selectedPath =
        NativeDialogs::pickFolder();

    if (!selectedPath)
    {
        return;
    }

    m_window->set_status_text(
        "Opening repository...");

    m_window->set_repository_path(
        slint::SharedString(
            selectedPath->string()));

    m_openRepositoryTask =
        m_scheduler.submit(
            [path = *selectedPath]()
            -> std::optional<std::filesystem::path>
            {
                GitRepository repository(path);

                return repository.findRoot();
            });

    m_repositoryTimer.start(
        slint::TimerMode::Repeated,
        std::chrono::milliseconds(50),
        [this]()
        {
            if (!m_openRepositoryTask.valid())
            {
                m_repositoryTimer.stop();
                return;
            }

            auto status =
                m_openRepositoryTask.wait_for(
                    std::chrono::milliseconds(0));

            if (status != std::future_status::ready)
            {
                return;
            }

            auto root =
                m_openRepositoryTask.get();

            m_repositoryTimer.stop();

            if (!root)
            {
                m_repository.reset();

                m_window->set_status_text(
                    "Selected folder is not a Git repository.");

                return;
            }

            m_repository =
                std::make_unique<GitRepository>(
                    *root);

            RepositoryStatus RepoStatus =
                m_repository->status();

            std::cout
                << "Branch: "
                << RepoStatus.branch
                << '\n';

            std::cout
                << "Files: "
                << RepoStatus.files.size()
                << '\n';

            for (const ChangedFile& file :
                 RepoStatus.files)
            {
                std::cout
                    << "  "
                    << file.path.string()
                    << '\n';
            }

            m_window->set_repository_path(
                slint::SharedString(
                    root->string()));

            m_window->set_status_text(
                "Repository opened.");
        });
}