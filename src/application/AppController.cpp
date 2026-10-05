#include "AppController.h"

#include "../git/GitRepository.h"
#include "../platform/NativeDialogs.h"
#include "../tasks/TaskScheduler.h"

#include <slint.h>

#include <algorithm>
#include <chrono>
#include <exception>
#include <memory>
#include <string>
#include <vector>

namespace
{

std::vector<std::string> splitDiffLines(
    const std::string& text)
{
    std::vector<std::string> lines;

    std::size_t start = 0;

    while (start < text.size())
    {
        std::size_t end =
            text.find('\n', start);

        if (end == std::string::npos)
        {
            end = text.size();
        }

        std::string line =
            text.substr(start, end - start);

        if (!line.empty() &&
            line.back() == '\r')
        {
            line.pop_back();
        }

        lines.push_back(
            std::move(line));

        if (end == text.size())
        {
            break;
        }

        start = end + 1;
    }

    return lines;
}

int getDiffLineType(
    const std::string& line)
{
    if (line.rfind("diff --git", 0) == 0 ||
        line.rfind("index ", 0) == 0 ||
        line.rfind("--- ", 0) == 0 ||
        line.rfind("+++ ", 0) == 0)
    {
        return 3;
    }

    if (line.rfind("@@", 0) == 0)
    {
        return 4;
    }

    if (!line.empty() &&
        line[0] == '+')
    {
        return 1;
    }

    if (!line.empty() &&
        line[0] == '-')
    {
        return 2;
    }

    return 0;
}

}

