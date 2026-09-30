#include "AppController.h"

#include "../git/GitRepository.h"
#include "../platform/NativeDialogs.h"
#include "../tasks/TaskScheduler.h"

#include <slint.h>

#include <chrono>
#include <exception>
#include <memory>
#include <string>
#include <vector>

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

    m_window->on_stage_file(
        [this](int index)
        {
            stageFile(index);
        });

    m_window->on_unstage_file(
        [this](int index)
        {
            unstageFile(index);
        });

    m_window->on_commit(
        [this](const slint::SharedString& message)
        {
            commit(
                std::string(message));
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

    m_repositoryTimer.stop();
    m_repository.reset();
    m_fileOrder.clear();

    m_window->set_changed_files({});
    m_window->set_staged_file_count(0);
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
            pollRepositoryOpen();
        });
}

void AppController::pollRepositoryOpen()
{
    if (!m_openRepositoryTask.valid())
    {
        m_repositoryTimer.stop();
        return;
    }

    if (m_openRepositoryTask.wait_for(
            std::chrono::milliseconds(0)) !=
        std::future_status::ready)
    {
        return;
    }

    m_repositoryTimer.stop();

    try
    {
        auto root = m_openRepositoryTask.get();

        if (!root)
        {
            m_repository.reset();

            m_window->set_repository_path("");
            m_window->set_changed_files({});
            m_window->set_status_text(
                "Selected folder is not a Git repository.");

            return;
        }

        m_repository =
            std::make_unique<GitRepository>(*root);

        m_window->set_repository_path(
            slint::SharedString(root->string()));

        m_window->set_status_text(
            "Loading working tree...");

        startStatusRefresh();
    }
    catch (const std::exception& e)
    {
        m_repository.reset();

        m_window->set_changed_files({});
        m_window->set_status_text(
            slint::SharedString(
                std::string("Failed to open repository: ") +
                e.what()));
    }
}

void AppController::pollStatus()
{
    if (!m_statusTask.valid())
    {
        m_repositoryTimer.stop();
        return;
    }

    if (m_statusTask.wait_for(
            std::chrono::milliseconds(0)) !=
        std::future_status::ready)
    {
        return;
    }

    m_repositoryTimer.stop();

    try
    {
        RepositoryStatus status =
            m_statusTask.get();

        std::vector<ChangedFile> orderedFiles;

        orderedFiles.reserve(
            status.files.size());


        // Keep existing files in their previous UI order.
        for (const auto& previousPath : m_fileOrder)
        {
            for (const ChangedFile& file : status.files)
            {
                if (file.path == previousPath)
                {
                    orderedFiles.push_back(file);
                    break;
                }
            }
        }


        // Append newly appearing files.
        for (const ChangedFile& file : status.files)
        {
            bool alreadyExists = false;

            for (const ChangedFile& existing : orderedFiles)
            {
                if (existing.path == file.path)
                {
                    alreadyExists = true;
                    break;
                }
            }

            if (!alreadyExists)
            {
                orderedFiles.push_back(file);
            }
        }

        m_fileOrder.clear();
        m_fileOrder.reserve(
            orderedFiles.size());

        for (const ChangedFile& file : orderedFiles)
        {
            m_fileOrder.push_back(file.path);
        }

        status.files =
            std::move(orderedFiles);

        m_repositoryStatus =
            status;

        std::vector<slint::SharedString> files;
        std::vector<bool> staged;
        std::vector<bool> unstaged;

        files.reserve(status.files.size());
        staged.reserve(status.files.size());
        unstaged.reserve(status.files.size());

        int stagedFileCount = 0;

        for (const ChangedFile& file :
             status.files)
        {
            std::string prefix;

            switch (file.status)
            {
            case FileStatus::Modified:
                prefix = "M";
                break;

            case FileStatus::Added:
                prefix = "A";
                break;

            case FileStatus::Deleted:
                prefix = "D";
                break;

            case FileStatus::Renamed:
                prefix = "R";
                break;

            case FileStatus::Copied:
                prefix = "C";
                break;

            case FileStatus::Untracked:
                prefix = "?";
                break;

            case FileStatus::Conflicted:
                prefix = "!";
                break;
            }

            files.emplace_back(
                prefix + "  " + file.path.string());

            staged.push_back(
                file.staged);

            unstaged.push_back(
                file.unstaged);
            
            if (file.staged)
            {
                ++stagedFileCount;
            }
        }

        m_window->set_changed_files(
            std::make_shared<
                slint::VectorModel<slint::SharedString>>(
                    std::move(files)));

        m_window->set_changed_files_staged(
            std::make_shared<
                slint::VectorModel<bool>>(
                    std::move(staged)));

        m_window->set_changed_files_unstaged(
            std::make_shared<
                slint::VectorModel<bool>>(
                    std::move(unstaged)));

        m_window->set_staged_file_count(
            stagedFileCount);

        if (status.branch.empty())
        {
            m_window->set_status_text(
                "Working tree loaded.");
        }
        else
        {
            m_window->set_status_text(
                slint::SharedString(
                    "Branch: " + status.branch));
        }
    }
    catch (const std::exception& e)
    {
        m_window->set_changed_files({});

        m_window->set_status_text(
            slint::SharedString(
                std::string("Failed to load status: ") +
                e.what()));
    }
}