AppController::AppController(
    const slint::ComponentHandle<MainWindow>& window,
    TaskScheduler& scheduler)
    : m_window(window)
    , m_scheduler(scheduler)
{
    m_operationLog = std::make_shared<OperationLog>();
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

    m_window->on_select_file(
        [this](int index)
        {
            selectFile(index);
        });

    m_window->on_checkout_branch(
        [this](int index)
        {
            checkoutBranch(index);
        });

    m_window->on_close_operation_dialog(
        [this]()
        {
            closeOperationDialog();
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

        startHistoryRefresh();
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

void AppController::startHistoryRefresh()
{
    if (!m_repository)
    {
        return;
    }

    const std::filesystem::path repositoryPath =
        m_repository->workingDirectory();

    m_historyTask =
        m_scheduler.submit(
            [repositoryPath]()
            {
                GitRepository repository(
                    repositoryPath);

                return repository.history(100);
            });

    m_repositoryTimer.stop();

    m_repositoryTimer.start(
        slint::TimerMode::Repeated,
        std::chrono::milliseconds(50),
        [this]()
        {
            pollHistory();
        });
}

void AppController::pollHistory()
{
    if (!m_historyTask.valid())
    {
        m_repositoryTimer.stop();
        return;
    }

    if (m_historyTask.wait_for(
            std::chrono::milliseconds(0)) !=
        std::future_status::ready)
    {
        return;
    }

    m_repositoryTimer.stop();

    try
    {
        std::vector<Commit> commits =
            m_historyTask.get();

        updateHistoryGraph(commits);

        std::vector<slint::SharedString>
            commitHashes;

        std::vector<slint::SharedString>
            commitDetails;

        commitHashes.reserve(
            commits.size());

        commitDetails.reserve(
            commits.size());

        for (const Commit& commit :
             commits)
        {
            std::string shortHash =
                commit.hash.substr(
                    0,
                    std::min<std::size_t>(
                        7,
                        commit.hash.size()));

            commitHashes.emplace_back(
                shortHash);

            commitDetails.emplace_back(
                commit.subject +
                "  —  " +
                commit.author +
                "  " +
                commit.date);
        }

        m_window->set_history_commits(
            std::make_shared<
                slint::VectorModel<
                    slint::SharedString>>(
                std::move(commitHashes)));

        m_window->set_history_details(
            std::make_shared<
                slint::VectorModel<
                    slint::SharedString>>(
                std::move(commitDetails)));

        m_window->set_history_count(
            static_cast<int>(
                commits.size()));
        
        startBranchRefresh();
    }
    catch (const std::exception& e)
    {
        m_window->set_status_text(
            slint::SharedString(
                std::string("Failed to load history: ") +
                e.what()));
    }
}

void AppController::selectFile(int index)
{
    if (!m_repository)
    {
        return;
    }

    if (index < 0 ||
        index >= static_cast<int>(
            m_repositoryStatus.files.size()))
    {
        return;
    }

    const ChangedFile& file =
        m_repositoryStatus.files[index];

    startDiffRefresh(
        file.path,
        file.staged);
}

void AppController::startDiffRefresh(
    const std::filesystem::path& path,
    bool staged)
{
    if (!m_repository)
    {
        return;
    }

    const std::filesystem::path repositoryPath =
        m_repository->workingDirectory();

    m_diffTask =
        m_scheduler.submit(
            [repositoryPath, path, staged]()
            {
                GitRepository repository(
                    repositoryPath);

                return repository.diff(
                    path,
                    staged);
            });

    m_repositoryTimer.stop();

    m_repositoryTimer.start(
        slint::TimerMode::Repeated,
        std::chrono::milliseconds(50),
        [this]()
        {
            pollDiff();
        });
}

void AppController::pollDiff()
{
    if (!m_diffTask.valid())
    {
        m_repositoryTimer.stop();
        return;
    }

    if (m_diffTask.wait_for(
            std::chrono::milliseconds(0)) !=
        std::future_status::ready)
    {
        return;
    }

    m_repositoryTimer.stop();

    try
    {
        Diff diff =
            m_diffTask.get();

        std::vector<std::string> lines =
            splitDiffLines(diff.text);

        std::vector<slint::SharedString>
            diffLines;

        std::vector<int>
            diffLineTypes;

        diffLines.reserve(lines.size());
        diffLineTypes.reserve(lines.size());

        for (const std::string& line : lines)
        {
            diffLines.emplace_back(line);
            diffLineTypes.push_back(
                getDiffLineType(line));
        }

        m_window->set_diff_lines(
            std::make_shared<
                slint::VectorModel<
                    slint::SharedString>>(
                std::move(diffLines)));

        m_window->set_diff_line_types(
            std::make_shared<
                slint::VectorModel<int>>(
                std::move(diffLineTypes)));
    }
    catch (const std::exception& e)
    {
        std::vector<slint::SharedString>
            errorLines;

        std::vector<int>
            errorTypes;

        errorLines.emplace_back(
            std::string("Failed to load diff: ") +
            e.what());

        errorTypes.push_back(2);

        m_window->set_diff_lines(
            std::make_shared<
                slint::VectorModel<
                    slint::SharedString>>(
                std::move(errorLines)));

        m_window->set_diff_line_types(
            std::make_shared<
                slint::VectorModel<int>>(
                std::move(errorTypes)));
    }
}

void AppController::startBranchRefresh()
{
    if (!m_repository)
    {
        return;
    }

    const std::filesystem::path repositoryPath =
        m_repository->workingDirectory();

    m_branchTask =
        m_scheduler.submit(
            [repositoryPath]()
            {
                GitRepository repository(
                    repositoryPath);

                return repository.branches();
            });

    m_repositoryTimer.stop();

    m_repositoryTimer.start(
        slint::TimerMode::Repeated,
        std::chrono::milliseconds(50),
        [this]()
        {
            pollBranches();
        });
}

void AppController::pollBranches()
{
    if (!m_branchTask.valid())
    {
        m_repositoryTimer.stop();
        return;
    }

    if (m_branchTask.wait_for(
            std::chrono::milliseconds(0)) !=
        std::future_status::ready)
    {
        return;
    }

    m_repositoryTimer.stop();

    try
    {
        std::vector<Branch> branches =
            m_branchTask.get();

        std::vector<slint::SharedString>
            branchNames;

        std::vector<bool>
            branchCurrent;

        branchNames.reserve(
            branches.size());

        branchCurrent.reserve(
            branches.size());

        for (const Branch& branch :
             branches)
        {
            branchNames.emplace_back(
                branch.name);

            branchCurrent.push_back(
                branch.current);
        }

        m_branches = branches;

        m_window->set_branch_names(
            std::make_shared<
                slint::VectorModel<
                    slint::SharedString>>(
                std::move(branchNames)));

        m_window->set_branch_current(
            std::make_shared<
                slint::VectorModel<bool>>(
                std::move(branchCurrent)));
    }
    catch (const std::exception& e)
    {
        m_window->set_status_text(
            slint::SharedString(
                std::string(
                    "Failed to load branches: ") +
                e.what()));
    }
}

void AppController::checkoutBranch(int index)
{
    if (!m_repository)
    {
        return;
    }

    if (m_checkoutTask.valid() &&
        m_checkoutTask.wait_for(
            std::chrono::milliseconds(0)) !=
        std::future_status::ready)
    {
        return;
    }

    if (index < 0 ||
        index >= static_cast<int>(
            m_branches.size()))
    {
        return;
    }

    const Branch& branch =
        m_branches[index];

    if (branch.current)
    {
        return;
    }

    startCheckout(branch.name);
}

void AppController::startCheckout(
    const std::string& branchName)
{
    if (!m_repository)
    {
        return;
    }

    const std::filesystem::path repositoryPath =
        m_repository->workingDirectory();

    startOperation(
        "Switch Branch",
        "git switch " + branchName);

    std::shared_ptr<OperationLog> operationLog =
        m_operationLog;

    m_checkoutTask =
        m_scheduler.submit(
            [repositoryPath,
             branchName,
             operationLog]()
            {
                GitRepository repository(
                    repositoryPath);

                repository.checkoutStreaming(
                    branchName,
                    [operationLog](
                        bool isError,
                        const std::string& text)
                    {
                        operationLog->append(
                            isError,
                            text);
                    });
            });

    m_repositoryTimer.stop();

    m_repositoryTimer.start(
        slint::TimerMode::Repeated,
        std::chrono::milliseconds(50),
        [this]()
        {
            pollCheckout();
        });
}

void AppController::pollCheckout()
{
    if (!m_checkoutTask.valid())
    {
        m_repositoryTimer.stop();
        return;
    }

    if (m_checkoutTask.wait_for(
            std::chrono::milliseconds(0)) !=
        std::future_status::ready)
    {
        return;
    }

    m_repositoryTimer.stop();

    try
    {
        m_checkoutTask.get();

        finishOperation(
            true,
            0);

        m_window->set_diff_lines(
            std::make_shared<
                slint::VectorModel<
                    slint::SharedString>>());

        m_window->set_diff_line_types(
            std::make_shared<
                slint::VectorModel<int>>());

        startStatusRefresh();
    }
    catch (const std::exception& e)
    {
        appendOperationLog(
            true,
            e.what());

        finishOperation(
            false,
            1);

        m_window->set_status_text(
            slint::SharedString(
                std::string(
                    "Failed to switch branch: ") +
                e.what()));
    }
}

void AppController::updateHistoryGraph(
    const std::vector<Commit>& commits)
{
    std::vector<CommitGraphNode> graph =
        CommitGraph::build(commits);

    std::vector<int> lanes;
    std::vector<int> parentLanes;
    std::vector<int> parentCounts;

    lanes.reserve(
        graph.size());

    parentLanes.reserve(
        graph.size());

    parentCounts.reserve(
        graph.size());

    for (const CommitGraphNode& node :
         graph)
    {
        lanes.push_back(
            node.lane);

        if (!node.parentLanes.empty())
        {
            parentLanes.push_back(
                node.parentLanes[0]);

            parentCounts.push_back(1);
        }
        else
        {
            parentLanes.push_back(
                node.lane);

            parentCounts.push_back(0);
        }
    }

    m_window->set_history_lanes(
        std::make_shared<
            slint::VectorModel<int>>(
                std::move(lanes)));

    m_window->set_history_parent_lanes(
        std::make_shared<
            slint::VectorModel<int>>(
                std::move(parentLanes)));

    m_window->set_history_parent_counts(
        std::make_shared<
            slint::VectorModel<int>>(
                std::move(parentCounts)));
}

void AppController::closeOperationDialog()
{
    if (m_operationState.status ==
        OperationStatus::Running)
    {
        return;
    }

    m_operationTimer.stop();
    m_window->set_operation_visible(false);
}

void AppController::pollOperationLog()
{
    if (!m_operationLog)
    {
        m_operationTimer.stop();
        return;
    }

    std::vector<std::string> entries =
        m_operationLog->consume();

    bool changed = false;

    for (const std::string& entry : entries)
    {
        m_operationPendingText += entry;

        std::size_t newlinePosition = 0;

        while ((newlinePosition =
                    m_operationPendingText.find('\n'))
               != std::string::npos)
        {
            std::string line =
                m_operationPendingText.substr(
                    0,
                    newlinePosition);

            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            m_operationLines.push_back(
                std::move(line));

            m_operationPendingText.erase(
                0,
                newlinePosition + 1);

            changed = true;
        }

        changed = true;
    }

    // Display an incomplete line while it is still arriving.
    std::vector<std::string> visibleLines =
        m_operationLines;

    if (!m_operationPendingText.empty())
    {
        visibleLines.push_back(
            m_operationPendingText);
    }

    if (changed)
    {
        std::vector<slint::SharedString> uiLines;
        uiLines.reserve(visibleLines.size());

        for (const std::string& line : visibleLines)
        {
            uiLines.emplace_back(line);
        }

        m_window->set_operation_log_lines(
            std::make_shared<
                slint::VectorModel<
                    slint::SharedString>>(
                std::move(uiLines)));
    }

    if (m_operationState.status != OperationStatus::Running &&
        m_operationPendingText.empty())
    {
        m_operationTimer.stop();
    }
}

void AppController::startOperation(
    const std::string& title,
    const std::string& command)
{
    m_operationLog =
        std::make_shared<OperationLog>();

    m_operationState =
        OperationState{};

    m_operationState.status =
        OperationStatus::Running;

    m_operationState.title =
        title;

    m_operationState.command =
        command;

    m_operationState.exitCode = -1;

    m_operationLines.clear();
    m_operationPendingText.clear();

    m_window->set_operation_title(
        slint::SharedString(title));

    m_window->set_operation_command(
        slint::SharedString(command));

    m_window->set_operation_status(
        slint::SharedString("Running..."));

    m_window->set_operation_running(true);
    m_window->set_operation_visible(true);

    m_window->set_operation_log_lines(
        std::make_shared<
            slint::VectorModel<
                slint::SharedString>>());

    m_operationTimer.stop();

    m_operationTimer.start(
        slint::TimerMode::Repeated,
        std::chrono::milliseconds(50),
        [this]()
        {
            pollOperationLog();
        });
}

void AppController::finishOperation(
    bool success,
    int exitCode)
{
    m_operationState.status =
        success
            ? OperationStatus::Success
            : OperationStatus::Failed;

    m_operationState.exitCode =
        exitCode;

    m_window->set_operation_running(false);

    m_window->set_operation_status(
        slint::SharedString(
            success
                ? "Completed"
                : "Failed"));
}

void AppController::appendOperationLog(
    bool isError,
    const std::string& text)
{
    if (!m_operationLog)
    {
        return;
    }

    m_operationLog->append(
        isError,
        text);
}