void AppController::stageFile(int index)
{
    startFileOperation(
        index,
        true);
}

void AppController::unstageFile(int index)
{
    startFileOperation(
        index,
        false);
}

void AppController::startFileOperation(
    int index,
    bool stage)
{
    if (!m_repository)
    {
        return;
    }

    if (m_fileOperationTask.valid())
    {
        if (m_fileOperationTask.wait_for(
                std::chrono::milliseconds(0)) !=
            std::future_status::ready)
        {
            return;
        }

        m_fileOperationTask.get();
    }

    if (index < 0 ||
        static_cast<std::size_t>(index) >=
            m_repositoryStatus.files.size())
    {
        return;
    }

    const std::filesystem::path path =
        m_repositoryStatus.files[index].path;

    m_window->set_status_text(
        stage
            ? "Staging..."
            : "Unstaging...");

    const std::filesystem::path repositoryPath = m_repository->workingDirectory();

    m_fileOperationTask =
        m_scheduler.submit(
            [repositoryPath, path, stage]()
            {
                GitRepository repository(
                    repositoryPath);

                if (stage)
                {
                    repository.stage(path);
                }
                else
                {
                    repository.unstage(path);
                }
            });

    m_repositoryTimer.stop();

    m_repositoryTimer.start(
        slint::TimerMode::Repeated,
        std::chrono::milliseconds(50),
        [this]()
        {
            pollFileOperation();
        });
}

void AppController::pollFileOperation()
{
    if (!m_fileOperationTask.valid())
    {
        m_repositoryTimer.stop();
        return;
    }

    if (m_fileOperationTask.wait_for(
            std::chrono::milliseconds(0)) !=
        std::future_status::ready)
    {
        return;
    }

    m_repositoryTimer.stop();

    try
    {
        m_fileOperationTask.get();

        m_window->set_status_text(
            "Refreshing working tree...");

        startStatusRefresh();
    }
    catch (const std::exception& e)
    {
        m_window->set_status_text(
            slint::SharedString(
                std::string("Git operation failed: ") +
                e.what()));
    }
}

void AppController::startStatusRefresh()
{
    if (!m_repository)
    {
        return;
    }

    const std::filesystem::path repositoryPath =
        m_repository->workingDirectory();

    m_statusTask =
        m_scheduler.submit(
            [repositoryPath]()
            {
                GitRepository repository(
                    repositoryPath);

                return repository.status();
            });

    m_repositoryTimer.stop();

    m_repositoryTimer.start(
        slint::TimerMode::Repeated,
        std::chrono::milliseconds(50),
        [this]()
        {
            pollStatus();
        });
}

void AppController::commit(
    const std::string& message)
{
    if (!m_repository)
    {
        return;
    }

    if (message.empty())
    {
        return;
    }

    if (m_repositoryStatus.files.empty())
    {
        return;
    }

    int stagedFileCount = 0;

    for (const ChangedFile& file :
         m_repositoryStatus.files)
    {
        if (file.staged)
        {
            ++stagedFileCount;
        }
    }

    if (stagedFileCount == 0)
    {
        return;
    }

    const std::filesystem::path repositoryPath =
        m_repository->workingDirectory();

    m_window->set_status_text(
        "Committing...");

    m_commitTask =
        m_scheduler.submit(
            [repositoryPath, message]()
            {
                GitRepository repository(
                    repositoryPath);

                repository.commit(
                    message);
            });

    m_repositoryTimer.stop();

    m_repositoryTimer.start(
        slint::TimerMode::Repeated,
        std::chrono::milliseconds(50),
        [this]()
        {
            pollCommit();
        });
}

void AppController::pollCommit()
{
    if (!m_commitTask.valid())
    {
        m_repositoryTimer.stop();
        return;
    }

    if (m_commitTask.wait_for(
            std::chrono::milliseconds(0)) !=
        std::future_status::ready)
    {
        return;
    }

    m_repositoryTimer.stop();

    try
    {
        m_commitTask.get();

        m_window->set_status_text(
            "Commit created.");

        startStatusRefresh();
    }
    catch (const std::exception& e)
    {
        m_window->set_status_text(
            slint::SharedString(
                std::string("Commit failed: ") +
                e.what()));
    }
}